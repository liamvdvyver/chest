//============================================================================//
// Implementation of GenericEngine for the UCI protocol.
//============================================================================//

#include "uci.h"

#include <cstdlib>
#include <sstream>
#include <string>

#include "engine.h"
#include "libChest/eval.h"
#include "libChest/makemove.h"
#include "libChest/move.h"
#include "libChest/search.h"
#include "libChest/state.h"
#include "libChest/timemanagement.h"
#include "libChest/zobrist.h"

// Helper
void display_state(const GenericEngine &engine) {
    std::string state_str = engine.get_astate().state.pretty();
    state_str.append(engine.get_astate().state.to_fen());
    state_str.append("\n0x");
    state_str.append(Zobrist(engine.get_astate().state).pretty());
    state_str.append("\nStatic eval: ");
    state_str.append(
        std::to_string(eval::DefaultEval(engine.get_astate()).eval()));
    state_str.append("\nRepetitions: ");
    state_str.append(std::to_string(engine.get_node().n_repetitions()));
    std::stringstream out_stream{state_str};
    for (std::string ln; std::getline(out_stream, ln);) {
        ln.push_back('\n');
        engine.log(ln, LogLevel::ENGINE_INFO);
    }
}

//============================================================================//
// Options
//============================================================================//

//-- Spin --------------------------------------------------------------------//

std::string UCISpinOption::get_type_string() const {
    std::string ret = "type spin default ";
    ret.append(std::to_string(m_default_val));
    ret.append(" min ");
    ret.append(std::to_string(m_min_val));
    ret.append(" max ");
    ret.append(std::to_string(m_max_val));
    return ret;
}

bool UCISpinOption::parse(std::string_view opt_name, std::stringstream &value) {
    match_literal(opt_name, "value", value);

    std::optional<int> val{};
    std::string arg;
    while (value >> arg) {
        try {
            val = std::stoi(arg);
        } catch (std::invalid_argument &e) {
            bad_arg(opt_name, arg);
        }
    }
    if (!val.has_value() || val.value() < m_min_val ||
        val.value() > m_max_val) {
        bad_arg(opt_name, arg);
        return false;
    }

    m_set_val = val.value();
    return true;
}

//-- Hash --------------------------------------------------------------------//

std::optional<int> Hash::execute() {
    m_engine->get_ttable().resize_mb(m_set_val);
    return {};
}

//============================================================================//
// Commands
//============================================================================//

//-- UCICheck ----------------------------------------------------------------//

void UCICheck::identify() {
    std::string name_msg = "id name ";
    name_msg.append(name);
    name_msg.push_back('\n');
    m_engine->log(name_msg, LogLevel::RAW_MESSAGE);
    std::string author_msg = "id author ";
    author_msg.append(author);
    author_msg.push_back('\n');
    m_engine->log(author_msg, LogLevel::RAW_MESSAGE);
};

void UCICheck::tell_options() {
    for (const auto &[name, fct] : *m_options) {
        std::string msg = "option name ";
        msg.append(name);
        msg.push_back(' ');
        msg.append(fct()->get_type_string());
        msg.push_back('\n');
        m_engine->log(msg, LogLevel::RAW_MESSAGE);
    }
};

std::optional<int> UCICheck::execute() {
    identify();
    tell_options();
    m_engine->log("uciok\n", LogLevel::RAW_MESSAGE);
    return {};
};

//-- SetOption ---------------------------------------------------------------//

bool SetOption::parse(const std::string_view keyword, std::stringstream &args) {
    // Match "name"
    if (!match_literal(keyword, "name", args)) {
        return false;
    }

    // Get option
    std::string opt_name;
    if (!(args >> opt_name)) {
        return false;
    }
    if (!(m_options->contains(opt_name))) return false;
    m_opt = m_options->at(opt_name)();

    return m_opt.get()->parse(opt_name, args);
}

std::optional<int> SetOption::execute() { return m_opt.get()->execute(); };

//-- ISReady -----------------------------------------------------------------//

std::optional<int> ISReady::execute() {
    m_engine->log("readyok\n", LogLevel::RAW_MESSAGE);
    return {};
};

//-- DebugConfig -------------------------------------------------------------//

bool DebugConfig::parse(const std::string_view keyword,
                        std::stringstream &args) {
    (void)keyword;
    std::string arg;
    args >> arg;
    if (arg == "on") {
        m_engine->set_debug(true);
    } else if (arg == "off") {
        m_engine->set_debug(false);
    } else {
        return false;
    }
    return true;
};
std::optional<int> DebugConfig::execute() { return {}; };

//-- Ucinewgame --------------------------------------------------------------//

std::optional<int> UciNewGame::execute() {
    m_engine->get_ttable().clear();
    return {};
};

//-- Position ----------------------------------------------------------------//

void Position::moves_impl(const std::string_view keyword,
                          std::stringstream &args) {
    (void)keyword;
    std::string move;
    while (args >> move) {
        m_moves.push_back(move);
    }
};
void Position::startpos_impl(const std::string_view keyword,
                             std::stringstream &args) {
    (void)keyword;
    (void)args;
    m_astate = {{{state::new_game_fen}}};
};
void Position::fen_impl(const std::string_view keyword,
                        std::stringstream &args) {
    static constexpr size_t fen_len = 6;
    std::string fen_string;
    std::string fen_field;
    for (size_t i = 0; i < fen_len; i++) {
        if (!(args >> fen_field)) {
            return bad_arg(keyword, fen_string);
        }
        if (!fen_string.empty()) {
            fen_string.push_back(' ');
        }
        fen_string.append(fen_field);
    }
    try {
        m_astate = {{{fen_string}}};
    } catch (std::invalid_argument &e) {
        bad_arg(keyword, fen_string);
        std::string msg = e.what();
        msg.push_back('\n');
        m_engine->log(msg, LogLevel::ENGINE_WARN);
    }
};
void Position::curpos_impl(const std::string_view keyword,
                           std::stringstream &args) {
    (void)keyword;
    (void)args;
    m_astate = m_engine->get_astate();
};

bool Position::sufficient_args() const { return m_astate.has_value(); };
std::optional<int> Position::execute() {
    assert(m_astate.has_value());
    m_engine->set_astate(m_astate.value());

    for (const std::string &move : m_moves) {
        const std::optional<move::FatMove> fmove =
            move::LongAlgMove(move).to_fmove(m_engine->get_astate());
        if (!fmove.has_value()) {
            return false;
        }
        // TODO: batch updates in batches of MAX_LENGTH
        m_engine->get_node().prep_search(1);
        m_engine->get_node().make_move(fmove.value());
    }

    if (m_engine->is_debug()) {
        display_state(*m_engine);
    }

    return {};
}

//-- Quit --------------------------------------------------------------------//

std::optional<int> Quit::execute() { return {EXIT_SUCCESS}; };

//-- Go ----------------------------------------------------------------------//

void Go::parse_field(const std::string_view keyword, std::stringstream &args,
                     auto &field) {
    std::string arg;
    if (!(args >> arg)) {
        bad_arg(keyword, "");
    }
    uint64_t val{};
    try {
        val = std::stoi(arg);
    } catch (std::invalid_argument &e) {
        return bad_arg(keyword, arg);
    }
    field = val;
}

void Go::inc_impl(const std::string_view keyword, std::stringstream &args,
                  board::Colour to_move) {
    return parse_field(keyword, args, m_tc.increment(to_move));
}

void Go::time_impl(const std::string_view keyword, std::stringstream &args,
                   board::Colour to_move) {
    return parse_field(keyword, args, m_tc.remaining(to_move));
}

void Go::movestogo_impl(const std::string_view keyword,
                        std::stringstream &args) {
    return parse_field(keyword, args, m_tc.to_go);
}

void Go::movetime_impl(const std::string_view keyword,
                       std::stringstream &args) {
    return parse_field(keyword, args, m_tc.movetime);
}

void Go::perft_impl(const std::string_view keyword, std::stringstream &args) {
    m_type = SearchType::PERFT;
    return parse_field(keyword, args, m_depth);
}

void Go::ab_impl(const std::string_view keyword, std::stringstream &args) {
    (void)keyword;
    (void)args;
    m_type = SearchType::AB;
}

bool Go::sufficient_args() const {
    const board::Colour to_move = m_engine->get_astate().state.to_move;
    return m_tc.movetime || m_tc.copy_remaining(to_move) || m_depth;
}

std::optional<int> Go::execute() {
    if (m_trace && !m_engine->is_debug()) {
        m_engine->log("trace output requires debug on\n",
                      LogLevel::ENGINE_WARN);
    }
    return m_engine->is_debug() && m_trace ? execute_impl<true>()
                                           : execute_impl<false>();
}

template <bool Debug>
std::optional<int> Go::execute_impl() {
    using EvalTp = eval::DefaultEval;
    using DlSearcherTp =
        search::DLNegaMax<EvalTp, MAX_DEPTH, {.verbose = Debug}>;
    using IDSearcherTp = search::IDSearcher<DlSearcherTp, MAX_DEPTH>;

    switch (m_type) {
        case SearchType::ID:
            return search_impl<IDSearcherTp>();
        case SearchType::AB:
            return search_impl<DlSearcherTp>();
        case SearchType::PERFT:
            return perft_impl();
    }
}

std::optional<int> Go::perft_impl() {
    state::PerftNode<MAX_DEPTH> sn{m_engine->get_astate(), m_depth};
    const auto moves = sn.find_moves<false>();
    size_t perft = 0;

    for (auto m : moves) {
        if (sn.make_move(m)) {
            const auto mv_res = sn.perft().perft;
            perft += mv_res;

            // Need to_move to reset to convert castles to long algebraic
            // notation correctly
            sn.unmake_move();

            std::string partial_msg = move::LongAlgMove(m);
            partial_msg.append(": ");
            partial_msg.append(std::to_string(mv_res));
            partial_msg.push_back('\n');

            m_engine->log(partial_msg, LogLevel::ENGINE_INFO);
        } else {
            sn.unmake_move();
        }
    }

    std::string msg = "Result: ";
    msg.append(std::to_string(perft));
    msg.push_back('\n');
    m_engine->log(msg, LogLevel::ENGINE_INFO);
    return {};
};

template <search::DLSearcher TSearcher>
std::optional<int> Go::search_impl() {
    // Types used for searching.
    // Could template at engine level.

    using TimeManagerTp = search::DefaultTimeManager;

    // Calculate stop time
    // TODO: add buffer to account for parsing time
    // Maybe in parse(): record time message came in.
    const TimeManagerTp time_manager{};
    const search::ms_t search_time =
        time_manager(m_tc, m_engine->get_astate().state.to_move);

    const std::optional<std::chrono::time_point<std::chrono::steady_clock>>
        finish_time =
            m_tc.is_null()
                ? std::nullopt
                : std::optional(std::chrono::steady_clock::now() +
                                std::chrono::milliseconds(search_time));

    TSearcher searcher(m_engine->get_node(), m_engine->get_ttable());
    if (m_depth) {
        searcher.set_depth(m_depth);
    }

    move::FatMove best = searcher.search(finish_time, {}, m_engine).best_move;

    std::string msg = "bestmove ";
    msg.append(move::LongAlgMove(best));
    msg.push_back('\n');
    m_engine->log(msg, LogLevel::RAW_MESSAGE);
    return {};
};

//============================================================================//
// Main engine
//============================================================================//

UCIEngine::UCIEngine()
    : GenericEngine({
          {"uci", [this]() { return std::make_unique<UCICheck>(this); }},
          {"isready", [this]() { return std::make_unique<ISReady>(this); }},
          {"setoption", [this]() { return std::make_unique<SetOption>(this); }},
          {"debug", [this]() { return std::make_unique<DebugConfig>(this); }},
          {"ucinewgame",
           [this]() { return std::make_unique<UciNewGame>(this); }},
          {"position", [this]() { return std::make_unique<Position>(this); }},
          {"quit", [this]() { return std::make_unique<Quit>(this); }},
          {"go", [this]() { return std::make_unique<Go>(this); }},
      }) {}

void UCIEngine::log(const std::string_view &msg, const LogLevel level,
                    bool flush) const {
    std::string info_str = "info ";
    switch (level) {
        case LogLevel::RAW_MESSAGE:
            break;
        case LogLevel::PROTOCOL_INFO:
            log(info_str, LogLevel::RAW_MESSAGE, false);
            break;
        case LogLevel::ENGINE_INFO:
            if (!m_debug) {
                return;
            }  // else fallthrough
        default:
            info_str.append("string ");
            log(info_str, LogLevel::RAW_MESSAGE, false);
            break;
    }
    return GenericEngine::log(msg, level, flush);
};

void UCIEngine::report(const size_t depth, const eval::centipawn_t eval,
                       const size_t nodes,
                       const std::chrono::duration<double> time,
                       const MoveBuffer &pv) const {
    constexpr static uint64_t ms_per_s = 1000;
    std::string info_string;
    info_string += "depth ";
    info_string += std::to_string(depth);
    info_string += " score ";
    if (eval == eval::max_eval || eval == -eval::max_eval) {
        info_string += "mate ";
        info_string += std::to_string((int)(eval > 0) * depth / 2);
    } else {
        info_string += "cp ";
        info_string += std::to_string(eval);
    }
    info_string += " nodes ";
    info_string += std::to_string(nodes);
    info_string += " time ";
    info_string += std::to_string((uint64_t)(time.count() * ms_per_s));
    info_string += " nps ";
    info_string +=
        std::to_string((uint64_t)(static_cast<double>(nodes) / time.count()));
    info_string += " pv";

    for (const move::FatMove fmove : pv) {
        info_string += " ";
        info_string += move::LongAlgMove(fmove);
    }

    info_string.push_back('\n');

    log(info_string, LogLevel::PROTOCOL_INFO, true);
};

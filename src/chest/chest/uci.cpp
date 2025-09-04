#include "uci.h"

#include <cstdlib>
#include <sstream>
#include <string>

#include "engine.h"
#include "libChest/makemove.h"
#include "libChest/state.h"
#include "libChest/timemanagement.h"

// Free helper

void display_state(const GenericEngine &engine) {
    std::string state_str = engine.m_astate.state.pretty();
    state_str.append(engine.m_astate.state.to_fen());
    std::stringstream out_stream{state_str};
    for (std::string ln; std::getline(out_stream, ln);) {
        ln.push_back('\n');
        engine.log(ln, LogLevel::ENGINE_INFO);
    }
}

// UCICheck

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

std::optional<int> UCICheck::execute() {
    identify();
    m_engine->log("uciok\n", LogLevel::RAW_MESSAGE);
    return {};
};

// IsReady

std::optional<int> ISReady::execute() {
    m_engine->log("readyok\n", LogLevel::RAW_MESSAGE);
    return {};
};

// DebugConfig

bool DebugConfig::parse(const std::string_view keyword,
                        std::stringstream &args) {
    (void)keyword;
    std::string arg;
    args >> arg;
    if (arg == "on") {
        m_engine->m_debug = true;
    } else if (arg == "off") {
        m_engine->m_debug = false;
    } else {
        return false;
    }
    return true;
};
std::optional<int> DebugConfig::execute() { return {}; };

// Position
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
    m_astate = m_engine->m_astate;
};

bool Position::sufficient_args() const { return m_astate.has_value(); };
std::optional<int> Position::execute() {
    state::SearchNode<1> sn{m_engine->m_mover, m_astate.value(), 0};

    for (const std::string &move : m_moves) {
        std::optional<move::FatMove> fmove =
            move::LongAlgMove(move).to_fmove(sn.m_astate);
        if (!fmove.has_value()) {
            return false;
        }
        // TODO: batch updates in batches of MAX_LENGTH
        sn.prep_search(1);
        sn.make_move(fmove.value());
    }

    m_engine->m_astate = sn.m_astate;

    if (m_engine->m_debug) {
        display_state(*m_engine);
    }

    return {};
}

// Quit
std::optional<int> Quit::execute() { return {EXIT_SUCCESS}; };

// Go

// TODO: check if need template<typename T>
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

bool Go::sufficient_args() const {
    const board::Colour to_move = m_engine->m_astate.state.to_move;
    return m_tc.movetime || m_tc.copy_remaining(to_move);
};

std::optional<int> Go::execute() {
    // Types used for searching.
    // Could template at engine level.
    using EvalTp = eval::DefaultEval;
    using DlSearcherTp = search::DLNegaMax<EvalTp, MAX_DEPTH>;
    using SearcherTp = search::IDSearcher<DlSearcherTp, MAX_DEPTH>;
    using TimeManagerTp = search::DefaultTimeManager;

    // Calculate stop time
    // TODO: add buffer to account for parsing time
    // Maybe in parse(): record time message came in.
    TimeManagerTp time_manager{};
    state::AugmentedState &eng_astate = m_engine->m_astate;
    search::ms_t search_time = time_manager(m_tc, eng_astate.state.to_move);

    // TODO:consider making searcher member of engine rather than dynamically
    // allocating.
    DlSearcherTp nega(m_engine->m_mover, eng_astate, MAX_DEPTH);

    std::chrono::time_point<std::chrono::steady_clock> finish_time =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(search_time);

    SearcherTp idsearcher{eng_astate, nega, *m_engine};
    move::FatMove best = idsearcher.search(finish_time).best_move;

    std::string msg = "bestmove ";
    msg.append((move::long_alg_t)move::LongAlgMove(best, eng_astate));
    msg.push_back('\n');
    m_engine->log(msg, LogLevel::RAW_MESSAGE);
    return {};
};

// Main engine

void UCIEngine::log(const std::string_view &msg, const LogLevel level,
                    bool flush) const {
    std::string info_str = "info ";
    switch (level) {
        case LogLevel::RAW_MESSAGE:
            break;
        case LogLevel::PROTOCOL_INFO:
            log(info_str, LogLevel::RAW_MESSAGE, false);
            break;
        default:
            info_str.append("string ");
            log(info_str, LogLevel::RAW_MESSAGE, false);
            break;
    }
    return GenericEngine::log(msg, level, flush);
};

void UCIEngine::report(int depth, eval::centipawn_t eval, size_t nodes,
                       std::chrono::duration<double> time,
                       const svec<move::FatMove, MaxMoves> &pv,
                       const state::AugmentedState &astate) const {
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
    info_string += std::to_string((uint64_t)(nodes / time.count()));
    info_string += " pv";

    for (const move::FatMove fmove : pv) {
        info_string += " ";
        info_string += (move::long_alg_t)move::LongAlgMove(fmove, astate);
    }

    info_string.push_back('\n');

    log(info_string, LogLevel::PROTOCOL_INFO, true);
};

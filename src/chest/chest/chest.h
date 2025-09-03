#ifndef ENGINESTATE_H
#define ENGINESTATE_H

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "libChest/debug.h"
#include "libChest/eval.h"
#include "libChest/makemove.h"
#include "libChest/move.h"
#include "libChest/movebuffer.h"
#include "libChest/movegen.h"
#include "libChest/search.h"
#include "libChest/state.h"
#include "libChest/timemanagement.h"

//
// Engine state, using the state pattern.
// Since this is only handled by the io thread,
// virtual inheritance (state pattern) is used.
//

namespace id {
const std::string name = "Chest";
const std::string author = "Liam van der Vyver";
}  // namespace id

// Communications from the gui to the engine.
// In general, parsing, etc. is handled by the engine state struct, so this is
// kept minimal.
struct UciCommand {
   public:
    UciCommand(std::string input)
        : type(UciCommandType::UNRECOGNISED), input(input), args() {
        std::stringstream ss = (std::stringstream)input;
        std::string cur_tkn;

        // Find the first token which matches
        while ((type == UciCommandType::UNRECOGNISED) && ss >> cur_tkn) {
            type = parse_command(cur_tkn);
        }

        while (ss >> cur_tkn) {
            if (!cur_tkn.empty()) {
                args.push_back(cur_tkn);
            }
        }
        return;
    }
    enum class UciCommandType : uint8_t {
        UNRECOGNISED = 0,  // generally should be ignored.
        UCI,
        CONFIG_DEBUG,
        ISREADY,
        SETOPTION,
        REGISTER,
        UCINEWGAME,
        POSITION,
        GO,
        STOP,
        PONDERHIT,
        QUIT,
        // Non-standard
        PERFT,
    };
    UciCommandType type;
    std::string input;
    std::vector<std::string> args;

   private:
    std::string content;
    UciCommandType parse_command(const std::string &tkn) {
        if (tkn == "uci") {
            return UciCommandType::UCI;
        } else if (tkn == "debug") {
            return UciCommandType::CONFIG_DEBUG;
        } else if (tkn == "isready") {
            return UciCommandType::ISREADY;
        } else if (tkn == "setoption") {
            return UciCommandType::SETOPTION;
        } else if (tkn == "register") {
            return UciCommandType::REGISTER;
        } else if (tkn == "ucinewgame") {
            return UciCommandType::UCINEWGAME;
        } else if (tkn == "position") {
            return UciCommandType::POSITION;
        } else if (tkn == "go") {
            return UciCommandType::GO;
        } else if (tkn == "stop") {
            return UciCommandType::STOP;
        } else if (tkn == "ponderhit") {
            return UciCommandType::PONDERHIT;
        } else if (tkn == "quit") {
            return UciCommandType::QUIT;
        } else if (tkn == "perft") {
            return UciCommandType::PERFT;
        } else
            return UciCommandType::UNRECOGNISED;
    }
};

constexpr const int MAX_DEPTH = 64;

// Global vars, shared between the io thread and any working threads
struct Globals {
   public:
    move::movegen::AllMoveGenerator<> mover;

    // debugging
    enum class LogLevel : uint8_t {
        UCI_INFO,
        CHEST_INFO,
        CHEST_WARN,
        CHEST_ERR,
    };
    // TODO: implement fully
    // For now, just handle uci info always,
    // And log other info to stdout in debug mode.
    void log(std::string msg, LogLevel level = LogLevel::CHEST_INFO) const {
        if (level == LogLevel::UCI_INFO) {
            output << "info " << msg << '\n';
        } else if (debug) {
            output << "info string " << msg << '\n';
        };
    }

    // When debug is true, send extra debug info to output
    static constexpr std::string debug_str = "debug ";
#ifdef DEBUG
    bool debug = true
#else
    bool debug = false
#endif
        ;
    std::ostream &output = std::cout;
};

// Report reults of partial searches
// TODO: check that mate report works
// for now, assume the max eval means mate,
// and depth gives the number of ply til mate
struct EngineReporter : search::StatReporter {
    EngineReporter(const Globals &globals) : m_globals(globals) {};
    void report(int depth, eval::centipawn_t eval, size_t nodes,
                std::chrono::duration<double> time,
                const svec<move::FatMove, MaxMoves> &pv,
                const state::AugmentedState &astate) const override {
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
        info_string += std::to_string((uint64_t)(time.count() * 1000));
        info_string += " nps ";
        info_string += std::to_string((uint64_t)(nodes / time.count()));
        info_string += " pv";

        for (const move::FatMove fmove : pv) {
            info_string += " ";
            info_string += (move::long_alg_t)move::LongAlgMove(fmove, astate);
        }

        m_globals.log(info_string, Globals::LogLevel::UCI_INFO);
    };

   private:
    const Globals &m_globals;
};

// Abstract classes to handle state machine of UCI engine,
// i.e. respond to different commands.
class Engine {
   public:
    Engine(Globals &globals)
        : m_input(std::cin),
          m_globals(globals),
          m_astate(),
          m_input_buffer(new char[max_input_line_length]) {}

    virtual void handle_command(UciCommand command) {
        switch (command.type) {
            case UciCommand::UciCommandType::UNRECOGNISED:
                m_globals.log("unrecognised command: \n" + command.input,
                              Globals::LogLevel::UCI_INFO);
                break;
            case UciCommand::UciCommandType::UCI:
                identify();
                tell_opts();
                m_globals.output << "uciok" << std::endl;
                break;
            case UciCommand::UciCommandType::CONFIG_DEBUG:
                handle_debug(command);
                break;
            case UciCommand::UciCommandType::ISREADY:
                m_globals.output << "readyok" << std::endl;
                break;
            case UciCommand::UciCommandType::SETOPTION:
                m_globals.log("setoption not supported",
                              Globals::LogLevel::UCI_INFO);
                break;
            case UciCommand::UciCommandType::REGISTER:
                m_globals.log("register not supported",
                              Globals::LogLevel::UCI_INFO);
                break;
            case UciCommand::UciCommandType::UCINEWGAME:
                break;
            case UciCommand::UciCommandType::POSITION: {
                std::optional<state::AugmentedState> new_state =
                    handle_position(command);
                if (new_state) {
                    m_astate = new_state.value();
                };
                break;
            }
            case UciCommand::UciCommandType::GO:
                go(command);
                break;
            case UciCommand::UciCommandType::STOP:
                m_globals.log("stop not supported",
                              Globals::LogLevel::UCI_INFO);
                break;
            case UciCommand::UciCommandType::PONDERHIT:
                m_globals.log("ponderhit not supported",
                              Globals::LogLevel::UCI_INFO);
                go(command);
                break;

            case UciCommand::UciCommandType::QUIT:
                exit(0);
                break;

            case UciCommand::UciCommandType::PERFT:
                handle_perft(command);
                break;
        };
    }
    UciCommand read_command() {
        m_input.getline(m_input_buffer.get(), max_input_line_length - 1, '\n');
        return UciCommand{m_input_buffer.get()};
    }

   private:
    std::istream &m_input;
    Globals &m_globals;
    state::AugmentedState m_astate;
    std::unique_ptr<char> m_input_buffer;

    static constexpr size_t max_input_line_length = 16384;

    void bad_args(UciCommand command) {
        std::string msg;
        msg.append("bad arguments:");
        for (const std::string &arg : command.args) {
            msg.append(" ");
            msg.append(arg);
        }
        m_globals.log(msg, Globals::LogLevel::CHEST_ERR);
    };

    void identify() {
        m_globals.output << "id name " << id::name << std::endl;
        m_globals.output << "id author " << id::author << std::endl;
    }

    void tell_opts() {
        // Currently, no options supported
        return;
    }

    // Handle different commands
    void handle_debug(UciCommand command) {
        if (command.args.size() != 1) {
            bad_args(command);
        } else if (command.args.at(0) == "on") {
            m_globals.debug = true;
        } else if (command.args.at(0) == "off") {
            m_globals.debug = false;
        } else {
            bad_args(command);
        }
    };

    void handle_perft(UciCommand command) {
        if (command.args.size() != 1) {
            return bad_args(command);
        }
        int depth = std::stoi(command.args.at(0));

        // HACK: in makemove, should check if we have a matching element for
        // debug state::SearchNode<MAX_DEPTH> sn{m_globals.mover, m_astate,
        // depth};
        state::SearchNode<MAX_DEPTH, eval::DefaultEval, Zobrist> sn{
            m_globals.mover, m_astate, depth};
        size_t total = 0;
        sn.prep_search(depth);
        MoveBuffer &root_moves = sn.find_moves();

        for (auto fmv : root_moves) {
            if (sn.make_move(fmv)) {
                auto results = sn.perft().perft;
                total += results;
                std::string msg =
                    static_cast<std::string>(move::LongAlgMove{fmv, m_astate});
                msg.append(": ");
                msg.append(std::to_string(results));
                m_globals.log(msg);
            }

            sn.unmake_move();
        }

        std::string msg = "TOTAL: ";
        msg.append(std::to_string(total));
        m_globals.log(msg);
    }

    void log_board(Globals::LogLevel level = Globals::LogLevel::CHEST_INFO) {
        m_globals.log(m_astate.state.to_fen().append("\n").append(
                          m_astate.state.pretty()),
                      level);
    }

    std::optional<state::AugmentedState> handle_position(UciCommand command) {
        size_t i = 0;
        size_t sz = command.args.size();
        const int fen_len = 6;

        if (command.args.size() == 0) {
            log_board();
            return m_astate;
        }

        std::string &fst = command.args.at(i++);
        std::string state_fen;
        std::optional<state::AugmentedState> start_state;

        if (fst == "moves") {
            start_state = m_astate;
            --i;  // ensure moves will be read,
        } else if (fst == "startpos") {
            start_state = {{state::new_game_fen}};
        } else if (fst == "fen") {
            if (i + fen_len > sz) {
                bad_args(command);
            } else {
                state_fen = command.args.at(i++);
                for (int j = 1; j < fen_len; j++) {
                    state_fen += " ";
                    state_fen += command.args.at(i++);
                }
            }
            start_state = {{state_fen}};
        } else {
            bad_args(command);
            return {};
        }

        if (i < sz) {
            if (command.args.at(i++) != "moves") {
                bad_args(command);
                return {};
            }
        }

        // Handle state updates
        m_astate = start_state.value();
        state::SearchNode<1, eval::NullEval> sn(m_globals.mover, m_astate, 1);

        // Handle moves
        while (i < sz) {
            std::optional<move::FatMove> fmove =
                move::LongAlgMove(command.args.at(i++))
                    .to_fmove(start_state.value());
            if (!fmove.has_value()) {
                bad_args(command);
                return {};
            }

            // Make move
            sn.prep_search(1);
            sn.make_move(fmove.value());
        }

        if (m_globals.debug) {
            log_board();
        }

        return {sn.m_astate};
    }

    void go(UciCommand command) {
        (void)command;

        std::optional<int> to_go{0};

        std::optional<search::ms_t> b_remaining{1};
        std::optional<search::ms_t> w_remaining{1};
        std::optional<search::ms_t> b_increment{0};
        std::optional<search::ms_t> w_increment{0};

        // TODO: do this better
        for (size_t i = 0; i < command.args.size(); i++) {
            if (command.args.at(i) == "movestogo") {
                if (i + 1 == command.args.size()) {
                    bad_args(command);
                }
                to_go = std::stoi(command.args.at(++i));
            }

            if (command.args.at(i) == "btime") {
                if (i + 1 == command.args.size()) {
                    bad_args(command);
                }
                b_remaining = std::stoi(command.args.at(++i));
            }

            if (command.args.at(i) == "wtime") {
                if (i + 1 == command.args.size()) {
                    bad_args(command);
                }
                w_remaining = std::stoi(command.args.at(++i));
            }

            if (command.args.at(i) == "binc") {
                if (i + 1 == command.args.size()) {
                    bad_args(command);
                }
                b_increment = std::stoi(command.args.at(++i));
            }

            if (command.args.at(i) == "winc") {
                if (i + 1 == command.args.size()) {
                    bad_args(command);
                }
                w_increment = std::stoi(command.args.at(++i));
            }
        }

        if (!(w_remaining && b_remaining)) {
            return bad_args(command);
        }
        search::TimeControl time_control(
            b_remaining.value(), w_remaining.value(), b_increment.value(),
            w_increment.value(), to_go.value());

        using EvalTp = eval::DefaultEval;
        using DlSearcherTp = search::DLNegaMax<EvalTp, MAX_DEPTH>;
        using SearcherTp = search::IDSearcher<DlSearcherTp, MAX_DEPTH>;
        using TimeManagerTp = search::DefaultTimeManager;

        DlSearcherTp nega(m_globals.mover, m_astate, MAX_DEPTH);

        // Create reporter
        EngineReporter reporter(m_globals);

        // Calculate stop time
        TimeManagerTp time_manager{};
        search::ms_t search_time =
            time_manager(time_control, m_astate.state.to_move);

        std::chrono::time_point<std::chrono::steady_clock> finish_time =
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(search_time);

        SearcherTp idsearcher{m_astate, nega, reporter};
        move::FatMove best = idsearcher.search(finish_time).best_move;

        m_globals.output << "bestmove "
                         << (move::long_alg_t)move::LongAlgMove(best, m_astate)
                         << std::endl;
        ;
    }
};

#endif

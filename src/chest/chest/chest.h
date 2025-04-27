#ifndef ENGINESTATE_H
#define ENGINESTATE_H

#include <cmath>
#include <iostream>
#include <libChest/makemove.h>
#include <optional>
#include <ostream>
#include <sstream>
#include <vector>

//
// Engine state, using the state pattern.
// Since this is only handled by the io thread,
// virtual inheritance (state pattern) is used.
//

namespace id {
const std::string name = "Chest";
const std::string author = "Liam van der Vyver";
} // namespace id

// Communications from the gui to the engine.
// In general, parsing, etc. is handled by the engine state struct, so this is
// kept minimal.
struct UciCommand {
  public:
    UciCommand(char *input_buf) : type(UciCommandType::UNRECOGNISED), args() {
        std::stringstream ss = (std::stringstream)input_buf;
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
        UNRECOGNISED = 0, // generally should be ignored.
        UCI,
        DEBUG,
        ISREADY,
        SETOPTION,
        REGISTER,
        UCINEWGAME,
        POSITION,
        GO,
        STOP,
        PONDERHIT,
        QUIT,
    };
    UciCommandType type;
    std::vector<std::string> args;

  private:
    std::string content;
    UciCommandType parse_command(const std::string &tkn) {
        if (tkn == "uci") {
            return UciCommandType::UCI;
        } else if (tkn == "debug") {
            return UciCommandType::DEBUG;
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
        } else
            return UciCommandType::UNRECOGNISED;
    }
};

// Global vars, shared between the io thread and any working threads
struct Globals {
  public:
    Globals() : mover(), output(std::cout) {};
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
    void log(std::string msg, LogLevel level = LogLevel::CHEST_INFO) {
        if (level == LogLevel::UCI_INFO) {
            output << "info " << msg << std::endl;
        } else if (debug) {
            output << "info string " << msg << std::endl;
        };
    }

    // When debug is true, send extra debug info to output
    static constexpr std::string debug_str = "debug ";
    bool debug;
    std::ostream &output;
};

// Abstract classes to handle state machine of UCI engine,
// i.e. respond to different commands.
class Engine {
  public:
    Engine(Globals &globals)
        : m_input(std::cin), m_output(std::cout), m_globals(globals),
          m_astate() {}

    virtual void handle_command(UciCommand command) {
        switch (command.type) {
        case UciCommand::UciCommandType::UNRECOGNISED:
            m_globals.log("unrecognised command", Globals::LogLevel::UCI_INFO);
            break;
        case UciCommand::UciCommandType::UCI:
            identify();
            tell_opts();
            m_output << "uciok" << std::endl;
            break;
        case UciCommand::UciCommandType::DEBUG:
            handle_debug(command);
            break;
        case UciCommand::UciCommandType::ISREADY:
            m_output << "readyok" << std::endl;
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
            m_globals.log("stop not supported", Globals::LogLevel::UCI_INFO);
            break;
        case UciCommand::UciCommandType::PONDERHIT:
            m_globals.log("ponderhit not supported",
                          Globals::LogLevel::UCI_INFO);
            go(command);
            break;

        case UciCommand::UciCommandType::QUIT:
            exit(0);
            break;
        };
    }
    UciCommand read_command() {
        char input_buf[max_input_line_length];
        m_input.getline(input_buf, max_input_line_length - 1, '\n');
        return UciCommand{input_buf};
    }

  private:
    std::istream &m_input;
    std::ostream &m_output;
    Globals &m_globals;
    state::AugmentedState m_astate;

    static constexpr size_t max_input_line_length = 1024;

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
        m_output << "id name " << id::name << std::endl;
        m_output << "id author " << id::author << std::endl;
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

    std::optional<state::AugmentedState> handle_position(UciCommand command) {

        size_t i = 0;
        size_t sz = command.args.size();
        const int fen_len = 6;

        if (command.args.size() == 0) {
            bad_args(command);
            return {};
        }

        std::string &fst = command.args.at(i++);
        std::string state_fen;
        if (fst == "startpos") {
            state_fen = state::new_game_fen;
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
        }
        state::AugmentedState ret =
            state::AugmentedState(state::State(state_fen));

        if (i < sz) {
            if (command.args.at(i++) != "moves") {
                bad_args(command);
                return {};
            }
        }

        // Handle state updates
        state::SearchNode<move::movegen::AllMoveGenerator<>, 1> sn(
            m_globals.mover, ret, 1);

        // Handle moves
        while (i < sz) {
            std::optional<move::FatMove> fmove =
                move::LongAlgMove(command.args.at(i++)).to_fmove(ret);
            if (!fmove.has_value()) {
                bad_args(command);
                return {};
            }

            m_output << fmove->get_move().pretty() << std::endl;

            // Make move
            sn.prep_search(1);
            sn.make_move(fmove.value());
        }
        m_output << ret.state.pretty() << std::endl;

        return {sn.m_astate};
    }

    void go(UciCommand command) {
        // TODO: implement search options
        (void)command;

        state::SearchNode<move::movegen::AllMoveGenerator<>, 1> sn(
            m_globals.mover, m_astate, 1);
        move::FatMove best = sn.get_random_move().value();
        m_output << "bestmove "
                 << (move::long_alg_t)move::LongAlgMove(best, m_astate)
                 << std::endl;
        ;
    }
};

#endif

//============================================================================//
// Generic classes for defining an engine.
// Designed for UCI, should generalise to other protocols.
// See uci.h for uci specific code.
//============================================================================//

#include "engine.h"

bool EngineCommand::parse(const std::string_view keyword,
                          std::stringstream &args) {
    std::string cur_tkn;
    while (args >> cur_tkn) {
        if (!m_fields.contains(cur_tkn)) {
            bad_arg(keyword, cur_tkn);
        } else {
            m_fields[cur_tkn](cur_tkn, args);
        }
    }
    return sufficient_args();
}

void EngineCommand::bad_arg(const std::string_view keyword,
                            const std::string_view tkn) const {
    std::string msg = "unrecognised argument to ";
    msg.append(keyword);
    msg.append(": ");
    msg.append(tkn);
    msg.push_back('\n');
    m_engine->log(msg, LogLevel::ENGINE_WARN);
}

void EngineCommand::bad_usage(const std::string_view input) const {
    std::string msg = "invalid usage: ";
    msg.append(input);
    msg.push_back('\n');
    m_engine->log(msg, LogLevel::ENGINE_WARN);
}

// Print error messages and consume tokens until a token is matched.
// Returns whether it was matched
bool EngineCommand::match_literal(const std::string_view keyword,
                                  const std::string_view match_tkn,
                                  std::stringstream &args) const {
    std::string cur_tkn;
    while (args >> cur_tkn) {
        if (cur_tkn == match_tkn) {
            return true;
        } else {
            bad_arg(keyword, cur_tkn);
        }
    }
    std::string msg;
    msg.append(keyword);
    msg.append(" expects argument ");
    msg.append(match_tkn);
    msg.push_back('\n');
    m_engine->log(msg, LogLevel::ENGINE_WARN);
    return false;
};

std::optional<int> GenericEngine::run() {
    // Read line from input stream
    std::string input;
    getline(*m_input, input);
    std::stringstream ss{input};
    std::string keyword;

    // Parse first keyword
    while (ss >> keyword) {
        if (!m_commands.count(keyword)) {
            bad_command(keyword);
        } else {
            break;
        }
    }

    // Return if no valid keyword
    if (!m_commands.count(keyword)) {
        return {};
    }

    std::unique_ptr<EngineCommand> cmd = m_commands.at(keyword)();
    if (!cmd) {
        log("Invalid command registration.\n", LogLevel::ENGINE_ERR);
        return {};
    }

    if (!cmd->parse(keyword, ss)) {
        cmd->bad_usage(input);
        return {};
    }

    return cmd->execute();
}

void GenericEngine::log(const std::string_view &msg, const LogLevel level,
                        const bool flush) const {
    switch (level) {
        case LogLevel::ENGINE_INFO:
            if (!m_debug) {
                return;
            }
            *m_output << "[INFO]: ";
            break;
        case LogLevel::ENGINE_WARN:
            *m_output << "[WARN]: ";
            break;
        case LogLevel::ENGINE_ERR:
            *m_output << "[ERROR]: ";
            break;
        default:
            break;
    }
    m_output_lock.lock();
    *m_output << msg;
    if (flush) {
        *m_output << std::flush;
    }
    m_output_lock.unlock();
}

void GenericEngine::debug_log(const std::string_view &msg) const {
    return GenericEngine::log(msg, LogLevel::ENGINE_INFO, false);
};

void GenericEngine::bad_command(const std::string_view cmd) const {
    std::string msg = "unrecognised command: ";
    msg.append(cmd);
    msg.push_back('\n');
    log(msg, LogLevel::ENGINE_WARN);
}

void GenericEngine::bad_command_args(const std::string_view input) const {
    std::string msg = "invalid usage of command: ";
    msg.append(input);
    msg.push_back('\n');
    log(msg, LogLevel::ENGINE_WARN);
}

bool GenericEngine::check_not_busy() const {
    if (is_busy()) {
        log("search in progress\n", LogLevel::ENGINE_WARN);
        return false;
    };
    return true;
};

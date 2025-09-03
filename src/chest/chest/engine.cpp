#include "engine.h"

bool EngineCommand::parse(const std::string_view keyword,
                          std::stringstream &args) {
    std::string cur_tkn;
    while (args >> cur_tkn) {
        if (!m_fields.contains(cur_tkn)) {
            bad_arg(keyword, cur_tkn);
        } else {
            m_fields[cur_tkn](args, *m_engine);
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
    m_engine->log(msg);
}

void EngineCommand::bad_usage(const std::string_view input) const {
    std::string msg = "invalid usage: ";
    msg.append(input);
    msg.push_back('\n');
    m_engine->log(msg);
}

std::optional<int> GenericEngine::run() {
    // Read line from input stream
    std::string input;
    getline(*m_input, input);
    std::stringstream ss{input};
    std::string keyword;
    ss >> keyword;

    // Parse first keyword
    if (!m_commands.count(keyword)) {
        bad_command(keyword);
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

void GenericEngine::log(const std::string_view &msg,
                        const LogLevel level) const {
    (void)level;
    *m_output << msg;
}

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

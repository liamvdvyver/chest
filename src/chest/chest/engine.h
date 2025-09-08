//============================================================================//
// Generic classes for defining an engine.
// Designed for UCI, should generalise to other protocols.
// See uci.h for uci specific code.
//============================================================================//

#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>

#include "libChest/movegen.h"
#include "libChest/search.h"
#include "libChest/util.h"

constexpr size_t MAX_DEPTH = 64;

class GenericEngine;

class EngineCommand {
   public:
    using FieldParser = std::function<void(const std::string_view keyword,
                                           std::stringstream &args)>;

    // Pass fields to construct (in derived class one-arg constructor)
    EngineCommand(GenericEngine *engine) : m_engine(engine) {};
    EngineCommand(const EngineCommand &) = default;
    EngineCommand(EngineCommand &&) = delete;
    EngineCommand &operator=(const EngineCommand &) = default;
    EngineCommand &operator=(EngineCommand &&) = delete;
    virtual ~EngineCommand() = default;

    // Setup state for execution,
    // returns whether parse was sufficient to execute.
    virtual bool parse(const std::string_view keyword, std::stringstream &args);

    // Run the command.
    // Returns return code if engine should exit, otherwise empty.
    virtual std::optional<int> execute() = 0;

    void bad_arg(const std::string_view keyword,
                 const std::string_view tkn) const;

    void bad_usage(const std::string_view input) const;

   protected:
    std::map<std::string, FieldParser> m_fields;
    GenericEngine *m_engine;

    // After parse(): were the parse: arguments sufficient to execute()?.
    virtual bool sufficient_args() const { return true; };
};

enum class LogLevel : uint8_t {
    RAW_MESSAGE,    // Sent directly to output stream
    PROTOCOL_INFO,  // Configurable format for protocol-required information
    ENGINE_INFO,    // No exceptional circumstance
    ENGINE_WARN,    // Recoverable exceptional circumstance
    ENGINE_ERR,     // Internal error
};

class GenericEngine : public search::StatReporter {
   protected:
    using CommandFactory = std::function<std::unique_ptr<EngineCommand>()>;

   public:
    friend EngineCommand;
    // Pass commands to construct (in derived class no-arg constructor)
    GenericEngine(std::unordered_map<std::string, CommandFactory> &&commands)
        : m_commands(std::move(commands)) {}

    // Special members
    GenericEngine(const GenericEngine &) = default;
    GenericEngine(GenericEngine &&) = delete;
    GenericEngine &operator=(const GenericEngine &) = delete;
    GenericEngine &operator=(GenericEngine &&) = delete;
    ~GenericEngine() override = default;

    // Read command and execute.
    // Returns return code if engine should exit, otherwise empty.
    virtual std::optional<int> run();

    virtual void log(const std::string_view &msg,
                     const LogLevel level = LogLevel::ENGINE_INFO,
                     bool flush = false) const;

    std::istream *m_input = &std::cin;
    std::ostream *m_output = &std::cout;
    state::AugmentedState m_astate{};
    const move::movegen::AllMoveGenerator
        m_mover{};  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    bool m_debug = DEBUG();

    void bad_command(const std::string_view cmd) const;
    void bad_command_args(const std::string_view input) const;

   protected:
    std::unordered_map<std::string, CommandFactory> m_commands;
};

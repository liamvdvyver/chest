// Generic classes for defining an engine.
// Designed for UCI, should generalise to other protocols.
// See uci.h for uci specific code.

#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>

#include "libChest/search.h"

class GenericEngine;

class EngineCommand {
   public:
    using FieldParser =
        std::function<void(std::stringstream &args, GenericEngine &engine)>;

    // Pass fields to construct (in derived class one-arg constructor)
    EngineCommand(GenericEngine *engine,
                  std::map<std::string, FieldParser> fields)
        : m_fields(std::move(fields)), m_engine(engine) {};
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
    virtual bool sufficient_args() = 0;
};

enum class LogLevel : uint8_t {
    RAW_MESSAGE,    // Sent directly to output stream
    PROTOCOL_INFO,  // Configurable format for protocol-required information
    ENGINE_INFO,    // No exceptional circumstance
    ENGINE_WARN,    // Recoverable exceptional circumstance
    ENGINE_ERR,     // Internal error
};

class GenericEngine : search::StatReporter {
   protected:
    using CommandFactory = std::function<std::unique_ptr<EngineCommand>()>;

   public:
    // Pass commands to construct (in derived class no-arg constructor)
    GenericEngine(std::map<std::string, CommandFactory> commands)
        : m_commands(std::move(commands)) {}

    // Special members
    GenericEngine(const GenericEngine &) = default;
    GenericEngine(GenericEngine &&) = delete;
    GenericEngine &operator=(const GenericEngine &) = delete;
    GenericEngine &operator=(GenericEngine &&) = delete;
    virtual ~GenericEngine() = default;

    // Read command and execute.
    // Returns return code if engine should exit, otherwise empty.
    virtual std::optional<int> run();

    virtual void log(const std::string_view &msg,
                     const LogLevel level = LogLevel::ENGINE_INFO) const;

   protected:
    std::istream *m_input = &std::cin;
    std::ostream *m_output = &std::cout;
    state::AugmentedState m_astate{};

    std::map<std::string, CommandFactory> m_commands;

    void bad_command(const std::string_view cmd) const;
    void bad_command_args(const std::string_view input) const;
};

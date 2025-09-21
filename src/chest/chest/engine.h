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
#include <thread>

#include "libChest/eval.h"
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

    // Print error messages and consume tokens until a token is matched.
    // Returns whether it was matched
    bool match_literal(const std::string_view keyword,
                       const std::string_view match_tkn,
                       std::stringstream &args) const;

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

    // Read command and execute.
    // Returns return code if engine should exit, otherwise empty.
    virtual std::optional<int> run();

    virtual void log(const std::string_view &msg,
                     const LogLevel level = LogLevel::ENGINE_INFO,
                     bool flush = false) const;

    void debug_log(const std::string_view &msg) const override;

    void bad_command(const std::string_view cmd) const;
    void bad_command_args(const std::string_view input) const;

    //-- Accessors -----------------------------------------------------------//

    auto &get_node() { return m_node; }
    const auto &get_node() const { return m_node; }

    auto &get_astate() { return m_astate; }
    const auto &get_astate() const { return m_astate; }

    auto &get_ttable() { return m_ttable; }
    const auto &get_ttable() const { return m_ttable; }

    auto &get_searcher() { return m_searcher; }
    const auto &get_searcher() const { return m_searcher; }

    bool is_debug() const { return m_debug; }

    //-- Mutators ------------------------------------------------------------//

    void set_astate(state::AugmentedState &astate) {
        m_astate = astate;
        m_node.set_astate(m_astate);
    }

    void set_debug(const bool debug) { m_debug = debug; }

    //-- Worker thread -------------------------------------------------------//

    bool is_busy() const { return m_busy; }
    void set_busy(const bool busy = true) { m_busy = busy; }

    // Returns false and warns if busy
    bool check_not_busy() const;

    std::shared_ptr<std::thread> &get_worker() { return m_worker; };

    std::mutex &get_result_lock() { return m_result_lock; };

   protected:
    std::istream *m_input = &std::cin;
    std::ostream *m_output = &std::cout;
    state::AugmentedState m_astate{};
    search::DefaultNode<eval::DefaultEval, MAX_DEPTH> m_node =
        search::DefaultNode<eval::DefaultEval, MAX_DEPTH>(m_astate, MAX_DEPTH);

    std::unordered_map<std::string, CommandFactory> m_commands;
    bool m_debug = DEBUG();
    search::TTable m_ttable{};

    using EvalTp = eval::DefaultEval;
    using DlSearcherTp = search::DLNegaMax<EvalTp, MAX_DEPTH>;
    using IDSearcherTp = search::IDSearcher<DlSearcherTp, MAX_DEPTH>;

    IDSearcherTp m_searcher{m_node, m_ttable};

    std::shared_ptr<std::thread> m_worker = nullptr;

    // Set to true when engine is searching
    // TODO: use a mutex
    std::atomic<bool> m_busy = false;

    // Unlock when engine is ready to receive result
    std::mutex m_result_lock;

    mutable std::mutex m_output_lock;
};

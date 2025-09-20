//============================================================================//
// Implementation of GenericEngine for the UCI protocol.
//============================================================================//

#pragma once

#include "engine.h"
#include "libChest/search.h"
#include "libChest/state.h"
#include "libChest/timemanagement.h"

//============================================================================//
// Options
//============================================================================//

class UCIOption : public EngineCommand {
   public:
    using EngineCommand::EngineCommand;

    virtual std::string get_type_string() const = 0;
};

class UCISpinOption : public UCIOption {
   public:
    UCISpinOption(GenericEngine *engine, const int default_val,
                  const int min_val, const int max_val)
        : UCIOption(engine),
          m_default_val(default_val),
          m_min_val(min_val),
          m_max_val(max_val) {}

    std::string get_type_string() const override;

    bool parse(std::string_view opt_name, std::stringstream &value) override;

   protected:
    int m_default_val;
    int m_min_val;
    int m_max_val;

    int m_set_val = m_default_val;
};

class Hash : public UCISpinOption {
   public:
    Hash(GenericEngine *engine) : UCISpinOption(engine, 1, 1, gb) {};

    std::optional<int> execute() override;

   private:
    static constexpr size_t gb = 1024;
};

//============================================================================//
// Commands
//============================================================================//

class ISReady : public EngineCommand {
   public:
    ISReady(GenericEngine *engine) : EngineCommand(engine) {}
    std::optional<int> execute() override;
};

class DebugConfig : public EngineCommand {
   public:
    DebugConfig(GenericEngine *engine) : EngineCommand(engine) {}
    bool parse(const std::string_view keyword,
               std::stringstream &args) override;
    std::optional<int> execute() override;
};

class UciNewGame : public EngineCommand {
   public:
    UciNewGame(GenericEngine *engine) : EngineCommand(engine) {}
    std::optional<int> execute() override;
};

class Position : public EngineCommand {
   public:
    Position(GenericEngine *engine) : EngineCommand(engine) {
        m_fields["fen"] = [this](const std::string_view keyword,
                                 std::stringstream &args) {
            return fen_impl(keyword, args);
        };
        m_fields["moves"] = [this](const std::string_view keyword,
                                   std::stringstream &args) {
            return moves_impl(keyword, args);
        };
        m_fields["startpos"] = [this](const std::string_view keyword,
                                      std::stringstream &args) {
            return startpos_impl(keyword, args);
        };
        m_fields["curpos"] = [this](const std::string_view keyword,
                                    std::stringstream &args) {
            return curpos_impl(keyword, args);
        };
    }
    bool sufficient_args() const override;
    std::optional<int> execute() override;

   private:
    void fen_impl(const std::string_view keyword, std::stringstream &args);
    void moves_impl(const std::string_view keyword, std::stringstream &args);
    void startpos_impl(const std::string_view keyword, std::stringstream &args);
    void curpos_impl(const std::string_view keyword, std::stringstream &args);

    std::vector<std::string> m_moves{};
    std::optional<state::AugmentedState> m_astate{};
};

class Quit : public EngineCommand {
   public:
    Quit(GenericEngine *engine) : EngineCommand(engine) {}
    std::optional<int> execute() override;
};

class Go : public EngineCommand {
   public:
    Go(GenericEngine *engine) : EngineCommand(engine) { register_fields(); }

    std::optional<int> execute() override;
    bool sufficient_args() const override;

   private:
    void register_fields();

    struct SearchArgs {
        GenericEngine *eng{};
        size_t depth{};
        search::Bounds bounds{};
        search::TimeControl tc{};
    };

    enum class SearchType : uint8_t { ID, AB, PERFT };

    size_t m_depth = 0;
    SearchType m_type = SearchType::ID;
    bool m_trace = false;
    search::Bounds m_bounds{};

    void parse_field(const std::string_view keyword, std::stringstream &args,
                     auto &field);

    template <search::VerbosityLevel Verbosity>
    std::optional<int> execute_impl();

    template <search::VerbosityLevel Verbosity,
              Go::SearchType SearchType = Go::SearchType::ID>
    static void search_impl(SearchArgs args);

    std::optional<int> perft_impl();

    search::TimeControl m_tc{};
};

class Stop : public EngineCommand {
   public:
    Stop(GenericEngine *engine) : EngineCommand(engine) {}

    bool sufficient_args() const override;

    std::optional<int> execute() override;
};

//============================================================================//
// Main engine
//============================================================================//

class UCIEngine : public GenericEngine {
   public:
    UCIEngine();

    void log(const std::string_view &msg, const LogLevel level,
             bool flush) const override;

    void report(const size_t depth, const eval::centipawn_t eval,
                const size_t nodes, const std::chrono::duration<double> time,
                const MoveBuffer &pv) const override;

    using OptionFactory = std::function<std::unique_ptr<UCIOption>()>;
    std::unordered_map<std::string, OptionFactory> m_options = {
        {"Hash", [this]() { return std::make_unique<Hash>(this); }}};
};

//============================================================================//
// UCICheck/SetOption: need references to UCIEngine to get options.
//============================================================================//

class UCICheck : public EngineCommand {
   public:
    UCICheck(UCIEngine *engine)
        : EngineCommand(engine), m_options(&engine->m_options) {}

    std::optional<int> execute() override;

   private:
    void identify();
    static constexpr std::string name = "Chest";
    inline static const std::string author = "Liam van der Vyver";

    void tell_options();

    std::unordered_map<std::string, UCIEngine::OptionFactory> *m_options;
};

class SetOption : public EngineCommand {
   public:
    SetOption(UCIEngine *engine)
        : EngineCommand(engine), m_options(&engine->m_options) {}

    bool parse(const std::string_view keyword,
               std::stringstream &args) override;

    std::optional<int> execute() override;

   private:
    std::unordered_map<std::string, UCIEngine::OptionFactory> *m_options;
    std::unique_ptr<UCIOption> m_opt = nullptr;
};

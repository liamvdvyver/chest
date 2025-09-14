//============================================================================//
// Implementation of GenericEngine for the UCI protocol.
//============================================================================//

#pragma once

#include "engine.h"
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
    Go(GenericEngine *engine) : EngineCommand(engine) {
        m_fields["wtime"] =
            [this](const std::string_view keyword, std::stringstream &args

            ) { return time_impl(keyword, args, board::Colour::WHITE); };
        m_fields["btime"] = [this](const std::string_view keyword,
                                   std::stringstream &args) {
            return time_impl(keyword, args, board::Colour::BLACK);
        };
        m_fields["winc"] =
            [this](const std::string_view keyword, std::stringstream &args

            ) { return inc_impl(keyword, args, board::Colour::WHITE); };
        m_fields["binc"] =
            [this](const std::string_view keyword, std::stringstream &args

            ) { return inc_impl(keyword, args, board::Colour::BLACK); };
        m_fields["movestogo"] = [this](const std::string_view keyword,
                                       std::stringstream &args

                                ) { return movestogo_impl(keyword, args); };
        m_fields["movetime"] = [this](const std::string_view keyword,
                                      std::stringstream &args

                               ) { return movetime_impl(keyword, args); };
        m_fields["perft"] = [this](const std::string_view keyword,
                                   std::stringstream &args

                            ) { return perft_impl(keyword, args); };
    }
    std::optional<int> execute() override;
    bool sufficient_args() const override;

   private:
    // std::optional<size_t> m_perft_depth{};
    size_t m_perft_depth = 0;

    void parse_field(const std::string_view keyword, std::stringstream &args,
                     auto &field);
    void inc_impl(const std::string_view keyword, std::stringstream &args,
                  board::Colour to_move);
    void time_impl(const std::string_view keyword, std::stringstream &args,
                   board::Colour to_move);
    void movestogo_impl(const std::string_view keyword,
                        std::stringstream &args);
    void movetime_impl(const std::string_view keyword, std::stringstream &args);
    void perft_impl(const std::string_view keyword, std::stringstream &args);

    std::optional<int> search_impl();
    std::optional<int> perft_impl();

    search::TimeControl m_tc{};
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

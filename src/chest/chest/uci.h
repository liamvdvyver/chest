
#pragma once

#include <string>

#include "engine.h"
#include "libChest/state.h"
#include "libChest/timemanagement.h"

class UCICheck : public EngineCommand {
   public:
    UCICheck(GenericEngine *engine) : EngineCommand(engine) {}
    std::optional<int> execute() override;

   private:
    void identify();
    static constexpr std::string name = "Chest";
    inline static const std::string author = "Liam van der Vyver";
};

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
    // GOOd BNEW
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
            return this->time_impl(keyword, args, board::Colour::BLACK);
        };
        m_fields["winc"] =
            [this](const std::string_view keyword, std::stringstream &args

            ) { return this->inc_impl(keyword, args, board::Colour::WHITE); };
        m_fields["binc"] =
            [this](const std::string_view keyword, std::stringstream &args

            ) { return this->inc_impl(keyword, args, board::Colour::BLACK); };
        m_fields["movestogo"] =
            [this](const std::string_view keyword, std::stringstream &args

            ) { return this->movestogo_impl(keyword, args); };
    }
    std::optional<int> execute() override;
    bool sufficient_args() const override;

   private:
    void parse_field(const std::string_view keyword, std::stringstream &args,
                     auto &field);
    void inc_impl(const std::string_view keyword, std::stringstream &args,
                  board::Colour to_move);

    void time_impl(const std::string_view keyword, std::stringstream &args,
                   board::Colour to_move);

    void movestogo_impl(const std::string_view keyword,
                        std::stringstream &args);

    search::TimeControl m_tc{};
};

class UCIEngine : public GenericEngine {
   public:
    UCIEngine()
        : GenericEngine({
              {"uci", [this]() { return std::make_unique<UCICheck>(this); }},
              {"isready", [this]() { return std::make_unique<ISReady>(this); }},
              {"debug",
               [this]() { return std::make_unique<DebugConfig>(this); }},
              {"position",
               [this]() { return std::make_unique<Position>(this); }},
              {"quit", [this]() { return std::make_unique<Quit>(this); }},
              {"go", [this]() { return std::make_unique<Go>(this); }},
          }) {}

    void log(const std::string_view &msg, const LogLevel level,
             bool flush) const override;

    void report(int depth, eval::centipawn_t eval, size_t nodes,
                std::chrono::duration<double> time,
                const svec<move::FatMove, MaxMoves> &pv,
                const state::AugmentedState &astate) const override;
};

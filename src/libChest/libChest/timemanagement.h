#pragma once

#include <libChest/board.h>
#include <libChest/state.h>

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace search {

using ms_t = uint64_t;

// If movetime is set -> time manager shouldn't decide how long to think.
struct TimeControl {
    constexpr TimeControl() = default;
    constexpr TimeControl(ms_t movetime) : movetime(movetime) {}
    constexpr TimeControl(ms_t b_remaining, ms_t w_remaining, ms_t b_increment,
                          ms_t w_increment, size_t to_go = 0)
        : to_go(to_go),
          m_remaining{b_remaining, w_remaining},
          m_increment{b_increment, w_increment} {};

    size_t to_go = 0;
    ms_t movetime = 0;
    constexpr ms_t &remaining(board::Colour colour) {
        return m_remaining[(int)colour];
    };
    constexpr ms_t &increment(board::Colour colour) {
        return m_increment[(int)colour];
    };
    constexpr ms_t copy_remaining(board::Colour colour) const {
        return m_remaining[(int)colour];
    };
    constexpr ms_t copy_increment(board::Colour colour) const {
        return m_increment[(int)colour];
    };

   private:
    std::array<ms_t, board::n_colours> m_remaining{0, 0};
    std::array<ms_t, board::n_colours> m_increment{0, 0};
};

// StaticTimeManager is a functor which suggests the time to think
// based on time control remaining and current state.
// State is passed at construction, allowing incremental updates, etc.
template <typename T>
concept StaticTimeManager =
    requires(T t, const TimeControl &tc, const board::Colour to_move) {
        std::constructible_from<const state::AugmentedState &>;
        { t(tc, to_move) } -> std::same_as<ms_t>;
    };

//
// Default time manager
//

// Return remaining / MovesProp + increment / IncProp.
template <ms_t MovesProp, ms_t IncProp>
class EqualTimeManager {
   public:
    constexpr ms_t operator()(const TimeControl &tc,
                              const board::Colour to_move) const {
        const ms_t remaining = tc.copy_remaining(to_move);
        const ms_t increment = tc.copy_increment(to_move);
        return remaining / MovesProp + increment / IncProp;
    };
};

// Choose between two EqualTimeManagers, depending on whether there is another
// time control left, or it is sudden death.
// If movetime is set, return it back.
// Apply a buffer to the returned time to account for IPC/setup latency.
template <StaticTimeManager TNormalTimeManager,
          StaticTimeManager TSuddenDeathTimeManager, ms_t buffer>
class SuddenDeathTimeManager {
   public:
    constexpr ms_t operator()(const TimeControl &tc,
                              const board::Colour to_move) const {
        const ms_t target_time = tc.movetime ? tc.movetime
                                 : tc.to_go  ? m_normal_mgr(tc, to_move)
                                             : m_suddendeath_mgr(tc, to_move);
        return target_time > buffer ? target_time - buffer : 0;
    }

   private:
    TNormalTimeManager m_normal_mgr{};
    TSuddenDeathTimeManager m_suddendeath_mgr{};
};

static constexpr ms_t DefaultRemainingProp = 20;
static constexpr ms_t DefaultIncProp = 20;
static constexpr ms_t DefaultSuddenDeathProp = 45;
static constexpr ms_t DefaultBuffer = 20;

using DefaultTimeManager = search::SuddenDeathTimeManager<
    search::EqualTimeManager<DefaultRemainingProp, DefaultIncProp>,
    EqualTimeManager<DefaultSuddenDeathProp, 1>, DefaultBuffer>;

}  // namespace search
static_assert(search::StaticTimeManager<search::DefaultTimeManager>);

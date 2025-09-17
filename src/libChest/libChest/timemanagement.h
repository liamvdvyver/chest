//============================================================================//
// Time management for search
//============================================================================//

#pragma once

#include "board.h"
#include "state.h"

namespace search {

// All calculations done during ms, in line with protocols.
using ms_t = uint64_t;

//============================================================================//
// Time controls
//============================================================================//

// Contains information regarding the remaining time, increment, moves to next
struct TimeControl {
    constexpr TimeControl() = default;
    constexpr TimeControl(const ms_t movetime) : movetime(movetime) {}
    constexpr TimeControl(const ms_t b_remaining, const ms_t w_remaining,
                          const ms_t b_increment, const ms_t w_increment,
                          const size_t to_go = 0)
        : to_go(to_go),
          m_remaining{b_remaining, w_remaining},
          m_increment{b_increment, w_increment} {};

    // Moves till next time control.
    // If zero, sudden death (no more time controls).
    size_t to_go = 0;

    // If movetime is set -> time manager shouldn't decide how long to think.
    ms_t movetime = 0;

    // Accessors

    constexpr ms_t &remaining(const board::Colour colour) {
        return m_remaining[static_cast<size_t>(colour)];
    };
    constexpr ms_t &increment(const board::Colour colour) {
        return m_increment[static_cast<size_t>(colour)];
    };
    constexpr ms_t copy_remaining(const board::Colour colour) const {
        return m_remaining[static_cast<size_t>(colour)];
    };
    constexpr ms_t copy_increment(const board::Colour colour) const {
        return m_increment[static_cast<size_t>(colour)];
    };

    constexpr bool is_null() const {
        return !(m_remaining[0] || m_remaining[1] || movetime);
    }

   private:
    std::array<ms_t, board::n_colours> m_remaining{0, 0};
    std::array<ms_t, board::n_colours> m_increment{0, 0};
};

//============================================================================//
// Time managers
//============================================================================//

// Functor which suggests the time to think based on time control info and
// current state.
// State is passed at construction, allowing incremental updates, etc.
template <typename T>
concept StaticTimeManager =
    requires(T t, const TimeControl &tc, const board::Colour to_move) {
        std::constructible_from<const state::AugmentedState &>;
        { t(tc, to_move) } -> std::same_as<ms_t>;
    };

//----------------------------------------------------------------------------//
// Templates
//----------------------------------------------------------------------------//

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
        const ms_t movetime_buffered =
            tc.movetime > buffer ? tc.movetime - buffer : 0;
        const ms_t target_time = tc.movetime ? movetime_buffered
                                 : tc.to_go  ? m_normal_mgr(tc, to_move)
                                             : m_suddendeath_mgr(tc, to_move);
        return target_time;
    }

   private:
    TNormalTimeManager m_normal_mgr{};
    TSuddenDeathTimeManager m_suddendeath_mgr{};
};

//----------------------------------------------------------------------------//
// Concrete instances
//----------------------------------------------------------------------------//

static constexpr ms_t DefaultRemainingProp = 20;
static constexpr ms_t DefaultIncProp = 20;
static constexpr ms_t DefaultSuddenDeathProp = 45;
static constexpr ms_t DefaultBuffer = 20;

using DefaultTimeManager = search::SuddenDeathTimeManager<
    search::EqualTimeManager<DefaultRemainingProp, DefaultIncProp>,
    EqualTimeManager<DefaultSuddenDeathProp, 1>, DefaultBuffer>;

}  // namespace search
static_assert(search::StaticTimeManager<search::DefaultTimeManager>);

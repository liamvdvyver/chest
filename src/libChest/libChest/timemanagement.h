#ifndef TIMEMANAGEMENT_H
#define TIMEMANAGEMENT_H

#include <libChest/board.h>
#include <libChest/state.h>

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace search {

using ms_t = uint64_t;

struct TimeControl {
    constexpr TimeControl(ms_t b_remaining, ms_t w_remaining, ms_t b_increment,
                          ms_t w_increment, size_t to_go = 0)
        : to_go(to_go),
          m_remaining{w_remaining, b_remaining},
          m_increment{b_increment, w_increment} {};

    size_t to_go;
    constexpr ms_t &get_remaining(board::Colour colour) {
        return m_remaining[(int)colour];
    };
    constexpr ms_t &get_increment(board::Colour colour) {
        return m_increment[(int)colour];
    };
    constexpr ms_t copy_remaining(board::Colour colour) const {
        return m_remaining[(int)colour];
    };
    constexpr ms_t copy_increment(board::Colour colour) const {
        return m_increment[(int)colour];
    };

   private:
    std::array<ms_t, board::n_colours> m_remaining;
    std::array<ms_t, board::n_colours> m_increment;
};

// StaticTimeManager is a functor which suggests the time to think
// based on time control remaining and current state.
// State is passed at construction, allowing incremental updates, etc.
template <typename T>
concept StaticTimeManager = requires(T t, const TimeControl &tc) {
    std::constructible_from<const state::AugmentedState &>;
    { t(tc) } -> std::same_as<ms_t>;
};

//
// Default time manager
//

// Return remaining / MovesProp + increment / IncProp.
template <ms_t MovesProp, ms_t IncProp>
class EqualTimeManager {
   public:
    EqualTimeManager(const state::AugmentedState &astate)
        : m_astate(&astate) {};

    constexpr ms_t operator()(const TimeControl &tc) const {
        ms_t to_move = tc.copy_remaining(m_astate->state.to_move);
        ms_t increment = tc.copy_increment(m_astate->state.to_move);
        return to_move / MovesProp + increment / IncProp;
    };

   private:
    const state::AugmentedState *m_astate;
};

// Choose between two EqualTimeManagers, depending on whether there is another
// time control left, or it is sudden death.
template <StaticTimeManager TNormalTimeManager,
          StaticTimeManager TSuddenDeathTimeManager>
class SuddenDeathTimeManager {
   public:
    SuddenDeathTimeManager(const state::AugmentedState &astate)
        : m_astate(&astate) {};
    constexpr ms_t operator()(const TimeControl &tc) const {
        return tc.to_go ? m_normal_mgr(tc) : m_death_mgr(tc);
    }

   private:
    const state::AugmentedState *m_astate;
    TNormalTimeManager m_normal_mgr{*m_astate};
    TSuddenDeathTimeManager m_death_mgr{*m_astate};
};

static constexpr ms_t DefaultRemainingProp = 20;
static constexpr ms_t DefaultIncProp = 20;
static constexpr ms_t DefaultSuddenDeathProp = 45;

using DefaultTimeManager = search::SuddenDeathTimeManager<
    search::EqualTimeManager<DefaultRemainingProp, DefaultIncProp>,
    EqualTimeManager<DefaultSuddenDeathProp, 1>>;

}  // namespace search
static_assert(search::StaticTimeManager<search::DefaultTimeManager>);

#endif

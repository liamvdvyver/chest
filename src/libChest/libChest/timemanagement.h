#ifndef TIMEMANAGEMENT_H
#define TIMEMANAGEMENT_H

#include "libChest/board.h"
#include "libChest/state.h"
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace search {

typedef uint64_t ms_t;

struct TimeControl {

    constexpr TimeControl(ms_t b_remaining, ms_t w_remaining, ms_t b_increment,
                          ms_t w_increment, size_t to_go = 0)
        : to_go(to_go), m_remaining{w_remaining, b_remaining},
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
    ms_t m_remaining[board::n_colours];
    ms_t m_increment[board::n_colours];
};

// StaticTimeManager is a functor which suggests the time to think based on time
// control remaining
template <typename T>
concept StaticTimeManager = requires(T t, const TimeControl &tc) {
    { t(tc) } -> std::same_as<ms_t>;
};

//
// Default time manager
//

// Return a constant, max ms
class NullTimeManager {

  public:
    NullTimeManager(const state::AugmentedState &astate) : m_astate(astate) {};

    constexpr ms_t operator()(const TimeControl &tc) const {
        (void)tc;
        ms_t remaining = tc.copy_remaining(m_astate.state.to_move);
        return remaining < buffer ? remaining : remaining - buffer;
    };

  private:
    const state::AugmentedState &m_astate;
    const ms_t buffer = 50; // Buffer around remaining to avoid loss.
};
static_assert(StaticTimeManager<NullTimeManager>);

// Given a static estimate for moves remaining
template <int MovesProp, int IncProp> class EqualTimeManager {

  public:
    EqualTimeManager(const state::AugmentedState &astate) : m_astate(astate) {};

    constexpr ms_t operator()(const TimeControl &tc) const {
        ms_t to_move = tc.copy_remaining(m_astate.state.to_move);
        ms_t increment = tc.copy_increment(m_astate.state.to_move);
        return to_move / MovesProp + increment / IncProp;
    };

  private:
    const state::AugmentedState &m_astate;
};
static_assert(StaticTimeManager<EqualTimeManager<20, 2>>);
typedef EqualTimeManager<20, 2> DefaultTimeManager;
} // namespace search

#endif

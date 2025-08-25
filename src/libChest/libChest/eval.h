#ifndef EVAL_H
#define EVAL_H

#include "board.h"
#include "state.h"
#include <utility>

//
// Basic evaluation of positions
//

namespace eval {

using centipawn_t = int64_t;
constexpr centipawn_t max_eval = INT64_MAX;

// Given a position (augmented state),
// return a static evaluation.
template <typename T>
concept StaticEvaluator = requires(const state::AugmentedState &astate) {
    { T(astate).eval() } -> std::same_as<centipawn_t>;
};

// Returns the standard material evaluation
class MaterialEval {
  public:
    constexpr MaterialEval(const state::AugmentedState &astate)
        : m_astate(astate) {};
    constexpr centipawn_t eval() const {
        return side_eval(m_astate.state.to_move) -
               side_eval(!m_astate.state.to_move);
    }
    static constexpr centipawn_t piece_val(const board::Piece piece) {
        switch (piece) {
        case (board::Piece::KNIGHT):
            return 300;
        case (board::Piece::BISHOP):
            return 300;
        case (board::Piece::ROOK):
            return 500;
        case (board::Piece::QUEEN):
            return 900;
        case (board::Piece::PAWN):
            return 100;
        case (board::Piece::KING):
            return 0;
        default:
            std::unreachable();
        }
    }

  private:
    const state::AugmentedState &m_astate;

    // Helper: one sides's material evaluation
    constexpr centipawn_t side_eval(const board::Colour side) const {
        centipawn_t ret = 0;
        for (board::Piece p = (board::Piece)0; p < board::Piece::KING;
             p = (board::Piece)((int)p + 1)) {
            int bb_sz = m_astate.state.copy_bitboard({side, p}).size();
            ret += (piece_val(p) * bb_sz);
        }
        return ret;
    }
};
static_assert(StaticEvaluator<MaterialEval>);

} // namespace eval

#endif

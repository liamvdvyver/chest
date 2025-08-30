#ifndef EVAL_H
#define EVAL_H

#include <concepts>

#include "libChest/board.h"
#include "state.h"

//
// Basic evaluation of positions
//

namespace eval {

// Evaluation results

// Always returned from the perspective of side to move,
// i.e., a better black position, black to move, yields higher eval.
using centipawn_t = int64_t;

constexpr centipawn_t max_eval = std::numeric_limits<centipawn_t>::max();

// Templates

// Given a position (augmented state),
// return a static evaluation for the side to move.
template <typename T>
concept StaticEvaluator =
    requires(const T t, const state::AugmentedState &astate) {
        { t.eval(astate) } -> std::same_as<centipawn_t>;
    };

// // Given a position (augmented state),
// // return a gross static evaluation for the side to move.
// template <typename T>
// concept OneSideEvaluator = requires(
//     const T t, const state::AugmentedState &astate, const board::Colour side)
//     { { t.side_eval(side) } -> std::same_as<centipawn_t>;
// };

// Net off each side's evaluations.
template <typename T>
class NetEvaluator {
   public:
    constexpr centipawn_t eval(const state::AugmentedState &astate) const {
        return static_cast<const T *>(this)->side_eval(astate,
                                                       astate.state.to_move) -
               static_cast<const T *>(this)->side_eval(astate,
                                                       !astate.state.to_move);
    }
};

// Sum per-piece-type evaluations for a side.
template <typename T>
class SideEvaluator {
   public:
    constexpr centipawn_t side_eval(const state::AugmentedState &astate,
                                    const board::Colour side) const {
        centipawn_t ret = 0;
        for (board::Piece p = static_cast<board::Piece>(0);
             p < board::Piece::KING;
             p = static_cast<board::Piece>(static_cast<int>(p) + 1)) {
            int bb_sz = astate.state.copy_bitboard({side, p}).size();
            ret += (static_cast<const T *>(this)->piece_val(astate, side, p) *
                    bb_sz);
        }
        return ret;
    }
};

// Sum per-piece-per-square evaluations for a side.
template <typename T>
class PSTEval : SideEvaluator<PSTEval<T>>, NetEvaluator<PSTEval<T>> {
   public:
    centipawn_t piece_val(const state::AugmentedState &astate,
                          const board::Colour side, const board::Piece piece) {
        centipawn_t eval = 0;
        auto &pst = static_cast<const T *>(this)->get_pst();
        for (board::Bitboard b :
             astate.state.copy_bitboard({side, piece}).singletons()) {
            eval += pst[b.single_bitscan_forward()];
        }
        return eval;
    }
};

// Evaluators

// Uses the standard material evaluation.
class StdEval : public SideEvaluator<StdEval>, public NetEvaluator<StdEval> {
   public:
    static constexpr centipawn_t piece_val(const state::AugmentedState &astate,
                                           const board::Colour side,
                                           const board::Piece piece) {
        (void)astate;
        (void)side;
        switch (piece) {
            case (board::Piece::PAWN):
                return 100;
            case (board::Piece::KNIGHT):
                return 300;
            case (board::Piece::BISHOP):
                return 300;
            case (board::Piece::ROOK):
                return 500;
            case (board::Piece::QUEEN):
                return 900;
            case (board::Piece::KING):
                return 0;
            default:
                std::unreachable();
        }
    }
};
static_assert(StaticEvaluator<StdEval>);

class MichniewskiMaterial : public SideEvaluator<StdEval>,
                            public NetEvaluator<StdEval> {
   public:
    static constexpr centipawn_t piece_val(const state::AugmentedState &astate,
                                           const board::Colour side,
                                           const board::Piece piece) {
        (void)astate;
        (void)side;
        switch (piece) {
            case (board::Piece::PAWN):
                return 100;
            case (board::Piece::KNIGHT):
                return 320;
            case (board::Piece::BISHOP):
                return 330;
            case (board::Piece::ROOK):
                return 500;
            case (board::Piece::QUEEN):
                return 900;
            case (board::Piece::KING):
                return 20000;
            default:
                std::unreachable();
        }
    }
};
static_assert(StaticEvaluator<MichniewskiMaterial>);

}  // namespace eval

#endif

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
class MaterialEval : public NetEvaluator<T> {
   public:
    static constexpr centipawn_t side_eval(const state::AugmentedState &astate,
                                           const board::Colour side) {
        centipawn_t ret = 0;
        for (board::Piece p : board::PieceTypesIterator()) {
            int bb_sz = astate.state.copy_bitboard({side, p}).size();
            ret += (T::piece_val(p) * bb_sz);
        }
        return ret;
    }
};

// For use by implementers
using PST = std::array<centipawn_t, board::n_squares>;

// Sum per-piece-per-square evaluations for a side.
template <typename T>
class PSTEval : public NetEvaluator<PSTEval<T>> {
   public:
    static constexpr centipawn_t side_eval(const state::AugmentedState &astate,
                                           const board::Colour side) {
        centipawn_t ret = 0;

        for (board::Piece p : board::PieceTypesIterator()) {
            for (board::Bitboard b :
                 astate.state.copy_bitboard({side, p}).singletons()) {
                ret += T::pst_val({side, p}, b.single_bitscan_forward());
            }
        }
        return ret;
    }
};

// Evaluators

// Uses the standard material evaluation.
class StdEval : public MaterialEval<StdEval> {
   public:
    static constexpr centipawn_t piece_val(const board::Piece piece) {
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

class MichniewskiMaterial : public MaterialEval<StdEval> {
   public:
    static constexpr centipawn_t piece_val(const board::Piece piece) {
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

class MichniewskiPST : public PSTEval<MichniewskiPST> {
   public:
    static constexpr centipawn_t pst_val(board::ColouredPiece cp,
                                         board::Square sq) {
        board::Square offset =
            cp.colour == board::Colour::WHITE ? sq.flip() : sq;
        switch (cp.piece) {
            case (board::Piece::PAWN):
                return m_b_pawn_vals[offset];
            case (board::Piece::KNIGHT):
                return m_b_knight_vals[offset];
            case (board::Piece::BISHOP):
                return m_b_bishop_vals[offset];
            case (board::Piece::ROOK):
                return m_b_rook_vals[offset];
            case (board::Piece::QUEEN):
                return m_b_queen_vals[offset];
            case (board::Piece::KING):
                // TODO: blend between two pst's
                return m_b_king_vals_mid[offset];
            default:
                std::unreachable();
        };
    };

   private:
    static constexpr PST m_b_pawn_vals = {
        // clang-format off
         0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
         5,  5, 10, 25, 25, 10,  5,  5,
         0,  0,  0, 20, 20,  0,  0,  0,
         5, -5,-10,  0,  0,-10, -5,  5,
         5, 10, 10,-20,-20, 10, 10,  5,
         0,  0,  0,  0,  0,  0,  0,  0
        // clang-format on
    };
    static constexpr PST m_b_knight_vals = {
        // clang-format off
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50,
        // clang-format on
    };
    static constexpr PST m_b_bishop_vals = {
        // clang-format off
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20,
        // clang-format on
    };
    static constexpr PST m_b_rook_vals = {
        // clang-format off
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
        // clang-format on
    };
    static constexpr PST m_b_queen_vals = {
        // clang-format off
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
        // clang-format on
    };
    static constexpr PST m_b_king_vals_mid = {
        // clang-format off
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
        // clang-format on
    };

    static constexpr PST m_b_king_vals_end = {
        // clang-format off
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
        // clang-format on
    };
};
static_assert(StaticEvaluator<MichniewskiPST>);

class MichniewskiEval : public NetEvaluator<MichniewskiEval> {
   public:
    static constexpr centipawn_t side_eval(const state::AugmentedState &astate,
                                           const board::Colour side) {
        return MichniewskiMaterial::side_eval(astate, side) +
               MichniewskiPST::side_eval(astate, side);
    }
};
static_assert(StaticEvaluator<MichniewskiEval>);

}  // namespace eval

#endif

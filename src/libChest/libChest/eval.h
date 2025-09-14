//============================================================================//
// Evaluation of positions
//============================================================================//

#pragma once

#include "board.h"
#include "incremental.h"
#include "state.h"

namespace eval {

// Evaluation results

// Always returned from the perspective of side to move,
// i.e., a better black position, black to move, yields higher eval.
using centipawn_t = int64_t;

constexpr centipawn_t max_eval = std::numeric_limits<centipawn_t>::max();

//============================================================================//
// Static evaluation
//============================================================================//

//----------------------------------------------------------------------------//
// Concepts
//----------------------------------------------------------------------------//

// Initialised with position (augmented state),
// returns a static evaluation for the side to move,
// i.e., a better black position, black to move, yields higher eval.
template <typename T>
concept StaticEvaluator =
    requires(const T t, const state::AugmentedState &astate) {
        std::constructible_from<T, const state::AugmentedState &>;
        { t.eval() } -> std::same_as<centipawn_t>;
    };

// Initialised with position (augmented state),
// returns a static evaluation for the side to move.
// A valid  should return the same evaluation as fresh initialisation.
template <typename T>
concept IncrementallyUpdateableEvaluator = requires() {
    StaticEvaluator<T>;
    IncrementallyUpdateable<T>;
};

// Assigns values per-piece, e.g. standard evaluation.
template <typename T>
concept PieceEvaluator = requires(const board::Piece p) {
    { T::piece_val(p) } -> std::same_as<centipawn_t>;
};

// Assigns values per-square, per-piece.
template <typename T>
concept PieceSquareEvaluator =
    requires(const board::ColouredPiece cp, const board::Square sq) {
        { T::pst_val(cp, sq) } -> std::same_as<centipawn_t>;
    };

// Assigns a value to one side's position.
template <typename T>
concept SideEvaluator = requires(const T t, const board::Colour c) {
    { t.side_eval(c) } -> std::same_as<centipawn_t>;
};

//----------------------------------------------------------------------------//
// CRTP templates for implementation
//----------------------------------------------------------------------------//

// Net off each side's evaluations.
template <typename T>
class NetEval {
   public:
    constexpr centipawn_t eval() const {
        static_assert(SideEvaluator<T>);
        return static_cast<const T *>(this)->side_eval(
                   m_astate.get().state.to_move) -
               static_cast<const T *>(this)->side_eval(
                   !m_astate.get().state.to_move);
    }

   protected:
    std::reference_wrapper<const state::AugmentedState> m_astate;
    friend T;

   private:
    NetEval(const state::AugmentedState &astate) : m_astate(astate) {}
};

// Sum per-piece-type evaluations for a side.
template <typename T>
class MaterialEval : public NetEval<MaterialEval<T>> {
   public:
    constexpr centipawn_t side_eval(const board::Colour side) const {
        static_assert(PieceEvaluator<T>);
        centipawn_t ret = 0;
        for (board::Piece p : board::PieceTypesIterator()) {
            int bb_sz = static_cast<const T *>(this)
                            ->m_astate.state.copy_bitboard({side, p})
                            .size();
            ret += (T::piece_val(p) * bb_sz);
        }
        return ret;
    }
    friend T;

   private:
    MaterialEval(const state::AugmentedState &astate)
        : NetEval<MaterialEval>(astate) {}
};

// For use by implementers
using PST = std::array<centipawn_t, board::n_squares>;

// Sum per-piece-per-square evaluations for a side.
template <typename T>
class PSTEval : public NetEval<PSTEval<T>> {
   public:
    constexpr centipawn_t side_eval(const board::Colour side) const {
        static_assert(PieceSquareEvaluator<T>);
        centipawn_t ret = 0;

        for (board::Piece p : board::PieceTypesIterator()) {
            for (board::Bitboard b : static_cast<const T *>(this)
                                         ->m_astate.get()
                                         .state.copy_bitboard({side, p})
                                         .singletons()) {
                ret += T::pst_val({side, p}, b.single_bitscan_forward());
            }
        }
        return ret;
    }
    friend T;

   private:
    PSTEval(const state::AugmentedState &astate) : NetEval<PSTEval>(astate) {}
};

//============================================================================//
// Combined material/PST
//============================================================================//

// Given piece values and extra piece-square values for placement,
// return piece-square values which account for both.
template <PieceEvaluator TPEval, PieceSquareEvaluator TPSEval>
class CombinedEval : public PSTEval<CombinedEval<TPEval, TPSEval>> {
   public:
    CombinedEval(const state::AugmentedState &astate)
        : PSTEval<CombinedEval>(astate) {};

    // TODO: recompute pst values at initialisation time.
    static constexpr centipawn_t pst_val(const board::ColouredPiece cp,
                                         const board::Square sq) {
        return TPEval::piece_val(cp.piece) + TPSEval::pst_val(cp, sq);
    }
};

//============================================================================//
// Incremental updates
//============================================================================//

// Given a SideEvaluator, compute eval per-side on initialisation
// and incrementally update.
template <SideEvaluator TEval>
class IncrementalNetEval : public NetEval<IncrementalNetEval<TEval>> {
   public:
    // Must initialise with state.
    constexpr IncrementalNetEval() = delete;

    // Initialises with evaluation.
    constexpr IncrementalNetEval(const state::AugmentedState &astate)
        : NetEval<IncrementalNetEval>(astate) {
        // TEval is assumed a stateless PSTEval, therefore quick to construct.
        m_evaluation[0] =
            TEval(astate).side_eval(static_cast<board::Colour>(0));
        m_evaluation[1] =
            TEval(astate).side_eval(static_cast<board::Colour>(1));
    }

    // Side evaluation

    constexpr centipawn_t side_eval(const board::Colour side) const {
        return m_evaluation[static_cast<size_t>(side)];
    }

    // Incremental updates

    constexpr void add(const board::Bitboard loc,
                       const board::ColouredPiece cp) {
        m_evaluation[static_cast<size_t>(cp.colour)] +=
            TEval::pst_val(cp, loc.single_bitscan_forward());
    }
    constexpr void remove(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        m_evaluation[static_cast<size_t>(cp.colour)] -=
            TEval::pst_val(cp, loc.single_bitscan_forward());
    }
    constexpr void move(const board::Bitboard from, const board::Bitboard to,
                        const board::ColouredPiece cp) {
        remove(from, cp);
        add(to, cp);
    }
    constexpr void swap(const board::Bitboard loc,
                        const board::ColouredPiece from,
                        const board::ColouredPiece to) {
        remove(loc, from);
        add(loc, to);
    }
    constexpr void swap_oppside(const board::Bitboard loc,
                                const board::ColouredPiece from,
                                const board::ColouredPiece to) {
        swap(loc, from, to);
    }
    constexpr void swap_sameside(const board::Bitboard loc,
                                 const board::Colour side,
                                 const board::Piece from,
                                 const board::Piece to) {
        swap(loc, {side, from}, {side, to});
    }

    // Castling rights/ep/halfmove clock do not affect eval
    constexpr void toggle_castling_rights(state::CastlingRights rights) const {
        (void)rights;
    }
    constexpr void add_ep_sq(board::Square ep_sq) const { (void)ep_sq; }
    constexpr void remove_ep_sq(board::Square ep_sq) const { (void)ep_sq; }
    constexpr void increment_halfmove() const {}
    constexpr void decrement_halfmove() const {}

    // To move is fetched on demand
    constexpr void set_to_move(const board::Colour to_move) const {
        (void)to_move;
    }

   private:
    std::array<centipawn_t, board::n_colours> m_evaluation{};
};

//============================================================================//
// Concrete instances
//============================================================================//

//----------------------------------------------------------------------------//
// Standard
//----------------------------------------------------------------------------//

// Standard material evaluation.
class StdEval : public MaterialEval<StdEval> {
   public:
    StdEval(const state::AugmentedState &astate) : MaterialEval(astate) {};

    static constexpr centipawn_t piece_val(const board::Piece piece) {
        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
        switch (piece) {
            case (board::Piece::PAWN):
                return 100;
            case (board::Piece::KNIGHT):  // NOLINT bugprone-branch-clone
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
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    }
};
static_assert(StaticEvaluator<StdEval>);

//----------------------------------------------------------------------------//
// Evaluation functions from Tomasz Michniewski's Unified Evaluation test
// tournament.
// See https://www.chessprogramming.org/Simplified_Evaluation_Function
//----------------------------------------------------------------------------//

// Material scores only
// from Tomasz Michniewski's Unified Evaluation test tournament.
class MichniewskiMaterialEval : public MaterialEval<MichniewskiMaterialEval> {
   public:
    MichniewskiMaterialEval(const state::AugmentedState &astate)
        : MaterialEval(astate) {};

    static constexpr centipawn_t piece_val(const board::Piece piece) {
        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
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
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    }
};
static_assert(StaticEvaluator<MichniewskiMaterialEval>);

// Position scores only
// from Tomasz Michniewski's Unified Evaluation test tournament.
class MichniewskiPSTEval : public PSTEval<MichniewskiPSTEval> {
   public:
    MichniewskiPSTEval(const state::AugmentedState &astate)
        : PSTEval(astate) {};

    static constexpr centipawn_t pst_val(const board::ColouredPiece cp,
                                         const board::Square sq) {
        const board::Square offset =
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

    // PST values

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

// Full evaluation function
using MichniewskiEval =
    CombinedEval<MichniewskiMaterialEval, MichniewskiPSTEval>;
static_assert(StaticEvaluator<MichniewskiEval>);

// Full evaluation function, incrementally updateable
using MichniewskiIncrementalEval = IncrementalNetEval<MichniewskiEval>;
static_assert(IncrementallyUpdateableEvaluator<MichniewskiIncrementalEval>);

//----------------------------------------------------------------------------//
// Current recommended evaluation function.
//----------------------------------------------------------------------------//

using DefaultEval = MichniewskiIncrementalEval;

}  // namespace eval

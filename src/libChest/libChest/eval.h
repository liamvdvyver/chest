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
using centipawn_t = int32_t;

constexpr centipawn_t max_eval = std::numeric_limits<centipawn_t>::max() / 4;

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

// Calculates phase.
template <typename T>
concept PhaseEvaluator = requires(const T t) {
    { t.phase() } -> std::same_as<centipawn_t>;
    { t.max_phase() } -> std::same_as<centipawn_t>;
};

template <typename T>
concept IncrementallyUpdateablePhaseEvaluator = requires() {
    PhaseEvaluator<T>;
    IncrementallyUpdateable<T>;
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
                            ->m_astate.get()
                            .state.copy_bitboard({side, p})
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
// Not intended for use as crtp bases.
//============================================================================//

// Given a SideEvaluator, compute eval per-side on initialisation
// and incrementally update.
template <typename TEval>
class IncrementalNetPSTEval : public NetEval<IncrementalNetPSTEval<TEval>> {
   public:
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

   public:
    // Initialises with evaluation.
    constexpr IncrementalNetPSTEval(const state::AugmentedState &astate)
        : NetEval<IncrementalNetPSTEval>(astate) {
        static_assert(SideEvaluator<TEval>);
        // TEval is assumed a stateless PSTEval, therefore quick to construct.
        m_evaluation[0] =
            TEval(astate).side_eval(static_cast<board::Colour>(0));
        m_evaluation[1] =
            TEval(astate).side_eval(static_cast<board::Colour>(1));
    }

   private:
    std::array<centipawn_t, board::n_colours> m_evaluation{};
};

template <typename TEval>
class IncrementalNetMaterialEval
    : public NetEval<IncrementalNetMaterialEval<TEval>> {
   public:
    constexpr IncrementalNetMaterialEval(const state::AugmentedState &astate)
        : NetEval<IncrementalNetMaterialEval>(astate) {
        static_assert(SideEvaluator<TEval>);
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
        (void)loc;
        m_evaluation[static_cast<size_t>(cp.colour)] +=
            TEval::piece_val(cp.piece);
    }
    constexpr void remove(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        (void)loc;
        m_evaluation[static_cast<size_t>(cp.colour)] -=
            TEval::piece_val(cp.piece);
    }
    constexpr void move(const board::Bitboard from, const board::Bitboard to,
                        const board::ColouredPiece cp) {
        (void)from;
        (void)to;
        (void)cp;
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
// Tapered eval
//============================================================================//

enum class GamePhase : bool {
    MIDGAME,
    ENDGAME,
};

// Given a material evaluator,
// make incremental and compute phase.
// Interpolates over (crunched) closed interval [MgLimit, EgLimit].
template <PieceEvaluator T, centipawn_t MgLimit, centipawn_t EgLimit>
class PhaseEval : public IncrementalNetMaterialEval<T> {
   public:
    constexpr PhaseEval(const state::AugmentedState &astate)
        : IncrementalNetMaterialEval<T>(astate) {};

    // Eval returns the current phase value.
    constexpr centipawn_t mg_phase() const {
        const centipawn_t unbounded_phase =
            static_cast<const IncrementalNetMaterialEval<T> *>(this)->side_eval(
                board::Colour::BLACK) +
            static_cast<const IncrementalNetMaterialEval<T> *>(this)->side_eval(
                board::Colour::WHITE) -
            MgLimit;

        if (unbounded_phase > max_phase()) {
            return max_phase();
        } else if (unbounded_phase < 0) {
            return 0;
        } else {
            return unbounded_phase;
        }
    }

    constexpr centipawn_t eg_phase() const { return max_phase() - mg_phase(); }

    constexpr centipawn_t max_phase() const { return EgLimit - MgLimit; }
};

template <IncrementallyUpdateableEvaluator TMgEval,
          IncrementallyUpdateableEvaluator TEgEval,
          IncrementallyUpdateablePhaseEvaluator TPhaseEval>
class TaperedEval : public NetEval<TaperedEval<TMgEval, TEgEval, TPhaseEval>> {
   public:
    constexpr TaperedEval(const state::AugmentedState &astate)
        : NetEval<TaperedEval<TMgEval, TEgEval, TPhaseEval>>(astate),
          m_mg_eval(astate),
          m_eg_eval(astate),
          m_phase(astate) {}

    constexpr centipawn_t side_eval(const board::Colour side) const {
        const centipawn_t mg_eval = m_mg_eval.side_eval(side);
        const centipawn_t eg_eval = m_eg_eval.side_eval(side);

        const centipawn_t mg_phase = m_phase.mg_phase();
        const centipawn_t eg_phase = m_phase.eg_phase();

        return (mg_eval * mg_phase + eg_eval * eg_phase) / m_phase.max_phase();
    }

    // Incremental updates

    constexpr void add(const board::Bitboard loc,
                       const board::ColouredPiece cp) {
        m_mg_eval.add(loc, cp);
        m_eg_eval.add(loc, cp);
        m_phase.add(loc, cp);
    }
    constexpr void remove(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        m_mg_eval.remove(loc, cp);
        m_eg_eval.remove(loc, cp);
        m_phase.remove(loc, cp);
    }
    constexpr void move(const board::Bitboard from, const board::Bitboard to,
                        const board::ColouredPiece cp) {
        m_mg_eval.move(from, to, cp);
        m_eg_eval.move(from, to, cp);
        m_phase.move(from, to, cp);
    }
    constexpr void swap(const board::Bitboard loc,
                        const board::ColouredPiece from,
                        const board::ColouredPiece to) {
        m_mg_eval.swap(loc, from, to);
        m_eg_eval.swap(loc, from, to);
        m_phase.swap(loc, from, to);
    }
    constexpr void swap_oppside(const board::Bitboard loc,
                                const board::ColouredPiece from,
                                const board::ColouredPiece to) {
        m_mg_eval.swap_oppside(loc, from, to);
        m_eg_eval.swap_oppside(loc, from, to);
        m_phase.swap_oppside(loc, from, to);
    }
    constexpr void swap_sameside(const board::Bitboard loc,
                                 const board::Colour side,
                                 const board::Piece from,
                                 const board::Piece to) {
        m_mg_eval.swap_sameside(loc, side, from, to);
        m_eg_eval.swap_sameside(loc, side, from, to);
        m_phase.swap_sameside(loc, side, from, to);
    }
    constexpr void toggle_castling_rights(
        const state::CastlingRights rights) const {
        m_mg_eval.toggle_castling_rights(rights);
        m_eg_eval.toggle_castling_rights(rights);
        m_phase.toggle_castling_rights(rights);
    }
    constexpr void add_ep_sq(const board::Square ep_sq) const {
        m_mg_eval.add_ep_sq(ep_sq);
        m_eg_eval.add_ep_sq(ep_sq);
        m_phase.add_ep_sq(ep_sq);
    }
    constexpr void remove_ep_sq(const board::Square ep_sq) const {
        m_mg_eval.remove_ep_sq(ep_sq);
        m_eg_eval.remove_ep_sq(ep_sq);
        m_phase.remove_ep_sq(ep_sq);
    }
    constexpr void set_to_move(const board::Colour to_move) const {
        m_mg_eval.set_to_move(to_move);
        m_eg_eval.set_to_move(to_move);
        m_phase.set_to_move(to_move);
    }

   private:
    TMgEval m_mg_eval;
    TEgEval m_eg_eval;
    TPhaseEval m_phase;
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
    constexpr StdEval(const state::AugmentedState &astate)
        : MaterialEval(astate) {};

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
    constexpr MichniewskiMaterialEval(const state::AugmentedState &astate)
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
template <GamePhase Phase>
class MichniewskiPSTEval : public PSTEval<MichniewskiPSTEval<Phase>> {
   public:
    constexpr MichniewskiPSTEval(const state::AugmentedState &astate)
        : PSTEval<MichniewskiPSTEval<Phase>>(astate) {};

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
                if constexpr (Phase == GamePhase::MIDGAME) {
                    return m_b_king_vals_mid[offset];
                } else {
                    return m_b_king_vals_end[offset];
                }
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

// Use second option for tapering:
// endgame when no queens remaining
class MichniewskiPhase : public MaterialEval<MichniewskiPhase> {
   public:
    constexpr MichniewskiPhase(const state::AugmentedState &astate)
        : MaterialEval<MichniewskiPhase>(astate) {};

    static constexpr centipawn_t piece_val(const board::Piece p) {
        return p == board::Piece::QUEEN ? 1 : 0;
    }
};

// Full evaluation function

template <GamePhase Phase>
using MichniewskiEval =
    CombinedEval<MichniewskiMaterialEval, MichniewskiPSTEval<Phase>>;

template <GamePhase Phase>
using MichniewskiIncrementalEval =
    IncrementalNetPSTEval<MichniewskiEval<Phase>>;

using MichniewskiIncrementalPhase = PhaseEval<MichniewskiPhase, 0, 1>;

using MichniewskiTaperedEval =
    TaperedEval<MichniewskiIncrementalEval<GamePhase::MIDGAME>,
                MichniewskiIncrementalEval<GamePhase::ENDGAME>,
                MichniewskiIncrementalPhase>;

static_assert(IncrementallyUpdateableEvaluator<MichniewskiTaperedEval>);

//----------------------------------------------------------------------------//
// PeSTO evaluation function:
// https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
//----------------------------------------------------------------------------//

template <GamePhase Phase>
class PeSTOPSTEval : public PSTEval<PeSTOPSTEval<Phase>> {
   public:
    constexpr PeSTOPSTEval<Phase>(const state::AugmentedState &astate)
        : PSTEval<PeSTOPSTEval<Phase>>(astate){};

    template <GamePhase P = Phase>
    static constexpr centipawn_t pst_val(const board::ColouredPiece cp,
                                         const board::Square sq);

   private:
    //-- Value impls ---------------------------------------------------------//

    template <>
    constexpr centipawn_t pst_val<GamePhase::MIDGAME>(
        const board::ColouredPiece cp, const board::Square sq) {
        const board::Square offset =
            cp.colour == board::Colour::WHITE ? sq.flip() : sq;
        switch (cp.piece) {
            case (board::Piece::PAWN):
                return piece_val_mg(board::Piece::PAWN) +
                       m_pawn_values_mg[offset];
            case (board::Piece::KNIGHT):
                return piece_val_mg(board::Piece::KNIGHT) +
                       m_knight_values_mg[offset];
            case (board::Piece::BISHOP):
                return piece_val_mg(board::Piece::BISHOP) +
                       m_bishop_values_mg[offset];
            case (board::Piece::ROOK):
                return piece_val_mg(board::Piece::ROOK) +
                       m_rook_values_mg[offset];
            case (board::Piece::QUEEN):
                return piece_val_mg(board::Piece::QUEEN) +
                       m_queen_values_mg[offset];
            case (board::Piece::KING):
                return piece_val_mg(board::Piece::KING) +
                       m_king_values_mg[offset];
            default:
                std::unreachable();
        };
    }

    template <>
    constexpr centipawn_t pst_val<GamePhase::ENDGAME>(
        const board::ColouredPiece cp, const board::Square sq) {
        const board::Square offset =
            cp.colour == board::Colour::WHITE ? sq.flip() : sq;
        switch (cp.piece) {
            case (board::Piece::PAWN):
                return piece_val_eg(board::Piece::PAWN) +
                       m_pawn_values_eg[offset];
            case (board::Piece::KNIGHT):
                return piece_val_eg(board::Piece::KNIGHT) +
                       m_knight_values_eg[offset];
            case (board::Piece::BISHOP):
                return piece_val_eg(board::Piece::BISHOP) +
                       m_bishop_values_eg[offset];
            case (board::Piece::ROOK):
                return piece_val_eg(board::Piece::ROOK) +
                       m_rook_values_eg[offset];
            case (board::Piece::QUEEN):
                return piece_val_eg(board::Piece::QUEEN) +
                       m_queen_values_eg[offset];
            case (board::Piece::KING):
                return piece_val_eg(board::Piece::KING) +
                       m_king_values_eg[offset];
            default:
                std::unreachable();
        };
    }

    //-- Piece values --------------------------------------------------------//

    static constexpr centipawn_t piece_val_mg(const board::Piece piece) {
        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
        switch (piece) {
            case (board::Piece::PAWN):
                return 82;
            case (board::Piece::KNIGHT):
                return 337;
            case (board::Piece::BISHOP):
                return 365;
            case (board::Piece::ROOK):
                return 477;
            case (board::Piece::QUEEN):
                return 1025;
            case (board::Piece::KING):
                return 0;
            default:
                std::unreachable();
        }
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    }

    static constexpr centipawn_t piece_val_eg(const board::Piece piece) {
        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
        switch (piece) {
            case (board::Piece::PAWN):
                return 94;
            case (board::Piece::KNIGHT):
                return 281;
            case (board::Piece::BISHOP):
                return 297;
            case (board::Piece::ROOK):
                return 512;
            case (board::Piece::QUEEN):
                return 939;
            case (board::Piece::KING):
                return 0;
            default:
                std::unreachable();
        }
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    }

    //-- PSTs ----------------------------------------------------------------//

    static constexpr PST m_pawn_values_mg{
        // clang-format off
          0,   0,   0,   0,   0,   0,  0,   0,
         98, 134,  61,  95,  68, 126, 34, -11,
         -6,   7,  26,  31,  65,  56, 25, -20,
        -14,  13,   6,  21,  23,  12, 17, -23,
        -27,  -2,  -5,  12,  17,   6, 10, -25,
        -26,  -4,  -4, -10,   3,   3, 33, -12,
        -35,  -1, -20, -23, -15,  24, 38, -22,
          0,   0,   0,   0,   0,   0,  0,   0,
        // clang-format on
    };

    static constexpr PST m_pawn_values_eg = {
        // clang-format off
          0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
         94, 100,  85,  67,  56,  53,  82,  84,
         32,  24,  13,   5,  -2,   4,  17,  17,
         13,   9,  -3,  -7,  -7,  -8,   3,  -1,
          4,   7,  -6,   1,   0,  -5,  -1,  -8,
         13,   8,   8,  10,  13,   0,   2,  -7,
          0,   0,   0,   0,   0,   0,   0,   0,
        // clang-format on
    };

    static constexpr PST m_knight_values_mg = {
        // clang-format off
        -167, -89, -34, -49,  61, -97, -15, -107,
         -73, -41,  72,  36,  23,  62,   7,  -17,
         -47,  60,  37,  65,  84, 129,  73,   44,
          -9,  17,  19,  53,  37,  69,  18,   22,
         -13,   4,  16,  13,  28,  19,  21,   -8,
         -23,  -9,  12,  10,  19,  17,  25,  -16,
         -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23,
        // clang-format on
    };

    static constexpr PST m_knight_values_eg = {
        // clang-format off
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64,
        // clang-format on
    };

    static constexpr PST m_bishop_values_mg = {
        // clang-format off
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
         -4,   5,  19,  50,  37,  37,   7,  -2,
         -6,  13,  13,  26,  34,  12,  10,   4,
          0,  15,  15,  15,  14,  27,  18,  10,
          4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21,
        // clang-format on
    };

    static constexpr PST m_bishop_values_eg = {
        // clang-format off
        -14, -21, -11,  -8, -7,  -9, -17, -24,
         -8,  -4,   7, -12, -3, -13,  -4, -14,
          2,  -8,   0,  -1, -2,   6,   0,   4,
         -3,   9,  12,   9, 14,  10,   3,   2,
         -6,   3,  13,  19,  7,  10,  -3,  -9,
        -12,  -3,   8,  10, 13,   3,  -7, -15,
        -14, -18,  -7,  -1,  4,  -9, -15, -27,
        -23,  -9, -23,  -5, -9, -16,  -5, -17,
        // clang-format on
    };

    static constexpr PST m_rook_values_mg = {
        // clang-format off
         32,  42,  32,  51, 63,  9,  31,  43,
         27,  32,  58,  62, 80, 67,  26,  44,
         -5,  19,  26,  36, 17, 45,  61,  16,
        -24, -11,   7,  26, 24, 35,  -8, -20,
        -36, -26, -12,  -1,  9, -7,   6, -23,
        -45, -25, -16, -17,  3,  0,  -5, -33,
        -44, -16, -20,  -9, -1, 11,  -6, -71,
        -19, -13,   1,  17, 16,  7, -37, -26,
        // clang-format on
    };

    static constexpr PST m_rook_values_eg = {
        // clang-format off
        13, 10, 18, 15, 12,  12,   8,   5,
        11, 13, 13, 11, -3,   3,   8,   3,
         7,  7,  7,  5,  4,  -3,  -5,  -3,
         4,  3, 13,  1,  2,   1,  -1,   2,
         3,  5,  8,  4, -5,  -6,  -8, -11,
        -4,  0, -5, -1, -7, -12,  -8, -16,
        -6, -6,  0,  2, -9,  -9, -11,  -3,
        -9,  2,  3, -1, -5, -13,   4, -20,
        // clang-format on
    };

    static constexpr PST m_queen_values_mg = {
        // clang-format off
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
         -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
         -1, -18,  -9,  10, -15, -25, -31, -50,
        // clang-format on
    };

    static constexpr PST m_queen_values_eg = {
        // clang-format off
         -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
          3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16, -23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41,
        // clang-format on
    };

    static constexpr PST m_king_values_mg = {
        // clang-format off
        -65,  23,  16, -15, -56, -34,   2,  13,
         29,  -1, -20,  -7,  -8,  -4, -38, -29,
         -9,  24,   2, -16, -20,   6,  22, -22,
        -17, -20, -12, -27, -30, -25, -14, -36,
        -49,  -1, -27, -39, -46, -44, -33, -51,
        -14, -14, -22, -46, -44, -30, -15, -27,
          1,   7,  -8, -64, -43, -16,   9,   8,
        -15,  36,  12, -54,   8, -28,  24,  14,
        // clang-format on
    };

    static constexpr PST m_king_values_eg = {
        // clang-format off
        -74, -35, -18, -18, -11,  15,   4, -17,
        -12,  17,  14,  17,  17,  38,  23,  11,
         10,  17,  23,  15,  20,  45,  44,  13,
         -8,  22,  24,  27,  26,  33,  26,   3,
        -18,  -4,  21,  24,  27,  23,   9, -11,
        -19,  -3,  11,  21,  23,  16,   7,  -9,
        -27, -11,   4,  13,  14,   4,  -5, -17,
        -53, -34, -21, -11, -28, -14, -24, -43
        // clang-format on
    };
};

class PeSTOPhase : public MaterialEval<PeSTOPhase> {
   public:
    constexpr PeSTOPhase(const state::AugmentedState &astate)
        : MaterialEval<PeSTOPhase>(astate) {};

    static constexpr centipawn_t piece_val(const board::Piece p) {
        switch (p) {
            case (board::Piece::PAWN):
                return 0;
            case (board::Piece::KNIGHT):  // NOLINT bugprone-branch-clone
                return 1;
            case (board::Piece::BISHOP):
                return 1;
            case (board::Piece::ROOK):
                return 2;
            case (board::Piece::QUEEN):
                return 4;
            case (board::Piece::KING):
                return 0;
        }
    }
};

// Full evaluation function

static constexpr centipawn_t PeSTOMaxPhase = 24;
using PeSTOIncrementalPhase = PhaseEval<PeSTOPhase, 0, PeSTOMaxPhase>;

template <GamePhase Phase>
using PeSTOIncrementalEval = IncrementalNetPSTEval<PeSTOPSTEval<Phase>>;

using PeSTOEval = TaperedEval<PeSTOIncrementalEval<GamePhase::MIDGAME>,
                              PeSTOIncrementalEval<GamePhase::ENDGAME>,
                              PeSTOIncrementalPhase>;

static_assert(IncrementallyUpdateableEvaluator<PeSTOEval>);

//----------------------------------------------------------------------------//
// Current recommended evaluation function.
//----------------------------------------------------------------------------//

using DefaultEval = PeSTOEval;

}  // namespace eval

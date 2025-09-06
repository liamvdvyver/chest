//============================================================================//
// Pseudo-legal move generation:
//
// All moves are legal,
// with exceptions that they may result in check, or capture an enemy piece.
//
// Templated classes provide functions for loud/quiet/all move generation
// (loud moves are tactical moves, i.e. result in material change).
//
// TODO: rewrite the whole header using CRTP.
//
//============================================================================//

#pragma once

#include <tuple>

#include "attack.h"
#include "board.h"
#include "move.h"
#include "state.h"
#include "util.h"

namespace move::movegen {

//============================================================================//
// Concepts
//============================================================================//

// Staged move generation for multiple attackers in a set.
// The primary level-interface used for move generation.
template <typename T>
concept StagedMoveGenerator =
    requires(T t, const state::AugmentedState &astate, MoveBuffer &moves) {
        { t.get_quiet_moves(astate, moves) } -> std::same_as<void>;
        { t.get_loud_moves(astate, moves) } -> std::same_as<void>;
    };

// Unstaged move generation.
template <typename T>
concept OneshotMoveGenerator =
    requires(T t, const state::AugmentedState &astate, MoveBuffer &moves) {
        { t.get_all_moves(astate, moves) } -> std::same_as<void>;
    };

// Finds (loud/quiet) moves for a single attacker.
template <typename T>
concept OnePieceMoveGenerator =
    requires(T t, const state::AugmentedState &astate, MoveBuffer &moves,
             board::Bitboard singleton) {
        { t.get_quiet_moves(astate, moves, singleton) } -> std::same_as<void>;
        { t.get_loud_moves(astate, moves, singleton) } -> std::same_as<void>;
        { t.attackers(astate) } -> std::same_as<board::Bitboard>;
    };

// Is a square (belonging to a player) attacked by their opponent?
template <typename T>
concept AttackDetector = requires(T t, const state::AugmentedState &astate,
                                  board::Square sq, board::Colour colour) {
    { t.is_attacked(astate, sq, colour) } -> std::same_as<bool>;
};

//============================================================================//
// CRTP helpers
//============================================================================//

// Gets all pieces of a certain type for the side to move.
template <board::Piece Piece>
class WithAttackers {
   public:
    constexpr static board::Bitboard attackers(
        const state::AugmentedState &astate) {
        return astate.state.copy_bitboard({astate.state.to_move, Piece});
    }
};

// Gets loud, then quiet moves.
template <typename T>
class WithAllMoves {
    WithAllMoves() = default;

   public:
    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves) const {
        static_assert(StagedMoveGenerator<T>);
        static_cast<const T *>(this)->get_loud_moves(astate, moves);
        static_cast<const T *>(this)->get_quiet_moves(astate, moves);
    }
    friend T;
};

//============================================================================//
// Adaptor templates
//============================================================================//

//----------------------------------------------------------------------------//
// Attack generators -> One piece move generators:
// Calculate moves/captures given singleton bitboards.
//----------------------------------------------------------------------------//

template <attack::Attacker TAttacker, board::Piece Piece>
class AttackerAdaptor : public WithAttackers<Piece> {
   public:
    AttackerAdaptor(const TAttacker &attacker) : m_attacker(attacker) {};

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves,
                                   const board::Bitboard origin) const {
        const board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from);
        attacked = attacked.setdiff(astate.total_occupancy);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.push_back({move::Move(from, dest.single_bitscan_forward(),
                                        MoveType::NORMAL),
                             Piece});
        }
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  MoveBuffer &moves,
                                  const board::Bitboard origin) const {
        const board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from);
        attacked &= astate.opponent_occupancy();

        for (board::Bitboard dest : attacked.singletons()) {
            moves.push_back({move::Move(from, dest.single_bitscan_forward(),
                                        MoveType::CAPTURE),
                             Piece});
        }
    };

   private:
    const TAttacker &m_attacker;
};
static_assert(OnePieceMoveGenerator<
              AttackerAdaptor<attack::KingAttacker, board::Piece::KING>>);

template <attack::SlidingAttacker TAttacker, board::Piece Piece>
class SlidingSingletonMoverAdaptor : public WithAttackers<Piece> {
   public:
    SlidingSingletonMoverAdaptor(const TAttacker &attacker)
        : m_attacker(attacker) {};

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves,
                                   board::Bitboard origin) const {
        const board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from, astate.total_occupancy);
        attacked = attacked.setdiff(astate.total_occupancy);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.push_back({move::Move(from, dest.single_bitscan_forward(),
                                        MoveType::NORMAL),
                             Piece});
        }
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  MoveBuffer &moves,
                                  board::Bitboard origin) const {
        const board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from, astate.total_occupancy);
        attacked &= astate.opponent_occupancy();

        for (board::Bitboard dest : attacked.singletons()) {
            moves.push_back({move::Move(from, dest.single_bitscan_forward(),
                                        MoveType::CAPTURE),
                             Piece});
        }
    };

   private:
    const TAttacker &m_attacker;
};
static_assert(OnePieceMoveGenerator<SlidingSingletonMoverAdaptor<
                  attack::RookAttacker, board::Piece::ROOK>>);

//----------------------------------------------------------------------------//
// Multi-piece move generator
//----------------------------------------------------------------------------//

// Loops over all pieces of a given type for the active player.
template <OnePieceMoveGenerator TMover>
class LoopingMultiMover : public WithAllMoves<LoopingMultiMover<TMover>> {
   public:
    LoopingMultiMover(const TMover mover) : m_mover(mover) {};

    // Add quiet moves to the moves list
    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves) const {
        for (board::Bitboard b : m_mover.attackers(astate).singletons()) {
            m_mover.get_quiet_moves(astate, moves, b);
        }
    }

    // Add tactical moves to the moves list
    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  MoveBuffer &moves) const {
        for (board::Bitboard b : m_mover.attackers(astate).singletons()) {
            m_mover.get_loud_moves(astate, moves, b);
        }
    }

   protected:
    const TMover m_mover;
};

//============================================================================//
// Concrete instances
//============================================================================//

//----------------------------------------------------------------------------//
// Jumping pieces
//----------------------------------------------------------------------------//

using KingMover = LoopingMultiMover<
    AttackerAdaptor<attack::KingAttacker, board::Piece::KING>>;
using KnightMover = LoopingMultiMover<
    AttackerAdaptor<attack::KnightAttacker, board::Piece::KNIGHT>>;

static_assert(StagedMoveGenerator<KingMover>);
static_assert(StagedMoveGenerator<KnightMover>);

//----------------------------------------------------------------------------//
// Sliding pieces
//----------------------------------------------------------------------------//

using BishopMover = LoopingMultiMover<
    SlidingSingletonMoverAdaptor<attack::BishopAttacker, board::Piece::BISHOP>>;
static_assert(StagedMoveGenerator<BishopMover>);

using RookMoverNoCastles = LoopingMultiMover<
    SlidingSingletonMoverAdaptor<attack::RookAttacker, board::Piece::ROOK>>;
static_assert(StagedMoveGenerator<RookMoverNoCastles>);

using BishopMover = LoopingMultiMover<
    SlidingSingletonMoverAdaptor<attack::BishopAttacker, board::Piece::BISHOP>>;
static_assert(StagedMoveGenerator<BishopMover>);

using DiagQueenMover = LoopingMultiMover<
    SlidingSingletonMoverAdaptor<attack::BishopAttacker, board::Piece::QUEEN>>;
static_assert(StagedMoveGenerator<DiagQueenMover>);

using HorizQueenMover = LoopingMultiMover<
    SlidingSingletonMoverAdaptor<attack::RookAttacker, board::Piece::QUEEN>>;
static_assert(StagedMoveGenerator<HorizQueenMover>);

//============================================================================//
// Special pieces: require more specific logic
//============================================================================//

//----------------------------------------------------------------------------//
// Pawn moves: three different kinds
//----------------------------------------------------------------------------//

template <attack::ColouredAttacker TSinglePusher,
          attack::ColouredAttacker TDoublePusher,
          attack::ColouredAttacker TAttacker>
class PawnMoveGenerator
    : public WithAttackers<board::Piece::PAWN>,
      public WithAllMoves<
          PawnMoveGenerator<TSinglePusher, TDoublePusher, TAttacker>> {
   public:
    PawnMoveGenerator(const TSinglePusher &single_pusher,
                      const TDoublePusher &double_pusher,
                      const TAttacker &attacker)
        : m_single_pusher(single_pusher),
          m_double_pusher(double_pusher),
          m_attacker(attacker) {};

    // (consider promotions to be loud)
    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves,
                                   board::Bitboard origin) const {
        board::Square from = origin.single_bitscan_forward();
        get_single_pushes(moves, astate.total_occupancy, from,
                          astate.state.to_move);
        get_double_pushes(moves, astate.total_occupancy, from,
                          astate.state.to_move);
    }

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  MoveBuffer &moves,
                                  board::Bitboard origin) const {
        board::Square from = origin.single_bitscan_forward();
        get_captures(moves, astate, astate.opponent_occupancy(), from,
                     astate.state.to_move);
    }

   private:
    const TSinglePusher &m_single_pusher;
    const TDoublePusher &m_double_pusher;
    const TAttacker &m_attacker;

    constexpr void get_single_pushes(MoveBuffer &moves,
                                     board::Bitboard occ_total,
                                     board::Square from,
                                     board::Colour to_move) const {
        board::Bitboard push_dest = m_single_pusher(from, to_move);
        if (push_dest & occ_total) {
            return;
        }

        // Pawns are always (single) pushable, ignoring blockers
        board::Square to = push_dest.single_bitscan_forward();

        // Promotions/normal push
        if (push_dest & back_rank_mask(to_move)) {
            moves.push_back({move::Move(from, to, MoveType::PROMOTE_QUEEN),
                             board::Piece::PAWN});
            moves.push_back({move::Move(from, to, MoveType::PROMOTE_ROOK),
                             board::Piece::PAWN});
            moves.push_back({move::Move(from, to, MoveType::PROMOTE_BISHOP),
                             board::Piece::PAWN});
            moves.push_back({move::Move(from, to, MoveType::PROMOTE_KNIGHT),
                             board::Piece::PAWN});
        } else {
            moves.push_back({move::Move(from, to, MoveType::SINGLE_PUSH),
                             board::Piece::PAWN});
        }
    };

    constexpr void get_double_pushes(MoveBuffer &moves,
                                     board::Bitboard occ_total,
                                     board::Square from,
                                     board::Colour to_move) const {
        board::Bitboard push_dest = m_double_pusher(from, to_move);
        board::Bitboard jump_dest = m_single_pusher(from, to_move);
        if (push_dest.empty() | ((push_dest ^ jump_dest) & occ_total)) {
            return;
        }

        moves.push_back({move::Move(from, push_dest.single_bitscan_forward(),
                                    MoveType::DOUBLE_PUSH),
                         board::Piece::PAWN});
    };

    constexpr void get_captures(MoveBuffer &moves,
                                const state::AugmentedState &astate,
                                board::Bitboard occ_opponent,
                                board::Square from,
                                board::Colour to_move) const {
        board::Bitboard capture_dests = m_attacker(from, to_move);

        const board::Bitboard ep_bb =
            astate.state.ep_square.has_value()
                ? board::Bitboard(astate.state.ep_square.value())
                : 0;
        capture_dests &= (occ_opponent | ep_bb);

        if (capture_dests.empty()) {
            return;
        }

        if (capture_dests & back_rank_mask(to_move)) {
            for (board::Bitboard target : capture_dests.singletons()) {
                moves.push_back(
                    {move::Move(from, target.single_bitscan_forward(),
                                MoveType::PROMOTE_CAPTURE_QUEEN),
                     board::Piece::PAWN});
                moves.push_back(
                    {move::Move(from, target.single_bitscan_forward(),
                                MoveType::PROMOTE_CAPTURE_ROOK),
                     board::Piece::PAWN});
                moves.push_back(
                    {move::Move(from, target.single_bitscan_forward(),
                                MoveType::PROMOTE_CAPTURE_BISHOP),
                     board::Piece::PAWN});
                moves.push_back(
                    {move::Move(from, target.single_bitscan_forward(),
                                MoveType::PROMOTE_CAPTURE_KNIGHT),
                     board::Piece::PAWN});
            }
        } else {
            for (board::Bitboard target : capture_dests.singletons()) {
                MoveType type = (target & ep_bb).empty() ? MoveType::CAPTURE
                                                         : MoveType::CAPTURE_EP;
                moves.push_back(
                    {move::Move(from, target.single_bitscan_forward(), type),
                     board::Piece::PAWN});
            }
        }
    }

    static constexpr board::Bitboard back_rank_mask(board::Colour c) {
        return board::Bitboard::rank_mask(board::ranks::back_rank(c));
    }
};

using SingletonPawnMover =
    PawnMoveGenerator<attack::PawnSinglePusher, attack::PawnDoublePusher,
                      attack::PawnAttacker>;
using PawnMover = LoopingMultiMover<SingletonPawnMover>;
static_assert(StagedMoveGenerator<PawnMover>);

//----------------------------------------------------------------------------//
// Rook moves: add castling
//----------------------------------------------------------------------------//

// Given a MultiMoveGenerator which gets rook moves,
// add castling to quiet move generation move generation
template <StagedMoveGenerator TRookMover>
class CastlingRookMover : TRookMover {
   public:
    using TRookMover::get_loud_moves;
    using TRookMover::TRookMover;

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves) const {
        get_castles(astate, moves, astate.total_occupancy);
        TRookMover::get_quiet_moves(astate, moves);
    }

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves) const {
        TRookMover::get_all_moves(astate, moves);
        get_castles(astate, moves, astate.total_occupancy);
    }

   private:
    // Assumes that castling rights are set correctly,
    // i.e. doesn't check king position, rook positions.
    void get_castles(const state::AugmentedState &astate, MoveBuffer &moves,
                     board::Bitboard total_occ) const {
        for (board::Piece side : state::CastlingInfo::castling_sides) {
            board::ColouredPiece cp = {astate.state.to_move, side};
            if (astate.state.castling_rights.get_square_rights(cp)) {
                board::Bitboard rk_mask =
                    state::CastlingInfo::get_rook_mask(cp);
                board::Bitboard blockers = rk_mask & total_occ;
                if (blockers.empty()) {
                    moves.push_back(
                        {move::Move(state::CastlingInfo::get_rook_start(cp),
                                    state::CastlingInfo::get_king_start(
                                        astate.state.to_move),
                                    MoveType::CASTLE),
                         side});
                }
            }
        }
    };
};

using RookMover = CastlingRookMover<RookMoverNoCastles>;
static_assert(StagedMoveGenerator<RookMover>);

//============================================================================//
// All move generation
//============================================================================//

// Gets all the legal moves in a position, performs attack detection.
template <bool InOrder = false>
class AllMoveGenerator {
   public:
    constexpr AllMoveGenerator() = default;

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves) const {
        apply_tuple([&](auto &mover) { mover.get_quiet_moves(astate, moves); },
                    movers);
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  MoveBuffer &moves) const {
        apply_tuple([&](auto &mover) { mover.get_loud_moves(astate, moves); },
                    movers);
    };

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves) const {
        if constexpr (InOrder) {
            return get_all_moves_ordered(astate, moves);
        } else {
            return get_all_moves_defer(astate, moves);
        }
    }

    constexpr bool is_attacked(const state::AugmentedState &astate,
                               const board::Square sq,
                               const board::Colour colour) const {
        return (m_pawn_attacker(sq, colour) &
                astate.state.copy_bitboard({!colour, board::Piece::PAWN})) ||
               (m_knight_attacker(sq) &
                astate.state.copy_bitboard({!colour, board::Piece::KNIGHT})) ||
               (m_bishop_attacker(sq, astate.total_occupancy) &
                (astate.state.copy_bitboard({!colour, board::Piece::BISHOP}) |
                 astate.state.copy_bitboard({!colour, board::Piece::QUEEN}))) ||
               (m_rook_attacker(sq, astate.total_occupancy) &
                (astate.state.copy_bitboard({!colour, board::Piece::ROOK}) |
                 astate.state.copy_bitboard({!colour, board::Piece::QUEEN}))) ||
               (m_king_attacker(sq) &
                (astate.state.copy_bitboard({!colour, board::Piece::KING})));
    }

   private:
    // Gets all moves, loud moves guaranteed first.
    constexpr void get_all_moves_ordered(const state::AugmentedState &astate,
                                         MoveBuffer &moves) const {
        get_loud_moves(astate, moves);
        get_quiet_moves(astate, moves);
    };

    // Gets all moves, no guarantees on move ordering,
    // move ordering is deferred to members -> better memory access pattern
    constexpr void get_all_moves_defer(const state::AugmentedState &astate,
                                       MoveBuffer &moves) const {
        apply_tuple([&](auto &mover) { mover.get_all_moves(astate, moves); },
                    movers);
    };

    // Hold instances of Attackers
    attack::PawnAttacker m_pawn_attacker;
    attack::PawnSinglePusher m_pawn_single_pusher;
    attack::PawnDoublePusher m_pawn_double_pusher;
    attack::KnightAttacker m_knight_attacker;
    attack::BishopAttacker m_bishop_attacker;
    attack::RookAttacker m_rook_attacker;
    attack::KingAttacker m_king_attacker;

    // Movers in tuple for iteration
    std::tuple<PawnMover, KnightMover, BishopMover, RookMover, KingMover,
               DiagQueenMover, HorizQueenMover>
        movers = {
            {{m_pawn_single_pusher, m_pawn_double_pusher, m_pawn_attacker}},
            {m_knight_attacker},
            {m_bishop_attacker},
            {m_rook_attacker},
            {m_king_attacker},
            {m_bishop_attacker},
            {m_rook_attacker},
    };
};

static_assert(StagedMoveGenerator<AllMoveGenerator<true>>);
static_assert(OneshotMoveGenerator<AllMoveGenerator<true>>);
static_assert(AttackDetector<AllMoveGenerator<true>>);

}  // namespace move::movegen

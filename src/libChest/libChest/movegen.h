#ifndef MOVEGEN_H
#define MOVEGEN_H

#include <concepts>

#include "attack.h"
#include "board.h"
#include "move.h"
#include "movebuffer.h"
#include "state.h"

//
// Perform (pseudo-legal) move generation for one piece type x state.
// I.e. all moves must be legal, except may result in check, or enemy king
// capture. For now, when we expand a node, we can just see if we are in check.
//
// TODO: test legal move generation.
//
// Basic structure: provide classes which have separate funtions for:
//
// * all move generation
// * loud (capture, check, promotion) move generation
// * quiet move generation
// * check evasion
//
// full augmented state struct may be used.
//
// TODO: test whether staged generation is faster than all move generation +
// sorting.
//

namespace move::movegen {

// High level interfaces

// Staged move generation for multiple attackers in a set.
// The primary level-interface used for move generation.
template <typename T>
concept MultiPieceMoveGenerator =
    requires(T t, const state::AugmentedState &astate, MoveBuffer &moves) {
        { t.get_quiet_moves(astate, moves) } -> std::same_as<void>;
        { t.get_loud_moves(astate, moves) } -> std::same_as<void>;
    };

// Unstaged move generation.
// Currently not heavily, but there is tradeoff in the design of further
// template classes:
// * Can dispatch (cached) loud move generation, than quiet -> decent default
//   move ordering (chosen for now)
// * Can dispatch unstaged (or loud then quiet) generation per attacker,
//   to avoid cache misses. (also chosen for now)
// * Can perform one-shot move generation to avoid repeated lookups (not
//   chosen).
// TODO: test the performance implications.
template <typename T>
concept OneshotMoveGenerator =
    requires(T t, const state::AugmentedState &astate, MoveBuffer &moves) {
        { t.get_all_moves(astate, moves) } -> std::same_as<void>;
    };

// Find (loud/quiet) moves for a single attacker
template <typename T>
concept OnePieceMoveGenerator =
    requires(T t, const state::AugmentedState &astate, MoveBuffer &moves,
             board::Bitboard singleton) {
        { t.get_quiet_moves(astate, moves, singleton) } -> std::same_as<void>;
        { t.get_loud_moves(astate, moves, singleton) } -> std::same_as<void>;
        { t.get_all_moves(astate, moves, singleton) } -> std::same_as<void>;
    };

// Find attackers given state
template <typename T>
concept HasAttackers = requires(T t, const state::AugmentedState &astate) {
    { t.attackers(astate) } -> std::same_as<board::Bitboard>;
};

// Find the attackers given state, and moves given attacker
template <typename T>
concept PiecewiseMoveGenerator =
    requires { requires OnePieceMoveGenerator<T> && HasAttackers<T>; };

// Is a square (belonging to a player) attacked by their opponent?
template <typename T>
concept AttackDetector = requires(T t, const state::AugmentedState &astate,
                                  board::Square sq, board::Colour colour) {
    { t.is_attacked(astate, sq, colour) } -> std::same_as<bool>;
};

// Able to generate pseudo-legal moves, i.e. generte moves and detect checks
template <typename T>
concept PseudolegalGenerator =
    requires { requires OneshotMoveGenerator<T> && AttackDetector<T>; };

//
// Template adaptors provided to create concrete MultiMoveGenerators
//

// Given an Attacker, create a SingletonMoverGenerator
template <attack::Attacker TAttacker, board::Piece Piece>
class SingletonMoverAdaptor {
   public:
    SingletonMoverAdaptor(const TAttacker &attacker) : m_attacker(attacker) {};

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves,
                                   board::Bitboard origin) const {
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
                                  board::Bitboard origin) const {
        const board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from);
        attacked &= astate.opponent_occupancy();

        for (board::Bitboard dest : attacked.singletons()) {
            moves.push_back({move::Move(from, dest.single_bitscan_forward(),
                                        MoveType::CAPTURE),
                             Piece});
        }
    };

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves,
                                 board::Bitboard origin) const {
        get_loud_moves(astate, moves, origin);
        get_quiet_moves(astate, moves, origin);
    }

   private:
    const TAttacker &m_attacker;
};
static_assert(OnePieceMoveGenerator<
              SingletonMoverAdaptor<attack::KingAttacker, board::Piece::KING>>);

// Given a SlidingAttacker, create a SingletonMoveGenerator
template <attack::SlidingAttacker TAttacker, board::Piece Piece>
class SlidingSingletonMoverAdaptor {
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

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves,
                                 board::Bitboard origin) const {
        get_loud_moves(astate, moves, origin);
        get_quiet_moves(astate, moves, origin);
    }

   private:
    const TAttacker &m_attacker;
};
static_assert(OnePieceMoveGenerator<SlidingSingletonMoverAdaptor<
                  attack::RookAttacker, board::Piece::ROOK>>);

// Add attacker() to get all pieces of a type for the player to move
template <board::Piece piece, OnePieceMoveGenerator TMover>
class PiecewiseAdaptor : public TMover {
    using TMover::TMover;

   public:
    constexpr static board::Bitboard attackers(
        const state::AugmentedState &astate) {
        return astate.state.copy_bitboard({astate.state.to_move, piece});
    }
};

// Add attacker() to get all pieces of a type and queens for the player to move
template <board::Piece piece, OnePieceMoveGenerator TMover>
class SlidingPiecewiseAdaptor : public TMover {
    using TMover::TMover;

   public:
    constexpr static board::Bitboard attackers(
        const state::AugmentedState &astate) {
        return astate.state.copy_bitboard({astate.state.to_move, piece});
        //|
        // astate.state.copy_bitboard(
        //     {astate.state.to_move, board::Piece::QUEEN});
    }
};

// Given a piece, and SingletonMoverGenerator for the piece, create a
// PiecewiseMoveGenerator, which loops over all pieces of the same type for the
// active player.
template <PiecewiseMoveGenerator TMover>
class MultiMoverAdaptor {
   public:
    MultiMoverAdaptor(TMover mover) : m_mover(mover) {};

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

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves) const {
        for (board::Bitboard b : m_mover.attackers(astate).singletons()) {
            m_mover.get_all_moves(astate, moves, b);
        }
    }

   protected:
    TMover m_mover;
};

// Jumping

using KingMover = MultiMoverAdaptor<PiecewiseAdaptor<
    board::Piece::KING,
    SingletonMoverAdaptor<attack::KingAttacker, board::Piece::KING>>>;
using KnightMover = MultiMoverAdaptor<PiecewiseAdaptor<
    board::Piece::KNIGHT,
    SingletonMoverAdaptor<attack::KnightAttacker, board::Piece::KNIGHT>>>;

static_assert(MultiPieceMoveGenerator<KingMover>);
static_assert(MultiPieceMoveGenerator<KnightMover>);

// Sliding

using BishopMover = MultiMoverAdaptor<SlidingPiecewiseAdaptor<
    board::Piece::BISHOP, SlidingSingletonMoverAdaptor<attack::BishopAttacker,
                                                       board::Piece::BISHOP>>>;
static_assert(MultiPieceMoveGenerator<BishopMover>);

using DiagQueenMover = MultiMoverAdaptor<SlidingPiecewiseAdaptor<
    board::Piece::QUEEN,
    SlidingSingletonMoverAdaptor<attack::BishopAttacker, board::Piece::QUEEN>>>;
using HorizQueenMover = MultiMoverAdaptor<SlidingPiecewiseAdaptor<
    board::Piece::QUEEN,
    SlidingSingletonMoverAdaptor<attack::RookAttacker, board::Piece::QUEEN>>>;
static_assert(MultiPieceMoveGenerator<HorizQueenMover>);

//
// Pawn moves
//

// Template over ColouredAttackers to inject pawn attack generation at compile
// time. Hardcodes pawn specific logic given the three modes of movement for
// pawns.
template <attack::ColouredAttacker TSinglePusher,
          attack::ColouredAttacker TDoublePusher,
          attack::ColouredAttacker TAttacker>
class PawnMoveGenerator {
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

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves,
                                 board::Bitboard origin) const {
        get_loud_moves(astate, moves, origin);
        get_quiet_moves(astate, moves, origin);
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
        return board::Bitboard::rank_mask(board::back_rank[(int)c]);
    }
};

using PawnMover = MultiMoverAdaptor<PiecewiseAdaptor<
    board::Piece::PAWN,
    PawnMoveGenerator<attack::PawnSinglePusher, attack::PawnDoublePusher,
                      attack::PawnAttacker>>>;
static_assert(MultiPieceMoveGenerator<PawnMover>);

// Given a MultiMoveGenerator which gets rook moves,
// add castling to quiet move generation move generation
template <MultiPieceMoveGenerator TMover>
class RookMoverFactory : TMover {
   public:
    using TMover::get_all_moves;
    using TMover::get_loud_moves;
    using TMover::TMover;

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves) const {
        get_castles(astate, moves, astate.total_occupancy);
        TMover::get_quiet_moves(astate, moves);
    }

    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 MoveBuffer &moves) const {
        TMover::get_all_moves(astate, moves);
        get_castles(astate, moves, astate.total_occupancy);
    }

   private:
    // Assumes that castling rights are set correctly,
    // i.e. doesn't check king position, rook positions.
    void get_castles(const state::AugmentedState &astate, MoveBuffer &moves,
                     board::Bitboard total_occ) const {
        for (board::Piece side : state::CastlingInfo::castling_sides) {
            board::ColouredPiece cp = {astate.state.to_move, side};
            if (astate.state.get_castling_rights(cp)) {
                board::Bitboard rk_mask =
                    astate.castling_info.get_rook_mask(cp);
                board::Bitboard blockers = rk_mask & total_occ;
                if (blockers.empty()) {
                    moves.push_back(
                        {move::Move(astate.castling_info.get_rook_start(cp),
                                    astate.castling_info.get_king_start(
                                        astate.state.to_move),
                                    MoveType::CASTLE),
                         side});
                }
            }
        }
    };
};

using RookMover = RookMoverFactory<MultiMoverAdaptor<SlidingPiecewiseAdaptor<
    board::Piece::ROOK,
    SlidingSingletonMoverAdaptor<attack::RookAttacker, board::Piece::ROOK>>>>;
static_assert(MultiPieceMoveGenerator<RookMover>);

//
// All move generation
//

// Gets all the legal moves in a position,
// composes all neccessary MultiMoveGenerator specialisers.
// Also performs check detection.
template <bool InOrder = false>
class AllMoveGenerator {
   public:
    constexpr AllMoveGenerator()
        : m_pawn_attacker(),
          m_pawn_single_pusher(),
          m_pawn_double_pusher(),
          m_knight_attacker(),
          m_bishop_attacker(),
          m_rook_attacker(),
          m_king_attacker(),
          m_pawn_mover(
              {m_pawn_single_pusher, m_pawn_double_pusher, m_pawn_attacker}),
          m_knight_mover(m_knight_attacker),
          m_bishop_mover(m_bishop_attacker),
          m_rook_mover(m_rook_attacker),
          m_king_mover(m_king_attacker),
          m_d_queen_mover(m_bishop_attacker),
          m_h_queen_mover(m_rook_attacker) {}

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   MoveBuffer &moves) const {
        m_pawn_mover.get_quiet_moves(astate, moves);
        m_knight_mover.get_quiet_moves(astate, moves);
        m_bishop_mover.get_quiet_moves(astate, moves);
        m_rook_mover.get_quiet_moves(astate, moves);
        m_king_mover.get_quiet_moves(astate, moves);
        m_h_queen_mover.get_quiet_moves(astate, moves);
        m_d_queen_mover.get_quiet_moves(astate, moves);
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  MoveBuffer &moves) const {
        m_pawn_mover.get_loud_moves(astate, moves);
        m_knight_mover.get_loud_moves(astate, moves);
        m_bishop_mover.get_loud_moves(astate, moves);
        m_rook_mover.get_loud_moves(astate, moves);
        m_king_mover.get_loud_moves(astate, moves);
        m_h_queen_mover.get_loud_moves(astate, moves);
        m_d_queen_mover.get_loud_moves(astate, moves);
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
                               board::Square sq, board::Colour colour) const {
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
        m_pawn_mover.get_all_moves(astate, moves);
        m_knight_mover.get_all_moves(astate, moves);
        m_bishop_mover.get_all_moves(astate, moves);
        m_d_queen_mover.get_all_moves(astate, moves);
        m_rook_mover.get_all_moves(astate, moves);
        m_h_queen_mover.get_all_moves(astate, moves);
        m_king_mover.get_all_moves(astate, moves);
    };

    // Hold instances of Attackers
    attack::PawnAttacker m_pawn_attacker;
    attack::PawnSinglePusher m_pawn_single_pusher;
    attack::PawnDoublePusher m_pawn_double_pusher;
    attack::KnightAttacker m_knight_attacker;
    attack::BishopAttacker m_bishop_attacker;
    attack::RookAttacker m_rook_attacker;
    attack::KingAttacker m_king_attacker;

    // Create instances of MoveGenerators
    PawnMover m_pawn_mover;
    KnightMover m_knight_mover;
    BishopMover m_bishop_mover;
    RookMover m_rook_mover;
    KingMover m_king_mover;
    DiagQueenMover m_d_queen_mover;
    HorizQueenMover m_h_queen_mover;
};

static_assert(MultiPieceMoveGenerator<AllMoveGenerator<true>>);
static_assert(OneshotMoveGenerator<AllMoveGenerator<true>>);
static_assert(AttackDetector<AllMoveGenerator<true>>);

}  // namespace move::movegen

#endif

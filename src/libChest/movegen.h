#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "attack.h"
#include "board.h"
#include "move.h"
#include "state.h"
#include <vector>

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
concept MultiMoveGenerator = requires(T t, const state::AugmentedState &astate,
                                      std::vector<move::Move> &moves) {
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
concept OneshotMoveGenerator = requires(
    T t, const state::AugmentedState &astate, std::vector<move::Move> &moves) {
    { t.get_all_moves(astate, moves) } -> std::same_as<void>;
};

// Find (loud/quiet) moves for a single attacker
template <typename T>
concept SingletonMoveGenerator =
    requires(T t, const state::AugmentedState &astate,
             std::vector<move::Move> &moves, board::Bitboard singleton) {
        { t.get_quiet_moves(astate, moves, singleton) } -> std::same_as<void>;
        { t.get_loud_moves(astate, moves, singleton) } -> std::same_as<void>;
    };

// Find attackers given state
template <typename T>
concept HasAttackers = requires(T t, const state::AugmentedState &astate) {
    { t.attackers(astate) } -> std::same_as<board::Bitboard>;
};

// Find the attackers given state, and moves given attacker
template <typename T>
concept PiecewiseMoveGenerator =
    requires { requires SingletonMoveGenerator<T> && HasAttackers<T>; };

//
// Template factories provided to create concrete MultiMoveGenerators
//

// Given an Attacker, create a SingletonMoverGenerator
template <attack::Attacker TAttacker> class SingletonMoverFactory {

  public:
    SingletonMoverFactory() : m_attacker() {};

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   std::vector<move::Move> &moves,
                                   board::Bitboard origin) {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked =
            m_attacker(from).setdiff(astate.total_occupancy);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::NORMAL);
        }
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  std::vector<move::Move> &moves,
                                  board::Bitboard origin) {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from);
        attacked &= astate.opponent_occupancy();

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::CAPTURE);
        }
    };

  private:
    TAttacker m_attacker;
};
static_assert(
    SingletonMoveGenerator<SingletonMoverFactory<attack::KingAttacker>>);

// Given a SlidingAttacker, create a SingletonMoveGenerator
template <attack::SlidingAttacker TAttacker>
class SlidingSingletonMoverFactory {

  public:
    SlidingSingletonMoverFactory() : m_attacker() {};

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   std::vector<move::Move> &moves,
                                   board::Bitboard origin) {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from, astate.total_occupancy)
                                       .setdiff(astate.total_occupancy);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::NORMAL);
        }
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  std::vector<move::Move> &moves,
                                  board::Bitboard origin) {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker(from, astate.total_occupancy);
        attacked &= astate.opponent_occupancy();

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::CAPTURE);
        }
    };

  private:
    TAttacker m_attacker;
};
static_assert(
    SingletonMoveGenerator<SlidingSingletonMoverFactory<attack::RookAttacker>>);

// Add attacker() to get all pieces of a type for the player to move
template <board::Piece piece, SingletonMoveGenerator TMover>
class PiecewiseFactory : public TMover {
    using TMover::TMover;

  public:
    constexpr static board::Bitboard
    attackers(const state::AugmentedState &astate) {
        return astate.state.copy_bitboard(piece, astate.state.to_move);
    }
};

// Add attacker() to get all pieces of a type and queens for the player to move
template <board::Piece piece, SingletonMoveGenerator TMover>
class SlidingPiecewiseFactory : public TMover {
    using TMover::TMover;

  public:
    constexpr static board::Bitboard
    attackers(const state::AugmentedState &astate) {
        return astate.state.copy_bitboard(piece, astate.state.to_move) |
               astate.state.copy_bitboard(board::Piece::QUEEN,
                                          astate.state.to_move);
    }
};

// Given a piece, and SingletonMoverGenerator for the piece, create a
// PiecewiseMoveGenerator, which loops over all pieces of the same type for the
// active player.
template <PiecewiseMoveGenerator TMover> class MultiMoverFactory {

  public:
    MultiMoverFactory() : m_mover() {};

    // Add quiet moves to the moves list
    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   std::vector<move::Move> &moves) {
        for (board::Bitboard b : m_mover.attackers(astate).singletons()) {
            m_mover.get_quiet_moves(astate, moves, b);
        }
    }

    // Add tactical moves to the moves list
    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  std::vector<move::Move> &moves) {
        for (board::Bitboard b : m_mover.attackers(astate).singletons()) {
            m_mover.get_loud_moves(astate, moves, b);
        }
    }

  protected:
    TMover m_mover;
};

// Jumping

typedef MultiMoverFactory<PiecewiseFactory<
    board::Piece::KING, SingletonMoverFactory<attack::KingAttacker>>>
    KingMover;
typedef MultiMoverFactory<PiecewiseFactory<
    board::Piece::KNIGHT, SingletonMoverFactory<attack::KnightAttacker>>>
    KnightMover;

static_assert(MultiMoveGenerator<KingMover>);
static_assert(MultiMoveGenerator<KnightMover>);

// Sliding

typedef MultiMoverFactory<SlidingPiecewiseFactory<
    board::Piece::BISHOP, SlidingSingletonMoverFactory<attack::BishopAttacker>>>
    BishopMover;
static_assert(MultiMoveGenerator<BishopMover>);

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
    PawnMoveGenerator() : m_single_pusher(), m_double_pusher(), m_attacker() {};

    // (consider promotions to be loud)
    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   std::vector<move::Move> &moves,
                                   board::Bitboard origin) {

        board::Square from = origin.single_bitscan_forward();
        get_single_pushes(moves, astate.total_occupancy, from,
                          astate.state.to_move);
        get_double_pushes(moves, astate.total_occupancy, from,
                          astate.state.to_move);
    }

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  std::vector<move::Move> &moves,
                                  board::Bitboard origin) {

        board::Square from = origin.single_bitscan_forward();
        get_captures(moves, astate, astate.total_occupancy, from,
                     astate.state.to_move);
    }

  private:
    TSinglePusher m_single_pusher;
    TDoublePusher m_double_pusher;
    TAttacker m_attacker;

    constexpr void get_single_pushes(std::vector<move::Move> &moves,
                                     board::Bitboard occ_total,
                                     board::Square from,
                                     board::Colour to_move) {
        board::Bitboard push_dest = m_single_pusher(from, to_move);
        if (push_dest & occ_total) {
            return;
        }

        // Pawns are always (single) pushable, ignoring blockers
        board::Square to = push_dest.single_bitscan_forward();

        // Promotions/normal push
        if (push_dest & back_rank_mask(to_move)) {
            moves.emplace_back(from, to, MoveType::PROMOTE_QUEEN);
            moves.emplace_back(from, to, MoveType::PROMOTE_ROOK);
            moves.emplace_back(from, to, MoveType::PROMOTE_BISHOP);
            moves.emplace_back(from, to, MoveType::PROMOTE_KNIGHT);
        } else {
            moves.emplace_back(from, to, MoveType::SINGLE_PUSH);
        }
    };

    constexpr void get_double_pushes(std::vector<move::Move> &moves,
                                     board::Bitboard occ_total,
                                     board::Square from,
                                     board::Colour to_move) {

        board::Bitboard push_dest = m_double_pusher(from, to_move);
        if (push_dest.empty() | (push_dest & occ_total)) {
            return;
        }

        moves.emplace_back(from, push_dest.single_bitscan_forward(),
                           MoveType::SINGLE_PUSH);
    };

    constexpr void get_captures(std::vector<move::Move> &moves,
                                const state::AugmentedState &astate,
                                board::Bitboard occ_opponent,
                                board::Square from, board::Colour to_move) {

        board::Bitboard capture_dests = m_attacker(from, to_move);

        const board::Bitboard ep_bb =
            board::Bitboard(astate.state.ep_square.value_or(0));

        capture_dests &= occ_opponent;
        if (capture_dests.empty()) {
            return;
        }

        if (capture_dests & back_rank_mask(to_move)) {
            for (board::Bitboard target : capture_dests.singletons()) {
                moves.emplace_back(from, target.single_bitscan_forward(),
                                   MoveType::PROMOTE_CAPTURE_QUEEN);
                moves.emplace_back(from, target.single_bitscan_forward(),
                                   MoveType::PROMOTE_CAPTURE_ROOK);
                moves.emplace_back(from, target.single_bitscan_forward(),
                                   MoveType::PROMOTE_CAPTURE_BISHOP);
                moves.emplace_back(from, target.single_bitscan_forward(),
                                   MoveType::PROMOTE_CAPTURE_KNIGHT);
            }
        } else {
            for (board::Bitboard target : capture_dests.singletons()) {
                MoveType type = (target & ep_bb).empty() ? MoveType::CAPTURE
                                                         : MoveType::CAPTURE_EP;
                moves.emplace_back(from, target.single_bitscan_forward(), type);
            }
        }
    }

    // Helpers
    static constexpr int back_rank_n(board::Colour c) {
        return (bool)c ? board::board_size - 1 : 0;
    }
    static constexpr board::Bitboard back_rank_mask(board::Colour c) {
        return board::Bitboard::rank_mask(back_rank_n(c));
    }
};

typedef MultiMoverFactory<PiecewiseFactory<
    board::Piece::PAWN,
    PawnMoveGenerator<attack::PawnSinglePusher, attack::PawnDoublePusher,
                      attack::PawnAttacker>>>
    PawnMover;
static_assert(MultiMoveGenerator<PawnMover>);

// Given a MultiMoveGenerator which gets rook moves,
// add castling to quiet move generation move generation
template <MultiMoveGenerator TMover> class RookMoverFactory : TMover {

  public:
    using TMover::get_loud_moves;
    using TMover::TMover;

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   std::vector<move::Move> &moves) {
        get_castles(astate, moves, astate.total_occupancy);
        TMover::get_quiet_moves(astate, moves);
    }

  private:
    // Assumes that castling rights are set correctly,
    // i.e. doesn't check king position, rook positions.
    void get_castles(const state::AugmentedState &astate,
                     std::vector<move::Move> &moves,
                     board::Bitboard total_occ) {

        // Castle positions
        const board::Square w_ks_castle = board::H1;
        const board::Square w_qs_castle = board::A1;
        const board::Square b_ks_castle = board::H8;
        const board::Square b_qs_castle = board::A8;

        // King positions
        const board::Square w_king = board::E1;
        const board::Square b_king = board::E8;

        // Prebaked castle moves
        const move::Move b_ks_move =
            move::Move(b_ks_castle, b_king, MoveType::CASTLE);
        const move::Move b_qs_move =
            move::Move(b_qs_castle, b_king, MoveType::CASTLE);
        const move::Move w_ks_move =
            move::Move(w_ks_castle, w_king, MoveType::CASTLE);
        const move::Move w_qs_move =
            move::Move(w_qs_castle, w_king, MoveType::CASTLE);

        // Masks: squares which must be unobstructed to castle
        static const board::Bitboard w_ks_mask =
            board::Bitboard(board::Square(board::F1)) ^
            board::Bitboard(board::Square(board::G1));
        static const board::Bitboard w_qs_mask =
            board::Bitboard(board::Square(board::B1)) ^
            board::Bitboard(board::Square(board::C1)) ^
            board::Bitboard(board::Square(board::D1));
        static const board::Bitboard b_ks_mask =
            w_ks_mask.shift(0, board::board_size - 1);
        static const board::Bitboard b_qs_mask =
            w_qs_mask.shift(0, board::board_size - 1);

        // Check both sides
        if (astate.state.get_castling_rights(board::Piece::KING,
                                             astate.state.to_move)) {
            board::Bitboard ks_mask =
                (bool)astate.state.to_move ? w_ks_mask : b_ks_mask;
            board::Bitboard ks_blockers = ks_mask & total_occ;
            if (ks_blockers.empty()) {
                moves.push_back((bool)astate.state.to_move ? w_ks_move
                                                           : b_ks_move);
            }
        }

        if (astate.state.get_castling_rights(board::Piece::QUEEN,
                                             astate.state.to_move)) {
            board::Bitboard qs_mask =
                (bool)astate.state.to_move ? w_qs_mask : b_qs_mask;
            board::Bitboard qs_blockers = qs_mask & total_occ;
            if (qs_blockers.empty()) {
                moves.push_back((bool)astate.state.to_move ? w_qs_move
                                                           : b_qs_move);
            }
        }
    };
};

typedef RookMoverFactory<MultiMoverFactory<SlidingPiecewiseFactory<
    board::Piece::ROOK, SlidingSingletonMoverFactory<attack::RookAttacker>>>>
    RookMover;
static_assert(MultiMoveGenerator<RookMover>);

//
// All move generation
//

// Gets all the legal moves in a position,
// composes all neccessary MultiMoveGenerator specialisers.
class AllMoveGenerator {

  public:
    constexpr AllMoveGenerator()
        : m_pawn_mover(), m_knight_mover(), m_bishop_mover(), m_rook_mover(),
          m_king_mover() {}

    constexpr void get_quiet_moves(const state::AugmentedState &astate,
                                   std::vector<move::Move> &moves) {
        m_pawn_mover.get_quiet_moves(astate, moves);
        m_knight_mover.get_quiet_moves(astate, moves);
        m_bishop_mover.get_quiet_moves(astate, moves);
        m_rook_mover.get_quiet_moves(astate, moves);
        m_king_mover.get_quiet_moves(astate, moves);
    };

    constexpr void get_loud_moves(const state::AugmentedState &astate,
                                  std::vector<move::Move> &moves) {
        m_pawn_mover.get_loud_moves(astate, moves);
        m_knight_mover.get_loud_moves(astate, moves);
        m_bishop_mover.get_loud_moves(astate, moves);
        m_rook_mover.get_loud_moves(astate, moves);
        m_king_mover.get_loud_moves(astate, moves);
    };
    constexpr void get_all_moves(const state::AugmentedState &astate,
                                 std::vector<move::Move> &moves) {
        m_pawn_mover.get_loud_moves(astate, moves);
        m_pawn_mover.get_quiet_moves(astate, moves);
        m_knight_mover.get_loud_moves(astate, moves);
        m_knight_mover.get_quiet_moves(astate, moves);
        m_bishop_mover.get_loud_moves(astate, moves);
        m_bishop_mover.get_quiet_moves(astate, moves);
        m_rook_mover.get_loud_moves(astate, moves);
        m_rook_mover.get_quiet_moves(astate, moves);
        m_king_mover.get_loud_moves(astate, moves);
        m_king_mover.get_quiet_moves(astate, moves);
    };

  private:
    // Hold instances of MoveGenerators
    PawnMover m_pawn_mover;
    KnightMover m_knight_mover;
    BishopMover m_bishop_mover;
    RookMover m_rook_mover;
    KingMover m_king_mover;
};

static_assert(MultiMoveGenerator<AllMoveGenerator>);
static_assert(OneshotMoveGenerator<AllMoveGenerator>);

} // namespace move::movegen

#endif

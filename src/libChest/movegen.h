#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "attack.h"
#include "board.h"
#include "move.h"
#include "state.h"
#include <type_traits>
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
// full state struct may be used.
//
// TODO: test whether staged generation is faster than all move generation +
// sorting.
//

namespace move::movegen {

//
// Responsible for getting the moves from moving a certain piece type,
// Instances compose the pawn move generators for one colour.
//
// TODO: check out some asm, how expensive is calling these constructors every
// node?
//

// TODO: use a concept?
class MoveGenerator {
    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves) = 0;
    virtual void get_loud_moves(const state::State &state,
                                std::vector<move::Move> &moves) = 0;
    virtual void get_all_moves(const state::State &state,
                               std::vector<move::Move> &moves) = 0;
};

template <board::Piece piece> class PieceMoveGenerator : public MoveGenerator {

  public:
    PieceMoveGenerator() {};
    // Add quiet moves to the moves list
    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves) {
        for (board::Bitboard b : attackers(state).singletons()) {
            get_quiet_moves(
                state, moves, b, state.side_occupancy(state.to_move),
                state.side_occupancy(!state.to_move), state.total_occupancy());
        }
    };

    // Add tactical moves to the moves list
    void get_loud_moves(const state::State &state,
                        std::vector<move::Move> &moves) {
        for (board::Bitboard b : attackers(state).singletons()) {
            get_loud_moves(state, moves, b, state.side_occupancy(state.to_move),
                           state.side_occupancy(!state.to_move),
                           state.total_occupancy());
        }
    };

    // Add all moves to the moves list
    //
    // Calls other methods, override if quiet/loud moves can be more quickly
    // generated together.
    void get_all_moves(const state::State &state,
                       std::vector<move::Move> &moves) {

        // TODO: implement in state.h
        bool checked = false;
        if (checked) {
            // return get_check_moves();
        } else {
            get_loud_moves(state, moves);
            get_quiet_moves(state, moves);
        };
    };

  protected:
    // Helper: all pieces to be looped over at this node
    virtual constexpr board::Bitboard
    attackers(const state::State &state) const {
        return state.copy_bitboard(piece, state.to_move);
    }

    // Return all the moves for a specific piece
    // Pieces are found by looping over bitboard singletons,
    // implementations may bitscan to call required methods
    // without penalty.
    virtual void constexpr get_quiet_moves(const state::State &state,
                                           std::vector<move::Move> &moves,
                                           board::Bitboard origin,
                                           board::Bitboard occ_to_move,
                                           board::Bitboard occ_opponent,
                                           board::Bitboard occ_total) = 0;
    virtual void constexpr get_loud_moves(const state::State &state,
                                          std::vector<move::Move> &moves,
                                          board::Bitboard origin,
                                          board::Bitboard occ_to_move,
                                          board::Bitboard occ_opponent,
                                          board::Bitboard occ_total) = 0;
};

//
// Normal jumping (king/knight) move generation
//

template <board::Piece piece, typename T>
class JumpingMoveGenerator : public PieceMoveGenerator<piece> {

  public:
    using PieceMoveGenerator<piece>::get_loud_moves;
    using PieceMoveGenerator<piece>::get_quiet_moves;

  private:
    // Compose a PrecomputedAttackGenerator with assert
    T m_attacker;
    static_assert(
        std::is_base_of<attack::PrecomputedAttackGenerator, T>::value);

    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves,
                                 board::Bitboard origin,
                                 board::Bitboard occ_to_move,
                                 board::Bitboard occ_opponent,
                                 board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked =
            m_attacker.get_attack_set(from).setdiff(occ_total);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::NORMAL);
        }
    };

    virtual void get_loud_moves(const state::State &state,
                                std::vector<move::Move> &moves,
                                board::Bitboard origin,
                                board::Bitboard occ_to_move,
                                board::Bitboard occ_opponent,
                                board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker.get_attack_set(from);
        attacked &= occ_opponent;

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::CAPTURE);
        }
    };
};

//
// Pawn moves
//

template <board::Colour c>
class PawnMoveGenerator : public PieceMoveGenerator<board::Piece::PAWN> {

  public:
    using PieceMoveGenerator::get_loud_moves;
    using PieceMoveGenerator::get_quiet_moves;

  private:
    void get_single_pushes(std::vector<move::Move> &moves,
                           board::Bitboard origin, board::Bitboard occ_total,
                           board::Square from) {
        board::Bitboard push_dest = m_single_pusher.get_attack_set(from);
        if (push_dest & occ_total) {
            return;
        }

        // Pawns are always (single) pushable, ignoring blockers
        board::Square to = push_dest.single_bitscan_forward();

        // Promotions/normal push
        if (push_dest & back_rank_mask) {
            moves.emplace_back(from, to, MoveType::PROMOTE_QUEEN);
            moves.emplace_back(from, to, MoveType::PROMOTE_ROOK);
            moves.emplace_back(from, to, MoveType::PROMOTE_BISHOP);
            moves.emplace_back(from, to, MoveType::PROMOTE_KNIGHT);
        } else {
            moves.emplace_back(from, to, MoveType::SINGLE_PUSH);
        }
    };

    void get_double_pushes(std::vector<move::Move> &moves,
                           board::Bitboard origin, board::Bitboard occ_total,
                           board::Square from) {

        board::Bitboard push_dest = m_double_pusher.get_attack_set(from);
        if (push_dest.empty() | (push_dest & occ_total)) {
            return;
        }

        moves.emplace_back(from, push_dest.single_bitscan_forward(),
                           MoveType::SINGLE_PUSH);
    };

    void get_captures(std::vector<move::Move> &moves, const state::State &state,
                      board::Bitboard origin, board::Bitboard occ_opponent,
                      board::Square from) {

        board::Bitboard capture_dests = m_attacker.get_attack_set(from);

        // TODO: get optional directly from state
        board::Bitboard ep_bb = state.en_passant_active()
                                    ? board::Bitboard(state.en_passant_square())
                                    : 0;

        capture_dests &= occ_opponent;
        if (capture_dests.empty()) {
            return;
        }

        if (capture_dests & back_rank_mask) {
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

    // (consider promotions to be loud)
    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves,
                                 board::Bitboard origin,
                                 board::Bitboard occ_to_move,
                                 board::Bitboard occ_opponent,
                                 board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        get_single_pushes(moves, origin, occ_total, from);
        get_double_pushes(moves, origin, occ_total, from);
    }

    virtual void get_loud_moves(const state::State &state,
                                std::vector<move::Move> &moves,
                                board::Bitboard origin,
                                board::Bitboard occ_to_move,
                                board::Bitboard occ_opponent,
                                board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        get_captures(moves, state, origin, occ_total, from);
    }

    // Sepate methods for attacks/pushes
    // template <board::Colour c>
    // class SinglePusher : public attack::PawnSinglePushGenerator<c> {
    class SinglePusher : public attack::PawnSinglePushGenerator<c> {
      public:
        using attack::PawnSinglePushGenerator<c>::PawnSinglePushGenerator;
    };

    // template <board::Colour c>
    class DoublePusher : public attack::PawnDoublePushGenerator<c> {
      public:
        using attack::PawnDoublePushGenerator<c>::PawnDoublePushGenerator;
    };

    // template <board::Colour c>
    class Attacker : public attack::PawnAttackGenerator<c> {
      public:
        using attack::PawnAttackGenerator<c>::PawnAttackGenerator;
    };
    // private:
    // Attack set generators
    // Instantiate classes based on colour
    // Keep an instance of each in static memory
    // TODO: test static vs stack allocation
    SinglePusher m_single_pusher{};
    DoublePusher m_double_pusher{};
    Attacker m_attacker{};

    // Helper constants
    static constexpr int back_rank_n = (bool)c ? board::board_size - 1 : 0;
    static constexpr board::Bitboard back_rank_mask =
        board::Bitboard::rank_mask(back_rank_n);
};

//
// Uniform move generation.
//

// For pieces which move the same for quiet moves/attacks,
// and for white/black.
// Composes a AttackGenerator.
template <board::Piece piece, typename T>
// requires attack::AttackGenerator<T>
class UniformMoveGenerator : public PieceMoveGenerator<piece> {

  public:
    using PieceMoveGenerator<piece>::get_loud_moves;
    using PieceMoveGenerator<piece>::get_quiet_moves;

  private:
    // Compose an AttackGenerator
    T m_attacker;

    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves,
                                 board::Bitboard origin,
                                 board::Bitboard occ_to_move,
                                 board::Bitboard occ_opponent,
                                 board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked =
            m_attacker.get_attack_set(from).setdiff(occ_total);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::NORMAL);
        }
    };

    virtual void get_loud_moves(const state::State &state,
                                std::vector<move::Move> &moves,
                                board::Bitboard origin,
                                board::Bitboard occ_to_move,
                                board::Bitboard occ_opponent,
                                board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker.get_attack_set(from);
        attacked &= occ_opponent;

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::CAPTURE);
        }
    }
};

// Sliding move generators include the queen as a piece to move.
template <board::Piece piece, typename T>
class SlidingMoveGenerator : public PieceMoveGenerator<piece> {
  public:
    using PieceMoveGenerator<piece>::get_loud_moves;
    using PieceMoveGenerator<piece>::get_quiet_moves;

  protected:
    T m_attacker;

    // Include queen as slider
    constexpr board::Bitboard
    attackers(const state::State &state) const override {
        return state.copy_bitboard(piece, state.to_move) |
               state.copy_bitboard(board::Piece::QUEEN, state.to_move);
    }

    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves,
                                 board::Bitboard origin,
                                 board::Bitboard occ_to_move,
                                 board::Bitboard occ_opponent,
                                 board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked =
            m_attacker.get_attack_set(from, occ_total).setdiff(occ_total);

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::NORMAL);
        }
    };

    virtual void get_loud_moves(const state::State &state,
                                std::vector<move::Move> &moves,
                                board::Bitboard origin,
                                board::Bitboard occ_to_move,
                                board::Bitboard occ_opponent,
                                board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        board::Bitboard attacked = m_attacker.get_attack_set(from, occ_total);
        attacked &= occ_opponent;

        for (board::Bitboard dest : attacked.singletons()) {
            moves.emplace_back(from, dest.single_bitscan_forward(),
                               MoveType::CAPTURE);
        }
    }
};

// // Rooks need to also add castles
class RookMoveGenerator
    : public SlidingMoveGenerator<board::Piece::ROOK,
                                  attack::RookAttackGenerator> {

    using SlidingMoveGenerator::get_quiet_moves;

  public:
    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves) override {
        SlidingMoveGenerator::get_quiet_moves(state, moves);
        get_castles(state, moves,
                    state.total_occupancy()); // TODO: don't calculate this
                                              // twice, if it matters at all
    }

  private:
    // Assumes that castling rights are set correctly,
    // i.e. doesn't check king position, rook positions.
    void get_castles(const state::State &state, std::vector<move::Move> &moves,
                     board::Bitboard total_occ) {

        // Castle positions
        const board::Square w_ks_castle = board::H1;
        const board::Square w_qs_castle = board::A1;
        const board::Square b_ks_castle = board::H8;
        const board::Square b_qs_castle = board::A8;
        const board::Bitboard ks_castles =
            board::Bitboard(w_ks_castle) | board::Bitboard(b_ks_castle);
        const board::Bitboard qs_castles =
            board::Bitboard(w_qs_castle) | board::Bitboard(b_qs_castle);

        // King positions
        const board::Square w_king = board::E1;
        const board::Square b_king = board::E8;
        const board::Bitboard king_bb =
            board::Bitboard(w_king) | board::Bitboard(b_king);

        // Prebaked castle moves
        move::Move b_ks_move =
            move::Move(b_ks_castle, b_king, MoveType::CASTLE);
        move::Move b_qs_move =
            move::Move(b_qs_castle, b_king, MoveType::CASTLE);
        move::Move w_ks_move =
            move::Move(w_ks_castle, w_king, MoveType::CASTLE);
        move::Move w_qs_move =
            move::Move(w_qs_castle, w_king, MoveType::CASTLE);

        // Current player rooks
        board::Bitboard cur_rooks = board::Bitboard(
            state.copy_bitboard(board::Piece::ROOK, state.to_move));

        // Check both sides
        // TODO: better to get pos based on colour?
        // This has the nice property of easy assertion.

        if (state.can_castle(board::Piece::KING, state.to_move)) {
            board::Square ks_pos =
                (cur_rooks & ks_castles).single_bitscan_forward();
            board::Bitboard ks_attack =
                m_attacker.get_attack_set(ks_pos, total_occ);
            if (ks_attack & king_bb) {
                moves.push_back((bool)state.to_move ? w_ks_move : b_ks_move);
            }
        }

        if (state.can_castle(board::Piece::QUEEN, state.to_move)) {
            board::Square qs_pos =
                (cur_rooks & qs_castles).single_bitscan_forward();
            board::Bitboard qs_attack =
                m_attacker.get_attack_set(qs_pos, total_occ);
            if (qs_attack & king_bb) {
                moves.push_back((bool)state.to_move ? w_qs_move : b_qs_move);
            }
        }
    };
};

//
// All move generation
//

// Gets all the legal moves in a position,
// composes all neccessary MoveGenerators
class AllMoveGenerator : public MoveGenerator {

  public:
    AllMoveGenerator()
        : m_w_pawn_mover(), m_b_pawn_mover(), m_knight_mover(), m_king_mover(),
          m_bishop_mover(), m_rook_mover() {}

    virtual void get_quiet_moves(const state::State &state,
                                 std::vector<move::Move> &moves) override {
        (bool)state.to_move ? m_w_pawn_mover.get_quiet_moves(state, moves)
                            : m_b_pawn_mover.get_quiet_moves(state, moves);
        m_knight_mover.get_quiet_moves(state, moves);
        m_king_mover.get_quiet_moves(state, moves);
        m_bishop_mover.get_quiet_moves(state, moves);
        m_rook_mover.get_quiet_moves(state, moves);
    };

    virtual void get_loud_moves(const state::State &state,
                                std::vector<move::Move> &moves) override {
        (bool)state.to_move ? m_w_pawn_mover.get_loud_moves(state, moves)
                            : m_b_pawn_mover.get_loud_moves(state, moves);
        m_knight_mover.get_loud_moves(state, moves);
        m_king_mover.get_loud_moves(state, moves);
        m_bishop_mover.get_loud_moves(state, moves);
        m_rook_mover.get_loud_moves(state, moves);
    };
    virtual void get_all_moves(const state::State &state,
                               std::vector<move::Move> &moves) override {

        (bool)state.to_move ? m_w_pawn_mover.get_all_moves(state, moves)
                            : m_b_pawn_mover.get_all_moves(state, moves);
        m_knight_mover.get_all_moves(state, moves);
        m_king_mover.get_all_moves(state, moves);
        m_bishop_mover.get_all_moves(state, moves);
        m_rook_mover.get_all_moves(state, moves);
    };

  private:
    // Hold instances of MoveGenerators
    PawnMoveGenerator<board::Colour::WHITE> m_w_pawn_mover;
    PawnMoveGenerator<board::Colour::BLACK> m_b_pawn_mover;
    UniformMoveGenerator<board::Piece::KNIGHT, attack::KnightAttackGenerator>
        m_knight_mover;
    UniformMoveGenerator<board::Piece::KING, attack::KingAttackGenerator>
        m_king_mover;
    SlidingMoveGenerator<board::Piece::BISHOP, attack::BishopAttackGenerator>
        m_bishop_mover;
    RookMoveGenerator m_rook_mover;
};

} // namespace move::movegen

#endif

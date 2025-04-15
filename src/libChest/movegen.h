#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "attack.h"
#include "board.h"
#include "move.h"
#include "state.h"
#include <vector>

//
// Perform (pseudo-legal) move generation for one piece type x state.
// I.e. all moves must be legal, except may result in check.
// For now, when we expand a node, we can just see if we are in check.
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
// Instances compose state and move vector (i.e, per node of search),
// Subtypes share static instances of PrecomputedAttackGenerators.
//
// TODO: check out some asm, how expensive is calling these constructors every
// node?
//
template <board::Piece piece> class MoveGenerator {
  public:
    constexpr MoveGenerator(std::vector<move::Move> &moves_,
                            const state::State &state_)
        : moves(moves_), state(state_) {};

    // Add quiet moves to the moves list
    virtual void get_quiet_moves() {
        for (board::Bitboard b : attackers().singletons()) {
            get_quiet_moves(b);
        }
    };

    // Add tactical moves to the moves list
    virtual void get_loud_moves() {
        for (board::Bitboard b : attackers().singletons()) {
            get_loud_moves(b);
        }
    };

    // Get out of check
    // void get_check_moves() {
    //     for (board::Bitboard b : attackers().singletons()) {
    //         get_check_moves(b);
    //     }
    // };

    // Add all moves to the moves list
    //
    // Calls other methods, override if quiet/loud moves can be more quickly
    // generated together.
    virtual void get_all_moves() {

        // TODO: implement in state.h
        bool checked = false;
        if (checked) {
            // return get_check_moves();
        } else {
            get_loud_moves();
            get_quiet_moves();
        };
    };

  protected:
    // Helper: all pieces to be looped over at this node
    constexpr board::Bitboard attackers() const {
        return state.copy_bitboard(piece, state.to_move);
    }

    // Return all the moves for a specific piece
    // Pieces are found by looping over bitboard singletons,
    // implementations may bitscan to call required methods
    // without penalty.
    virtual void constexpr get_quiet_moves(board::Bitboard origin,
                                           board::Bitboard occ_to_move,
                                           board::Bitboard occ_opponent,
                                           board::Bitboard occ_total) = 0;
    virtual void constexpr get_loud_moves(board::Bitboard origin,
                                          board::Bitboard occ_to_move,
                                          board::Bitboard occ_opponent,
                                          board::Bitboard occ_total) = 0;
    // virtual void constexpr get_check_moves(board::Bitboard origin,
    //                                        board::Bitboard occ_to_move,
    //                                        board::Bitboard occ_opponent,
    //                                        board::Bitboard occ_total) = 0;

    std::vector<move::Move> &moves;
    const state::State &state;
};

//
// Pawn moves
//

// Sepate methods for attacks/pushes
template <board::Colour c>
class PawnMoveGenerator : public MoveGenerator<board::Piece::PAWN> {
  public:
    using MoveGenerator::MoveGenerator;

  private:
    void get_single_pushes(board::Bitboard origin, board::Bitboard occ_total,
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

    void get_double_pushes(board::Bitboard origin, board::Bitboard occ_total,
                           board::Square from) {

        board::Bitboard push_dest = m_double_pusher.get_attack_set(from);
        if (push_dest.empty() | (push_dest & occ_total)) {
            return;
        }

        moves.emplace_back(from, push_dest.single_bitscan_forward(),
                           MoveType::SINGLE_PUSH);
    };

    void get_captures(board::Bitboard origin, board::Bitboard occ_opponent,
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
    virtual void get_quiet_moves(board::Bitboard origin,
                                 board::Bitboard occ_to_move,
                                 board::Bitboard occ_opponent,
                                 board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        get_single_pushes(origin, occ_total, from);
        get_double_pushes(origin, occ_total, from);
    }

    virtual void get_loud_moves(board::Bitboard origin,
                                board::Bitboard occ_to_move,
                                board::Bitboard occ_opponent,
                                board::Bitboard occ_total) override {

        board::Square from = origin.single_bitscan_forward();
        get_captures(origin, occ_total, from);
    }

  private:
    // Attack set generators
    // Instantiate classes based on colour
    class SinglePusher : attack::PawnSinglePushGenerator<c> {
      public:
        using attack::PawnSinglePushGenerator<c>::PawnSinglePushGenerator;
    };
    class DoublePusher : attack::PawnDoublePushGenerator<c> {
      public:
        using attack::PawnDoublePushGenerator<c>::PawnDoublePushGenerator;
    };
    class Attacker : attack::PawnAttackGenerator<c> {
      public:
        using attack::PawnAttackGenerator<c>::PawnAttackGenerator;
    };

    // Keep an instance of each in static memory
    // TODO: test static vs stack allocation
    static SinglePusher m_single_pusher{};
    static DoublePusher m_double_pusher{};
    static Attacker m_attacker{};

    // Helper constants
    static constexpr int back_rank_n = (bool)c ? board::board_size - 1 : 0;
    static constexpr board::Bitboard back_rank_mask =
        board::Bitboard::file_mask(back_rank_n);
};
} // namespace move::movegen

#endif

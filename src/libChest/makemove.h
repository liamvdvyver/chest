#ifndef MAKEMOVE_H
#define MAKEMOVE_H

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "state.h"
#include <cstdio>
#include <optional>
#include <vector>

//
// A basic class which composes:
//
// * the state
// * a stack of made moves
// * functionality to make/unmake a move
//
// TODO: clean up this whole header.
//

namespace state {

// Stores all information needed to unmake move.
struct MadeMove {
    move::Move move;
    State::IrreversibleInfo info;
    // board::Piece moved_piece;
};

static const MadeMove nullMadeMove{0, State().irreversible()};

// Stores augmented state, has buffers for made moves, found moves.
// This is the basic unit for iterative (non-recursive, incrementally updated)
// game tree traversal,
template <move::movegen::PseudolegalGenerator T> struct SearchNode {

  public:
    constexpr SearchNode(const T &mover, AugmentedState &astate, int max_depth)
        : m_astate(astate), m_max_depth(max_depth), m_cur_depth(0),
          m_mover(mover), m_made_moves(max_depth, nullMadeMove),
          m_found_moves(max_depth, std::vector<move::Move>(max_moves, 0)) {};

    // All state changes which do not depend on current move
    // Called before processing board state
    void tick() {

        // increment on black move
        m_astate.state.fullmove_number += (int)(!m_astate.state.to_move);
        m_astate.state.halfmove_clock++;
    }

    // Helpers: update astate/state bitboards
    //
    constexpr void move_piece(board::Bitboard from, board::Bitboard to,
                              board::Bitboard &moved, board::Colour side) {
        board::Bitboard from_to = from ^ to;
        moved ^= from_to;
        m_astate.side_occupancy(side) ^= from_to;
        m_astate.total_occupancy ^= from_to;
    }

    // Update one piece's bitboard (for player to move), and incrementally
    // update astate occupancy
    constexpr void move_piece(board::Bitboard from, board::Bitboard to,
                              board::Bitboard &moved) {
        move_piece(from, to, moved, m_astate.state.to_move);
    }

    // Update (add/remove) one piece's bitboard based on a capture, and
    // incrementally update astate occupancy
    constexpr void toggle_piece(board::Bitboard removed_loc,
                                board::Bitboard &removed_bb,
                                board::Colour removed_colour) {
        removed_bb ^= removed_loc;
        m_astate.side_occupancy(removed_colour) ^= removed_loc;
        m_astate.total_occupancy ^= removed_loc;
    }

    // Swap the piece occupying a square from one bitboard to another
    // no update made to occupancy (since total occ doesn't change)
    constexpr void swap_piece(board::Bitboard loc, board::Bitboard &old_bb,
                              board::Bitboard &new_bb) {
        old_bb ^= loc;
        new_bb ^= loc;
    }

    // Swap occupancy from one player to another,
    // no update made to state bitboards
    constexpr void swap_occ(board::Bitboard loc) {
        m_astate.side_occupancy() ^= loc;
        m_astate.opponent_occupancy() ^= loc;
    }

    // Update the halfmove clock, bitboards, castling rights (captured piece
    // only), and occupancy for a capture.
    // Does not update castling rights for the attacker, since type might be
    // known before calling (e.g. pawns moves).
    constexpr void capture(board::Bitboard attacker_singleton,
                           board::Square victim_sq,
                           board::Bitboard victim_singleton,
                           board::Bitboard &attacker, board::Bitboard &victim) {

        const board::Bitboard from_to_bb =
            attacker_singleton ^ victim_singleton;

        // Reset halfmove clock
        m_astate.state.halfmove_clock = 0;

        // Update castling rights for potentially captured rooks
        update_rk_castling_rights(victim_sq, !m_astate.state.to_move);

        // Update bitboards
        attacker ^= from_to_bb;
        victim ^= victim_singleton;

        // Update occupancy
        m_astate.opponent_occupancy() ^= victim_singleton;
        m_astate.side_occupancy() ^= from_to_bb;
        m_astate.total_occupancy ^= attacker_singleton;
    };

    // Helpers: make moves

    // Move the king, remove castling rights, and return the final location for
    // the bishop.
    // Assumes castling is legal and from_bb is correct.
    constexpr board::Bitboard castle(board::Square from) {

        const board::Colour to_move = m_astate.state.to_move;
        const CastlingInfo &castling_info = m_astate.castling_info;

        // Get side (assumed valid)
        const std::optional<board::Piece> opt_side =
            castling_info.get_side(from, to_move);
        assert(opt_side.has_value());
        board::Piece side = opt_side.value();
        int side_idx = CastlingInfo::side_idx(side);

        // Move the king
        move_piece(board::Bitboard(castling_info.king_start[(int)to_move]),
                   board::Bitboard(
                       castling_info.king_destinations[(int)to_move][side_idx]),
                   m_astate.state.get_bitboard(board::Piece::KING, to_move));

        // Update rights
        m_astate.state.set_both_castling_rights(to_move, false);

        // Set to_bb for rook
        return castling_info.rook_destinations[(int)to_move][side_idx];
    }

    // Given the colour/location of a (potential) rook which is moved/captured,
    // update castling rights for the corresponding player.
    // Returns which side (queen/kingside) had rights removed for the player
    constexpr std::optional<board::Piece>
    update_rk_castling_rights(board::Square loc, board::Colour player) {
        std::optional<board::Piece> ret =
            m_astate.castling_info.get_side(loc, player);

        if (ret.has_value()) {
            m_astate.state.set_castling_rights(ret.value(), player, false);
            return ret;
        }

        return {};
    }

    // Given the colour/location of a (potential) king which is moved update
    // castling rights for the corresponding player. Return true if the king was
    // at this square, false otherwise.
    constexpr bool update_kg_castling_rights(board::Square loc,
                                             board::Colour player) {
        if (m_astate.castling_info.king_start[(int)player] == loc) {
            for (board::Piece side : CastlingInfo::castling_sides) {
                m_astate.state.set_castling_rights(side, player, false);
            }
            return true;
        }
        return false;
    }

    // Move handlers

    // Pawns are complicated, this handles it.
    // TODO: clean it up.
    constexpr void handle_pawn_move(MadeMove mmove) {

        // Reset halfmove clock
        m_astate.state.halfmove_clock = 0;

        const board::Colour to_move = m_astate.state.to_move;
        board::Bitboard &moved =
            m_astate.state.get_bitboard(board::Piece::PAWN, to_move);

        const board::Square from = mmove.move.from();
        const board::Square to = mmove.move.to();
        const move::MoveType type = mmove.move.type();
        board::Bitboard &removed =
            m_astate.state.get_bitboard(mmove.info.captured_piece, !to_move);

        const board::Bitboard from_bb = board::Bitboard(from);
        const board::Bitboard to_bb = board::Bitboard(to);

        // Handle special cases: single/double push, ep capture

        switch (type) {
        case move::MoveType::SINGLE_PUSH: {
            move_piece(from_bb, to_bb, moved);
            return;
        }
        case move::MoveType::DOUBLE_PUSH: {
            const uint8_t ep_rank = board::push_rank[(int)to_move];
            m_astate.state.ep_square = board::Square(from.file(), ep_rank);
            move_piece(from_bb, to_bb, moved);
            return;
        }
        case (move::MoveType::CAPTURE_EP): {
            // Rank of captured piece
            uint8_t captured_rank = board::double_push_rank[(int)!to_move];

            board::Bitboard &removed =
                m_astate.state.get_bitboard(board::Piece::PAWN, !to_move);
            board::Bitboard removed_loc =
                board::Bitboard(board::Square(to.file(), captured_rank));
            move_piece(removed_loc, to_bb, removed, !to_move);
            capture(from_bb, to, to_bb, moved, removed);
            return;
        }
        default:
            break;
        }

        // Get pawn in new location
        if (move::is_capture(type)) {
            capture(from_bb, to, to_bb, moved, removed);
        } else {

            // Move to new location
            move_piece(from_bb, to_bb, moved);
        }

        // Swap for promoted piece
        if (move::is_promotion(type)) {

            board::Piece promoted = move::promoted_piece(type);
            swap_piece(to_bb, moved,
                       m_astate.state.get_bitboard(promoted, to_move));
        }
    };

    // Update bitboards for a move, updates castling rights, resets the halfmove
    // clock
    constexpr void update_bitboards(MadeMove mmove) {

        // Null move: nothing to do
        if (!mmove.move) {
            return;
        }

        const board::Square from = mmove.move.from();
        const board::Square to = mmove.move.to();
        const move::MoveType type = mmove.move.type();
        const board::Colour to_move = m_astate.state.to_move;

        board::Bitboard from_bb = board::Bitboard(mmove.move.from());
        board::Bitboard to_bb = board::Bitboard(mmove.move.to());

        // Gets set below
        // TODO: do this better

        std::optional<board::Piece> moved_type =
            m_astate.state.piece_at(from_bb, to_move);
        if (moved_type == board::Piece::PAWN) {
            m_astate.state.halfmove_clock = 0;
        }
        assert(moved_type.has_value());
        board::Bitboard &moved =
            m_astate.state.get_bitboard(moved_type.value(), to_move);

        switch (type) {
        case move::MoveType::NORMAL:
            update_kg_castling_rights(from, to_move);
            update_rk_castling_rights(from, to_move);
            return move_piece(from_bb, to_bb, moved);
        case move::MoveType::CAPTURE: {

            update_kg_castling_rights(from, to_move);
            update_rk_castling_rights(from, to_move);
            board::Bitboard &removed =
                *m_astate.state.bitboard_containing(to_bb, !to_move);
            return capture(from_bb, to, to_bb, moved, removed);
        }
        case move::MoveType::CASTLE: {
            to_bb = castle(from);
            return move_piece(from_bb, to_bb, moved);
        }
        default: {
            assert(move::is_pawn_move(type));
            return handle_pawn_move(mmove);
        }
        }
    }

    // Returns true if the move was legal, false if it either:
    // * Leaves the player who moved in check,
    // * Was a castle starting/passing through check
    constexpr bool make_move(move::Move move) {

        bool was_legal = true;

        // TODO: for flexibility in using non-vector containers,
        // use maxdepth insteead of push/pop back

        board::Piece captured = (board::Piece)0;
        if (move::is_capture(move.type())) {
            // Assumed valid, unless en-passant
            captured = m_astate.state
                           .piece_at(board::Bitboard(move.to()),
                                     !m_astate.state.to_move)
                           .value_or(board::Piece::PAWN);
        }

        MadeMove made{.move = move,
                      .info = m_astate.state.irreversible(captured)};

        m_made_moves.push_back(made);
        tick();
        m_astate.state.ep_square = {};

        update_bitboards(made);

        // Check legality
        // TODO: roll this into the the update_bitboards switch
        board::Bitboard to_move_king = m_astate.state.copy_bitboard(
            board::Piece::KING, m_astate.state.to_move);

        if (m_mover.is_attacked(m_astate, to_move_king.single_bitscan_forward(),
                                m_astate.state.to_move)) {
            was_legal = false;
        }

        // Special case: castle
        if (was_legal && move.type() == move::MoveType::CASTLE) {

            std::optional<board::Piece> side = m_astate.castling_info.get_side(
                move.from(), m_astate.state.to_move);
            assert(side.has_value());
            const int side_idx = CastlingInfo::side_idx(side.value());

            board::Bitboard king_mask =
                m_astate.castling_info
                    .king_mask[(int)m_astate.state.to_move][side_idx];
            for (board::Bitboard sq : king_mask.singletons()) {
                if (m_mover.is_attacked(m_astate, sq.single_bitscan_forward(),
                                        m_astate.state.to_move)) {
                    was_legal = false;
                    break;
                }
            };
        }

        // Next player
        m_astate.state.to_move = !m_astate.state.to_move;

        m_cur_depth++;
        return was_legal;
    };

    constexpr void unmake_move() {

        MadeMove unmake = m_made_moves.back();
        m_made_moves.pop_back();
        m_cur_depth--;

        m_astate.state.to_move = !m_astate.state.to_move;
        m_astate.state.reset(unmake.info);
        m_astate.state.fullmove_number -= (int)(!m_astate.state.to_move);

        const board::Square from = unmake.move.from();
        const board::Square to = unmake.move.to();
        const move::MoveType type = unmake.move.type();

        const board::Bitboard from_bb = board::Bitboard(from);
        const board::Bitboard to_bb = board::Bitboard(to);

        const board::Colour to_move = m_astate.state.to_move;

        // Undo promotions
        if (move::is_promotion(type)) {
            board::Bitboard &promo_bb = m_astate.state.get_bitboard(
                move::promoted_piece(type), to_move);
            board::Bitboard &pawn_bb =
                m_astate.state.get_bitboard(board::Piece::PAWN, to_move);
            swap_piece(to_bb, promo_bb, pawn_bb);
        }

        // Undo captures
        // Handle castling
        if (type == move::MoveType::CASTLE) {

            // Get info
            const CastlingInfo &castling_info = m_astate.castling_info;
            const board::Piece side =
                castling_info.get_side(from, to_move).value();
            const int side_idx = castling_info.side_idx(side);

            board::Bitboard &king_bb =
                m_astate.state.get_bitboard(board::Piece::KING, to_move);
            board::Bitboard &rook_bb =
                m_astate.state.get_bitboard(board::Piece::ROOK, to_move);

            // Move the king back
            move_piece(castling_info.king_destinations[(int)to_move][side_idx],
                       to, king_bb);

            // Move the rook back
            move_piece(castling_info.rook_destinations[(int)to_move][side_idx],
                       from, rook_bb);

            return;
        }
        // Move the piece normally

        // TODO: is it faster to store the piece type which was moved?
        board::Bitboard &moved =
            move::is_pawn_move(type)
                ? m_astate.state.get_bitboard(board::Piece::PAWN, to_move)
                : *m_astate.state.bitboard_containing(to_bb, to_move);

        move_piece(to_bb, from_bb, moved, to_move);

        if (move::is_capture(type)) {
            board::Bitboard &removed = m_astate.state.get_bitboard(
                unmake.info.captured_piece, !to_move);
            board::Square captured =
                (type == move::MoveType::CAPTURE_EP)
                    ? board::Square(to.file(),
                                    board::double_push_rank[(int)!to_move])
                    : to;
            toggle_piece(board::Bitboard(captured), removed, !to_move);
        }

        return;
    };

    struct PerftResult {
        uint64_t perft;
        uint64_t nodes;

        PerftResult operator+=(const PerftResult &b) {
            perft += b.perft;
            nodes += b.nodes;
            return *this;
        }
    };

    // Does depth == maxdepth?
    constexpr bool bottomed_out() {
        return m_max_depth == m_cur_depth;
    }

    // Find moves at the current depth
    // Returns a reference to the vector containing the found moves
    constexpr std::vector<move::Move> &find_moves() {

        m_found_moves.at(m_cur_depth).clear();
        m_mover.get_all_moves(m_astate, m_found_moves.at(m_cur_depth));
        return m_found_moves.at(m_cur_depth);
    }

    // Count the number of leaves at a certain depth, and (non-root) interior
    // nodes
    constexpr PerftResult perft() {

        // Cutoff
        if (bottomed_out()) {
            return {.perft = 1, .nodes = 0};
        }

        std::vector<move::Move> &moves = find_moves();
        PerftResult ret = {0, 0};

        for (move::Move m : moves) {

            bool was_legal = make_move(m);
            if (was_legal) {
                PerftResult subtree_result = perft();
                ret += subtree_result;
                ret.nodes += 1;
            }

            unmake_move();
        }

        return ret;
    };

    // private:
    // Size of move buffer
    // Note: 218 is the limit of legal chess moves
    // since we have psuedolegal moves, this could be insufficient,
    // I just chose 256, then we can index in with a uint8_t.
    static const int max_moves = 256;

    AugmentedState &m_astate;
    int m_max_depth;
    int m_cur_depth;

    // TODO: try different (stack-based) containers
    const T &m_mover;
    std::vector<MadeMove> m_made_moves;
    std::vector<std::vector<move::Move>> m_found_moves;
};
} // namespace state
#endif

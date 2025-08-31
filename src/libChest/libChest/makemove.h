#ifndef MAKEMOVE_H
#define MAKEMOVE_H

#include <cstdio>
#include <optional>

#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "state.h"

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
    move::FatMove fmove;
    State::IrreversibleInfo info;
    // board::Piece moved_piece;
};

static const MadeMove nullMadeMove{{}, State().irreversible()};

// Stores augmented state, has buffers for made moves, found moves.
// This is the basic unit for iterative (non-recursive, incrementally updated)
// game tree traversal,
template <size_t MaxDepth, eval::StaticEvaluator TEval>
struct SearchNode {
   public:
    constexpr SearchNode(const move::movegen::AllMoveGenerator<> &mover,
                         AugmentedState &astate, int max_depth)
        : m_astate(astate),
          m_eval(astate),
          m_max_depth(max_depth),
          m_cur_depth(0),
          m_mover(mover) {};

    // All state changes which do not depend on current move
    // Called before processing board state
    void tick() {
        // increment on black move
        m_astate.state.fullmove_number += (int)(!m_astate.state.to_move);
        m_astate.state.halfmove_clock++;
    }

    // Helpers: make moves

    // Assumes move is pseudo-legal, returns whether it was legal.
    // If legal, moves the king and the bishop and removes castling rights.
    constexpr bool castle(board::Square from, board::Colour to_move) {
        bool legal = true;

        // Constants for convinience
        const CastlingInfo &castling_info = m_astate.castling_info;

        // Get (queen/king)-side
        const std::optional<board::Piece> side =
            castling_info.get_side(from, to_move);
        board::ColouredPiece cp = {to_move, side.value()};

        // Check legality
        for (board::Bitboard sq :
             castling_info.get_king_mask(cp).singletons()) {
            if (m_mover.is_attacked(m_astate, sq.single_bitscan_forward(),
                                    to_move)) {
                legal = false;
            }
        }

        // Move the king
        move(board::Bitboard(castling_info.get_king_start(to_move)),
             board::Bitboard(castling_info.get_king_destination(cp)),
             {to_move, board::Piece::KING});

        // Move the rook
        move(board::Bitboard(castling_info.get_rook_start(cp)),
             board::Bitboard(castling_info.get_rook_destination(cp)),
             {to_move, board::Piece::ROOK});

        // Update rights
        remove_castling_rights(to_move);

        return legal;
    }

    // Given the colour/location of a (potential) rook which is moved/captured,
    // update castling rights for the corresponding player.
    // Returns which side (queen/kingside) had rights removed for the player
    constexpr std::optional<board::Piece> update_rk_castling_rights(
        board::Square loc, board::Colour player) {
        std::optional<board::Piece> ret =
            m_astate.castling_info.get_side(loc, player);
        if (ret.has_value()) {
            remove_castling_rights({player, ret.value()});
        }
        return ret;
    }

    // Given the colour/location of a (potential) king which is moved update
    // castling rights for the corresponding player. Return true if the king was
    // at this square, false otherwise.
    constexpr bool update_kg_castling_rights(board::Square loc,
                                             board::Colour player) {
        if (m_astate.castling_info.get_king_start(player) == loc) {
            remove_castling_rights(player);
            return true;
        }
        return false;
    }

    // Move handlers

    // Returns true if the move was legal, false if:
    // * Leaves the player who moved in check,
    // * Was a castle starting/passing through check
    // Move is pushed to the stack.
    constexpr bool make_move(const move::FatMove fmove) {
        move::Move mv = fmove.get_move();
        board::Piece moved_p = fmove.get_piece();

        // Default values, update as needed
        MadeMove made{.fmove = fmove, .info = m_astate.state.irreversible()};

        // Get some constants
        const board::Colour to_move = m_astate.state.to_move;
        const board::Bitboard from_bb = board::Bitboard(mv.from());
        const board::Bitboard to_bb = board::Bitboard(mv.to());

        // Prepare for next move
        // Early returns still need to push the made move
        tick();
        m_astate.state.ep_square = {};
        m_cur_depth++;

        // Find moved piece (avoid lookup if pawn move)
        board::ColouredPiece moved;
        moved = {to_move, moved_p};

        // Handle castles
        if (mv.type() == move::MoveType::CASTLE) {
            m_astate.state.to_move = !m_astate.state.to_move;
            m_made_moves.push_back(made);
            return castle(mv.from(), to_move);
        } else {
            // Move
            move(from_bb, to_bb, moved);

            // Handle capture
            if (move::is_capture(mv.type())) {
                // En-passant: first move piece back to the target square
                // TODO: is it faster to use the en-passant info from state?
                board::ColouredPiece captured;
                if (mv.type() == move::MoveType::CAPTURE_EP) {
                    captured = {!to_move, board::Piece::PAWN};

                    // Find the square of the double-pushed pawn
                    uint8_t dp_rank = board::double_push_rank[(int)!to_move];
                    board::Square dp_square{mv.to().file(), dp_rank};
                    move(board::Bitboard(dp_square), to_bb, captured);

                    // Otherwise, find the piece
                } else {
                    captured = m_astate.state.piece_at(to_bb, !to_move).value();

                    // Update rights if a rook was taken
                    update_rk_castling_rights(mv.to(), !to_move);
                }

                // Remove the captured piece
                remove(to_bb, captured);

                // Update the made move
                made.info.captured_piece = captured.piece;
            }

            // Handle pawn moves; wrapping conditional avoids branching when we
            // know it is not a pawn move
            // TODO: is it quicker to just use the switch?
            if (moved.piece == board::Piece::PAWN) {
                m_astate.state.halfmove_clock = 0;

                // Set the en-passant square
                if (mv.type() == move::MoveType::DOUBLE_PUSH) {
                    m_astate.state.ep_square = board::Square(
                        mv.to().file(), board::push_rank[(int)to_move]);
                }

                // Promote
                else if (move::is_promotion(mv.type())) {
                    board::Piece promoted = move::promoted_piece(mv.type());
                    swap_sameside(to_bb, to_move, board::Piece::PAWN, promoted);
                }

            } else {
                // If the piece wasn't a PAWN,
                // update castling rights for moved piece
                update_rk_castling_rights(mv.from(), to_move);
                update_kg_castling_rights(mv.from(), to_move);
            }
        }

        // Legality check
        bool was_legal = !m_mover.is_attacked(
            m_astate,
            m_astate.state.copy_bitboard({to_move, board::Piece::KING})
                .single_bitscan_forward(),
            to_move);

        m_astate.state.to_move = !m_astate.state.to_move;
        m_made_moves.push_back(made);
        return was_legal;
    };

    constexpr void unmake_move() {
        MadeMove unmake = m_made_moves.back();
        m_made_moves.pop_back();
        m_cur_depth--;

        m_astate.state.to_move = !m_astate.state.to_move;
        m_astate.state.reset(unmake.info);
        m_astate.state.fullmove_number -= (int)(!m_astate.state.to_move);

        const board::Square from = unmake.fmove.get_move().from();
        const board::Square to = unmake.fmove.get_move().to();
        const move::MoveType type = unmake.fmove.get_move().type();

        const board::Bitboard from_bb = board::Bitboard(from);
        const board::Bitboard to_bb = board::Bitboard(to);

        const board::Colour to_move = m_astate.state.to_move;

        // Undo promotions
        if (move::is_promotion(type)) {
            swap_sameside(to_bb, to_move, move::promoted_piece(type),
                          board::Piece::PAWN);
        }

        // Undo captures
        // Handle castling
        if (type == move::MoveType::CASTLE) {
            // Get info
            const CastlingInfo &castling_info = m_astate.castling_info;
            const board::Piece side =
                castling_info.get_side(from, to_move).value();
            const board::ColouredPiece cp = {to_move, side};

            board::ColouredPiece king = {to_move, board::Piece::KING};
            board::ColouredPiece rook = {to_move, board::Piece::ROOK};

            // Move the king back
            move(castling_info.get_king_destination(cp), to, king);

            // Move the rook back
            move(castling_info.get_rook_destination(cp), from, rook);

            return;
        }
        // Move the piece normally

        board::ColouredPiece moved = {to_move, unmake.fmove.get_piece()};

        move(to_bb, from_bb, moved);

        if (move::is_capture(type)) {
            board::ColouredPiece removed = {!to_move,
                                            unmake.info.captured_piece};

            board::Square captured =
                (type == move::MoveType::CAPTURE_EP)
                    ? board::Square(to.file(),
                                    board::double_push_rank[(int)!to_move])
                    : to;
            add(board::Bitboard(captured), removed);
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
    constexpr bool bottomed_out() { return m_max_depth == m_cur_depth; }

    // Find moves at the current depth
    // Returns a reference to the vector containing the found moves
    constexpr MoveBuffer &find_moves() {
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

// Check state/eval are same before/after make-unmake
#ifndef NDEBUG
        std::string fen;
        eval::centipawn_t eval;
        if (m_cur_depth == 0) {
            fen = m_astate.state.to_fen();
            eval = m_eval.eval();
        }
#endif

        MoveBuffer &moves = find_moves();
        PerftResult ret = {0, 0};

        for (move::FatMove m : moves) {
            bool was_legal = make_move(m);
            if (was_legal) {
                PerftResult subtree_result = perft();
                ret += subtree_result;
                ret.nodes += 1;
            }

            unmake_move();

#ifndef NDEBUG
            // Check incremental updates were reversed in unmake
            if (m_cur_depth == 0) {
                assert(m_astate.state.to_fen() == fen);
                assert(eval == m_eval.eval());
                assert(eval == TEval(m_astate).eval());

                // If above below, check fresh evaluation is the same
            } else {
                assert(m_eval.eval() == TEval(m_astate).eval());
            }
#endif
        }

        return ret;
    };

    // Clears move buffers and sets max_depth
    void prep_search(size_t depth) {
        assert(depth <= MaxDepth);

        m_max_depth = depth;
        m_cur_depth = 0;
        m_made_moves.clear();
        m_found_moves.clear();
    };

    std::optional<move::FatMove> get_random_move() {
        prep_search(1);
        find_moves();
        for (move::FatMove m : m_found_moves.at(0)) {
            bool legal = make_move(m);
            unmake_move();
            if (legal) {
                return m;
            }
        }
        return {};
    }

    // Incrementally updateable.
    constexpr void add(board::Bitboard loc, board::ColouredPiece cp) {
        m_astate.add(loc, cp);
        m_eval.add(loc, cp);
    };
    constexpr void remove(board::Bitboard loc, board::ColouredPiece cp) {
        m_astate.remove(loc, cp);
        m_eval.remove(loc, cp);
    };
    constexpr void move(board::Bitboard from, board::Bitboard to,
                        board::ColouredPiece cp) {
        m_astate.move(from, to, cp);
        m_eval.move(from, to, cp);
    };
    constexpr void swap(board::Bitboard loc, board::ColouredPiece from,
                        board::ColouredPiece to) {
        m_astate.swap(loc, from, to);
        m_eval.swap(loc, from, to);
    };
    constexpr void swap_oppside(board::Bitboard loc, board::ColouredPiece from,
                                board::ColouredPiece to) {
        m_astate.swap_oppside(loc, from, to);
        m_eval.swap_oppside(loc, from, to);
    };
    constexpr void swap_sameside(board::Bitboard loc, board::Colour side,
                                 board::Piece from, board::Piece to) {
        m_astate.swap_sameside(loc, side, from, to);
        m_eval.swap_sameside(loc, side, from, to);
    };
    constexpr void remove_castling_rights(board::ColouredPiece cp) const {
        m_astate.remove_castling_rights(cp);
        m_eval.remove_castling_rights(cp);
    };
    constexpr void remove_castling_rights(board::Colour colour) const {
        m_astate.remove_castling_rights(colour);
        m_eval.remove_castling_rights(colour);
    };

    // Get evaluatiion

    eval::centipawn_t eval() const { return m_eval.eval(); }

    AugmentedState &m_astate;
    TEval m_eval{m_astate.state};
    int m_max_depth;
    int m_cur_depth;

    const move::movegen::AllMoveGenerator<> &m_mover;
    svec<MadeMove, MaxDepth> m_made_moves;
    svec<MoveBuffer, MaxDepth> m_found_moves;
};

static_assert(IncrementallyUpdateable<SearchNode<1, eval::DefaultEval>>);
}  // namespace state
#endif

//============================================================================//
// Tree traversal.
// Provides SearchNode struct for make/unmake tree traversal.
//============================================================================//

// A lot of ColouredPieces being passed around:
// NOLINTBEGIN(modernize-use-designated-initializers)

#pragma once

#include "board.h"
#include "eval.h"
#include "incremental.h"
#include "move.h"
#include "movegen.h"
#include "state.h"
#include "util.h"
#include "zobrist.h"

namespace state {

//============================================================================//
// Helper classes
//============================================================================//

// Stores information other than the from/to squares and move type
// which is neccessary to unmake a move.
struct IrreversibleInfo {
    board::Piece captured_piece;
    uint8_t halfmove_clock;
    CastlingRights castling_rights;
    // Negative if no square (smaller than optional).
    int8_t ep_file;
};

// Stores all information needed to unmake move.
struct MadeMove {
    move::FatMove fmove;
    IrreversibleInfo info;
};

static const MadeMove nullMadeMove{};

//============================================================================//
// Search nodes
//============================================================================//

// Stores augmented state, has buffers for made moves, found moves.
// This is the basic unit for iterative (non-recursive, incrementally updated)
// game tree traversal,
template <size_t MaxDepth, typename... TComponents>
struct SearchNode {
   public:
    constexpr SearchNode(const move::movegen::AllMoveGenerator &mover,
                         AugmentedState &astate, const size_t max_depth)
        : m_astate(astate),
          m_max_depth(max_depth),
          m_components(TComponents{astate}...),
          m_mover(mover) {};

    // Checks if an incrementally updateable component exists.
    template <IncrementallyUpdateable T>
    static constexpr bool has() {
        return tuple_has<T, decltype(m_components)>::value;
    }

    // Gets the incrementally updateable component by type.
    template <IncrementallyUpdateable T>
        requires(has<T>())
    auto &get() {
        return std::get<T>(m_components);
    }

    // Get state: calling code should manipulate through incremental interface.
    const AugmentedState &get_astate() const { return m_astate; }

    //-- Traversal -----------------------------------------------------------//

    // Returns true if the move was legal, false if:
    // * Leaves the player who moved in check,
    // * Was a castle starting/passing through check
    // Move is pushed to the stack.
    constexpr bool make_move(const move::FatMove fmove) {
        const move::Move mv = fmove.get_move();

        // Prepare for next move
        // Early returns still need to push the made move!
        m_cur_depth++;
        m_astate.state.fullmove_number +=
            static_cast<uint>(!m_astate.state.to_move);
        m_astate.state.halfmove_clock++;

        const board::Piece moved_p = fmove.get_piece();
        const board::Colour to_move = m_astate.state.to_move;
        const board::Bitboard from_bb = board::Bitboard(mv.from());
        const board::Bitboard to_bb = board::Bitboard(mv.to());
        const board::ColouredPiece moved = {to_move, moved_p};

        // Populated later
        MadeMove made{.fmove = fmove, .info = irreversible()};

        // Clear en-passant square
        if (m_astate.state.ep_square.has_value()) {
            remove_ep_sq(m_astate.state.ep_square.value());
        }

        // Handle castles
        if (mv.type() == move::MoveType::CASTLE) {
            set_to_move(!m_astate.state.to_move);
            m_made_moves.push_back(made);
            return castle(mv.from(), to_move);
        }

        // Move the piece which was moved
        move(from_bb, to_bb, moved);

        // Handle capture
        if (move::is_capture(mv.type())) {
            remove_captured(mv, to_move, to_bb, made);
        }

        // Handle special state updates per-move
        switch (moved.piece) {
            case board::Piece::PAWN:
                post_pawn_move(mv, to_move);
                break;
            case board::Piece::ROOK:
                update_rk_castling_rights(mv.from(), to_move);
                break;
            case board::Piece::KING:
                update_kg_castling_rights<false>(mv.from(), to_move);
                break;
            default:
        }

        // Legality check
        bool was_legal = !m_mover.is_attacked(
            m_astate,
            m_astate.state.copy_bitboard({to_move, board::Piece::KING})
                .single_bitscan_forward(),
            to_move);

        set_to_move(!m_astate.state.to_move);
        m_made_moves.push_back(made);
        return was_legal;
    };

    // Unmakes the last move pushed.
    constexpr void unmake_move() {
        MadeMove unmake = m_made_moves.back();
        m_made_moves.pop_back();
        m_cur_depth--;

        // Reset player to move, irreversible info, clocks
        set_to_move(!m_astate.state.to_move);
        reset(unmake.info);
        m_astate.state.fullmove_number -= (int)(!m_astate.state.to_move);

        const board::Square from = unmake.fmove.get_move().from();
        const board::Square to = unmake.fmove.get_move().to();
        const move::MoveType type = unmake.fmove.get_move().type();
        const board::Colour to_move = m_astate.state.to_move;
        const board::ColouredPiece moved = {to_move, unmake.fmove.get_piece()};

        // Undo promotions
        if (move::is_promotion(type)) {
            swap_sameside(to, to_move, move::promoted_piece(type),
                          board::Piece::PAWN);
        }

        // Handle castling
        if (type == move::MoveType::CASTLE) {
            return unmake_castle(from, to, to_move);
        }

        // Move the piece normally
        move(to, from, moved);

        // Undo captures
        if (move::is_capture(type)) {
            const board::ColouredPiece removed = {!to_move,
                                                  unmake.info.captured_piece};

            const board::Square captured =
                (type == move::MoveType::CAPTURE_EP)
                    ? board::Square(to.file(),
                                    board::ranks::double_push_rank(!to_move))
                    : to;
            add(board::Bitboard(captured), removed);
        }

        return;
    }

    //-- Searching -----------------------------------------------------------//

    // Clears move buffers and sets max_depth
    void prep_search(size_t depth) {
        assert(depth <= MaxDepth);

        m_max_depth = depth;
        m_cur_depth = 0;
        m_made_moves.clear();
        m_found_moves.clear();
    }

    // Find moves at the current depth
    // Returns a reference to the vector containing the found moves
    template <bool InOrder = false>
    constexpr MoveBuffer &find_moves() {
        m_found_moves[m_cur_depth].clear();
        m_mover.get_all_moves<InOrder>(m_astate, m_found_moves[m_cur_depth]);
        return m_found_moves[m_cur_depth];
    }

    std::optional<move::FatMove> get_random_move() {
        prep_search(1);
        find_moves();
        for (move::FatMove m : m_found_moves[0]) {
            bool legal = make_move(m);
            unmake_move();
            if (legal) {
                return m;
            }
        }
        return {};
    }

    // Helper: does depth == maxdepth?
    constexpr bool bottomed_out() { return m_max_depth == m_cur_depth; }

    //-- Incremental updates -------------------------------------------------//

    constexpr void add(const board::Bitboard loc,
                       const board::ColouredPiece cp) {
        m_astate.add(loc, cp);
        apply_tuple([=](auto &component) { component.add(loc, cp); },
                    m_components);
    }
    constexpr void remove(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        m_astate.remove(loc, cp);
        apply_tuple([=](auto &component) { component.remove(loc, cp); },
                    m_components);
    }
    constexpr void move(const board::Bitboard from, const board::Bitboard to,
                        const board::ColouredPiece cp) {
        m_astate.move(from, to, cp);
        apply_tuple([=](auto &component) { component.move(from, to, cp); },
                    m_components);
    }
    constexpr void swap(const board::Bitboard loc,
                        const board::ColouredPiece from,
                        const board::ColouredPiece to) {
        m_astate.swap(loc, from, to);
        apply_tuple([=](auto &component) { component.swap(loc, from, to); },
                    m_components);
    }
    constexpr void swap_oppside(const board::Bitboard loc,
                                const board::ColouredPiece from,
                                const board::ColouredPiece to) {
        m_astate.swap_oppside(loc, from, to);
        apply_tuple(
            [=](auto &component) { component.swap_oppside(loc, from, to); },
            m_components);
    }
    constexpr void swap_sameside(const board::Bitboard loc,
                                 const board::Colour side,
                                 const board::Piece from,
                                 const board::Piece to) {
        m_astate.swap_sameside(loc, side, from, to);
        apply_tuple(
            [=](auto &component) {
                component.swap_sameside(loc, side, from, to);
            },
            m_components);
    }
    constexpr void toggle_castling_rights(const state::CastlingRights rights) {
        m_astate.toggle_castling_rights(rights);
        apply_tuple(
            [=](auto &component) { component.toggle_castling_rights(rights); },
            m_components);
    }
    constexpr void add_ep_sq(const board::Square ep_sq) {
        m_astate.add_ep_sq(ep_sq);
        apply_tuple([=](auto &component) { component.add_ep_sq(ep_sq); },
                    m_components);
    }
    constexpr void remove_ep_sq(const board::Square ep_sq) {
        m_astate.remove_ep_sq(ep_sq);
        apply_tuple([=](auto &component) { component.remove_ep_sq(ep_sq); },
                    m_components);
    }
    constexpr void set_to_move(const board::Colour colour) {
        m_astate.set_to_move(colour);
        apply_tuple([=](auto &component) { component.set_to_move(colour); },
                    m_components);
    }

   private:
    //-- Traversal helpers ---------------------------------------------------//

    // Removes the captured piece after the capturing piece has been moved.
    // Updates the made move with the type of piece captured.
    constexpr void remove_captured(const move::Move mv,
                                   const board::Colour to_move,
                                   const board::Bitboard to_bb,
                                   MadeMove &made) {
        if (mv.type() == move::MoveType::CAPTURE_EP) [[unlikely]] {
            const board::ColouredPiece captured = {!to_move,
                                                   board::Piece::PAWN};

            // Find the square of the double-pushed pawn
            const board::coord_t dp_rank =
                board::ranks::double_push_rank(!to_move);
            const board::Square dp_square{mv.to().file(), dp_rank};

            move(board::Bitboard(dp_square), to_bb, captured);
        }

        const board::ColouredPiece captured =
            m_astate.state.piece_at(to_bb, !to_move).value();

        // Update rights if a rook was taken
        update_rk_castling_rights(mv.to(), !to_move);

        // Remove the captured piece
        remove(to_bb, captured);

        // Update the made move
        made.info.captured_piece = captured.piece;
    }

    // Perform pawn specific state updates:
    // * Set en-passant square
    // * Perform promotion
    // Assumes pawn has already been moved
    constexpr void post_pawn_move(const move::Move mv,
                                  const board::Colour to_move) {
        m_astate.state.halfmove_clock = 0;

        // Set the en-passant square
        if (mv.type() == move::MoveType::DOUBLE_PUSH) {
            add_ep_sq(board::Square(mv.to().file(),
                                    board::ranks::push_rank(to_move)));
        }

        // Promote
        else if (move::is_promotion(mv.type())) {
            const board::Piece promoted = move::promoted_piece(mv.type());
            swap_sameside({mv.to()}, to_move, board::Piece::PAWN, promoted);
        }
    }

    //-- Castling helpers ----------------------------------------------------//

    // Assumes move is pseudo-legal, returns whether it was legal.
    // If legal, moves the king and the bishop and removes castling rights.
    // Sufficient for early return.
    constexpr bool castle(const board::Square from,
                          const board::Colour to_move) {
        bool legal = true;

        // Get (queen/king)-side
        const std::optional<board::Piece> side =
            state::CastlingInfo::get_side(from, to_move);
        const board::ColouredPiece cp = {to_move, side.value()};

        // Check legality
        for (const board::Bitboard sq :
             state::CastlingInfo::get_king_mask(cp).singletons()) {
            if (m_mover.is_attacked(m_astate, sq.single_bitscan_forward(),
                                    to_move)) {
                legal = false;
            }
        }

        // Move the king
        move(board::Bitboard(state::CastlingInfo::get_king_start(to_move)),
             board::Bitboard(state::CastlingInfo::get_king_destination(cp)),
             {to_move, board::Piece::KING});

        // Move the rook
        move(board::Bitboard(state::CastlingInfo::get_rook_start(cp)),
             board::Bitboard(state::CastlingInfo::get_rook_destination(cp)),
             {to_move, board::Piece::ROOK});

        // Update rights
        toggle_castling_rights(
            m_astate.state.castling_rights.get_player_rights(to_move));

        return legal;
    }

    // Given the colour/location of a (potential) rook which is moved/captured,
    // update castling rights for the corresponding player.
    // Returns which side (queen/kingside) had rights removed for the player
    constexpr std::optional<board::Piece> update_rk_castling_rights(
        const board::Square loc, const board::Colour player) {
        const std::optional<board::Piece> ret =
            state::CastlingInfo::get_side(loc, player);

        if (ret.has_value() && m_astate.state.castling_rights.get_square_rights(
                                   {player, ret.value()})) {
            toggle_castling_rights(board::ColouredPiece{player, ret.value()});
        }
        return ret;
    }

    // Given the colour/location of a (potential) king which is moved update
    // castling rights for the corresponding player. Return true if the king was
    // at this square, false otherwise.
    // If checked, ensures the moved piece was the king.
    template <bool Checked = true>
    constexpr bool update_kg_castling_rights(const board::Square loc,
                                             const board::Colour player) {
        if constexpr (Checked) {
            if (state::CastlingInfo::get_king_start(player) == loc) {
                toggle_castling_rights(
                    m_astate.state.castling_rights.get_player_rights(player));
                return true;
            }
            return false;
        } else {
            toggle_castling_rights(
                m_astate.state.castling_rights.get_player_rights(player));
            return true;
        }
    }

    // Move the king and rook back after castling performed by to_move.
    constexpr void unmake_castle(const board::Square from,
                                 const board::Square to,
                                 const board::Colour to_move) {
        // Get info
        const board::Piece side =
            state::CastlingInfo::get_side(from, to_move).value();
        const board::ColouredPiece cp = {to_move, side};
        const board::ColouredPiece king = {to_move, board::Piece::KING};
        const board::ColouredPiece rook = {to_move, board::Piece::ROOK};

        // Move the king back
        move(state::CastlingInfo::get_king_destination(cp), to, king);

        // Move the rook back
        move(state::CastlingInfo::get_rook_destination(cp), from, rook);

        return;
    }

    //-- Irreversible info helpers -------------------------------------------//

    // Gets the irreversible info for a move to be made.
    constexpr IrreversibleInfo irreversible(
        board::Piece captured = (board::Piece)0) const {
        return {.captured_piece = captured,
                .halfmove_clock = m_astate.state.halfmove_clock,
                .castling_rights = m_astate.state.castling_rights,
                .ep_file = m_astate.state.ep_square.has_value()
                               ? (int8_t)m_astate.state.ep_square.value().file()
                               : (int8_t)-1};
    };

    // Resets the state according to irreversible info.
    // Assumes the player to move is the one who made the move,
    // i.e. to_move has already been reset.
    constexpr void reset(const IrreversibleInfo info) {
        // Reset clock
        m_astate.state.halfmove_clock = info.halfmove_clock;

        // Reset castling info
        // TODO: consider storing inverse, not calculating here.
        const state::CastlingRights cur_rights = m_astate.state.castling_rights;
        const state::CastlingRights new_rights = info.castling_rights;
        toggle_castling_rights(cur_rights ^ new_rights);

        // Reset ep square
        if (m_astate.state.ep_square.has_value()) {
            remove_ep_sq(m_astate.state.ep_square.value());
        }
        if (info.ep_file >= 0) {
            add_ep_sq(board::Square(
                info.ep_file,
                board::ranks::push_rank(!m_astate.state.to_move)));
        }
    };

    //-- Data members --------------------------------------------------------//

    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    // Movegen and state should outlive search node.

    AugmentedState &m_astate;
    size_t m_max_depth;
    size_t m_cur_depth = 0;

    // Incrementally updateable components in a tuple
    std::tuple<TComponents...> m_components;

    const move::movegen::AllMoveGenerator &m_mover;
    SVec<MadeMove, MaxDepth> m_made_moves;
    SVec<MoveBuffer, MaxDepth> m_found_moves;

    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};
static_assert(IncrementallyUpdateable<SearchNode<1, eval::DefaultEval>>);

//============================================================================//
// Perft
//============================================================================//

// Perft implementation is kept seperate.
template <size_t MaxDepth, typename... TComponents>
struct PerftNode : public SearchNode<MaxDepth, TComponents...> {
   public:
    using SearchNode<MaxDepth, TComponents...>::SearchNode;

    struct PerftResult {
        uint64_t perft;
        uint64_t nodes;

        PerftResult operator+=(const PerftResult &b) {
            perft += b.perft;
            nodes += b.nodes;
            return *this;
        }
    };

    // Count the number of leaves at a certain depth, and (non-root) interior
    // nodes
    constexpr PerftResult perft() {
        // Cutoff
        if (this->bottomed_out()) {
            return {.perft = 1, .nodes = 0};
        }

#if DEBUG()
        // Check state/eval are same before/after make-unmake
        // Get incremental info
        eval::centipawn_t eval{};
        Zobrist hash{};
        if constexpr (requires { this->template get<eval::DefaultEval>(); }) {
            eval = this->template get<eval::DefaultEval>().eval();
            assert(eval == eval::DefaultEval(this->get_astate()).eval());
        }
        if constexpr (requires { this->template get<Zobrist>(); }) {
            hash = this->template get<Zobrist>();
            assert(hash == Zobrist(this->get_astate()));
        }
#endif
        MoveBuffer &moves = this->find_moves();
        PerftResult ret = {0, 0};

        for (move::FatMove m : moves) {
            bool was_legal = this->make_move(m);
            if (was_legal) {
                PerftResult subtree_result = perft();
                ret += subtree_result;
                ret.nodes += 1;

#if DEBUG()
                // Check move was made correctly
                if constexpr (requires { this->template get<Zobrist>(); }) {
                    assert(Zobrist{this->get_astate()} ==
                           this->template get<Zobrist>());
                }
                if constexpr (requires {
                                  this->template get<eval::DefaultEval>();
                              }) {
                    assert(eval::DefaultEval{this->get_astate()}.eval() ==
                           this->template get<eval::DefaultEval>().eval());
                }
#endif
            }

            this->unmake_move();
        }

#if DEBUG()
        // Check moves were all unmade
        if constexpr (requires { this->template get<eval::DefaultEval>(); }) {
            assert(eval == this->template get<eval::DefaultEval>().eval());
        }
        if constexpr (requires { this->template get<Zobrist>(); }) {
            assert(hash == this->template get<Zobrist>());
        }
#endif
        return ret;
    };
};

}  // namespace state

// NOLINTEND(modernize-use-designated-initializers)

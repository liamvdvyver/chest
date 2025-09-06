//============================================================================//
// Representations of full, partial, or augemnted game state
//============================================================================//

#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include "board.h"

namespace state {

//============================================================================//
// FEN strings
//============================================================================//

using fen_t = std::string;  // FEN strings

// Standard game setup
static const fen_t new_game_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

//============================================================================//
// Castling information (contants and helpers)
//============================================================================//

// Precompute masks/squares for castling
namespace CastlingInfo {

// For array sizing
static constexpr size_t n_castling_sides = 2;
static constexpr size_t n_castling_squares =
    board::n_colours * n_castling_sides;
// Iteration through castles
static constexpr std::array<board::Piece, n_castling_sides> castling_sides{
    {board::Piece::QUEEN, board::Piece::KING}};
static constexpr std::array<board::ColouredPiece, n_castling_squares>
    castling_squares{
        {{.colour = board::Colour::BLACK, .piece = board::Piece::QUEEN},
         {.colour = board::Colour::WHITE, .piece = board::Piece::QUEEN},
         {.colour = board::Colour::BLACK, .piece = board::Piece::KING},
         {.colour = board::Colour::WHITE, .piece = board::Piece::KING}}};

// Is the piece a valid argument to castling accessors?
static constexpr bool is_castling_side(board::Piece p) {
    return p == board::Piece::KING || p == board::Piece::QUEEN;
}

namespace detail {
// The following final destinations are the same for classical and 960, and
// will remain const:

// Final king positions
static constexpr board::Square w_ks_king_dest = board::G1;
static constexpr board::Square w_qs_king_dest = board::C1;
static constexpr board::Square b_ks_king_dest = board::G8;
static constexpr board::Square b_qs_king_dest = board::C8;

// Final rook positions
static constexpr board::Square w_ks_rook_dest = board::F1;
static constexpr board::Square w_qs_rook_dest = board::D1;
static constexpr board::Square b_ks_rook_dest = board::F8;
static constexpr board::Square b_qs_rook_dest = board::D8;

// The following should be calculated at game init to support 960:

// Initial king positions
static constexpr board::Square w_king_start = board::E1;
static constexpr board::Square b_king_start = board::E8;

// Locations of rooks w/ castling rights
static constexpr board::Square w_ks_rook_start = board::H1;
static constexpr board::Square w_qs_rook_start = board::A1;
static constexpr board::Square b_ks_rook_start = board::H8;
static constexpr board::Square b_qs_rook_start = board::A8;

// Masks: squares which must be unobstructed to castle

static constexpr board::Bitboard w_ks_rook_mask =
    board::Bitboard(board::Square(board::F1)) ^
    board::Bitboard(board::Square(board::G1));

static constexpr board::Bitboard w_qs_rook_mask =
    board::Bitboard(board::Square(board::B1)) ^
    board::Bitboard(board::Square(board::C1)) ^
    board::Bitboard(board::Square(board::D1));

static constexpr board::Bitboard b_ks_rook_mask =
    w_ks_rook_mask.shift(0, board::board_size - 1);

static constexpr board::Bitboard b_qs_rook_mask =
    w_qs_rook_mask.shift(0, board::board_size - 1);

// Masks: squares which must be unchecked to castle

static constexpr board::Bitboard w_ks_king_mask =
    w_ks_king_dest ^ board::Bitboard(board::Square(board::E1)) ^
    board::Bitboard(board::Square(board::F1));

static constexpr board::Bitboard w_qs_king_mask =
    w_qs_king_dest ^ board::Bitboard(board::Square(board::E1)) ^
    board::Bitboard(board::Square(board::D1));

static constexpr board::Bitboard b_ks_king_mask =
    w_ks_king_mask.shift(0, board::board_size - 1);

static constexpr board::Bitboard b_qs_king_mask =
    w_qs_king_mask.shift(0, board::board_size - 1);

// Find index for layout of queenside/kingside arrays
static constexpr int side_idx(board::Piece side) {
    assert(is_castling_side(side));
    return ((int)(side == board::Piece::KING));
}

}  // namespace detail
// Given a square of a rook and a colour, find which side (king/queen) the
// rook belongs to, if any
static constexpr std::optional<board::Piece> get_side(board::Square square,
                                                      board::Colour colour) {
    switch (colour) {
        case board::Colour::BLACK:
            switch (square) {
                case detail::b_ks_rook_start:
                    return board::Piece::KING;
                case detail::b_qs_rook_start:
                    return board::Piece::QUEEN;
                default:
                    return {};
            }
        case board::Colour::WHITE:
            switch (square) {
                case detail::w_ks_rook_start:
                    return board::Piece::KING;
                case detail::w_qs_rook_start:
                    return board::Piece::QUEEN;
                default:
                    return {};
            }
    }
};

// Given colour and castling side, the king's destination
static constexpr board::Square get_king_destination(board::ColouredPiece cp) {
    assert(is_castling_side(cp.piece));
    return cp.colour == board::Colour::WHITE
               ? (cp.piece == board::Piece::KING ? detail::w_ks_king_dest
                                                 : detail::w_qs_king_dest)
               : (cp.piece == board::Piece::KING ? detail::b_ks_king_dest
                                                 : detail::b_qs_king_dest);
}

// Given colour and castling side, the rook's destination
static constexpr board::Square get_rook_destination(board::ColouredPiece cp) {
    assert(is_castling_side(cp.piece));
    return cp.colour == board::Colour::WHITE
               ? (cp.piece == board::Piece::KING ? detail::w_ks_rook_dest
                                                 : detail::w_qs_rook_dest)
               : (cp.piece == board::Piece::KING ? detail::b_ks_rook_dest
                                                 : detail::b_qs_rook_dest);
}

// Given colour and castling side, the rook's starting position
static constexpr board::Square get_rook_start(board::ColouredPiece cp) {
    assert(is_castling_side(cp.piece));
    return cp.colour == board::Colour::WHITE
               ? (cp.piece == board::Piece::KING ? detail::w_ks_rook_start
                                                 : detail::w_qs_rook_start)
               : (cp.piece == board::Piece::KING ? detail::b_ks_rook_start
                                                 : detail::b_qs_rook_start);
}

// Given colour, the king's starting position
static constexpr board::Square get_king_start(board::Colour colour) {
    return colour == board::Colour::WHITE ? detail::w_king_start
                                          : detail::b_king_start;
}

// Given colour and castling side, which squares must be unobstructed,
// including the final destination
static constexpr board::Bitboard get_rook_mask(board::ColouredPiece cp) {
    assert(is_castling_side(cp.piece));
    return cp.colour == board::Colour::WHITE
               ? (cp.piece == board::Piece::KING ? detail::w_ks_rook_mask
                                                 : detail::w_qs_rook_mask)
               : (cp.piece == board::Piece::KING ? detail::b_ks_rook_mask
                                                 : detail::b_qs_rook_mask);
}

// Given colour and castling side, which squares must be unchecked,
// including the starting and final king squares
static constexpr board::Bitboard get_king_mask(board::ColouredPiece cp) {
    assert(is_castling_side(cp.piece));
    return cp.colour == board::Colour::WHITE
               ? (cp.piece == board::Piece::KING ? detail::w_ks_king_mask
                                                 : detail::w_qs_king_mask)
               : (cp.piece == board::Piece::KING ? detail::b_ks_king_mask
                                                 : detail::b_qs_king_mask);
}
};  // namespace CastlingInfo

//============================================================================//
// Castling rights bitsets
//============================================================================//

// Generally mutated through xor.
using castling_rights_t = uint8_t;
struct CastlingRights : public Wrapper<castling_rights_t, CastlingRights> {
    using Wrapper::Wrapper;
    constexpr CastlingRights(const board::ColouredPiece cp)
        : Wrapper::Wrapper{
              static_cast<decltype(value)>(1 << square_offset(cp))} {}

    constexpr bool get_square_rights(const board::ColouredPiece cp) const {
        return value & (1 << square_offset(cp));
    }

    constexpr CastlingRights get_player_rights(
        const board::Colour colour) const {
        return value & (0b11 << 2 * static_cast<uint>(colour));
    }

    constexpr void set_castling_rights(const board::ColouredPiece cp,
                                       const bool rights) {
        *this &= ~CastlingRights{cp};          // clear bit
        *this ^= rights << square_offset(cp);  // Set to val
    }
    constexpr void set_both_castling_rights(const board::Colour colour,
                                            const bool rights) {
        for (board::Piece side : CastlingInfo::castling_sides) {
            set_castling_rights({.colour = colour, .piece = side}, rights);
        }
    }

    constexpr static CastlingRights square_mask(const board::ColouredPiece cp) {
        return 1 << square_offset(cp);
    }

    constexpr static size_t max = 0b1111;

   private:
    // Helper: defines layout of castling rights bitset
    static constexpr castling_rights_t square_offset(
        const board::ColouredPiece cp) {
        return 2 * static_cast<uint>(cp.colour) +
               CastlingInfo::detail::side_idx(cp.piece);
    }
};

//============================================================================//
// Minimal (complete, without redundancy) board state
//============================================================================//

// Does not track n-fold repetitions.
//
// Most members are directly accessible, except piece bitboard representation,
// which is subject to change.
struct State {
   public:
    // Blank state
    constexpr State() = default;

    // State from fen string
    State(const fen_t &fen_string);

    // Helper: default new game state
    static constexpr State new_game() { return {new_game_fen}; }

    // Last en-passant square, and whether current or not
    // TODO: this is wasteful, write a wrapper around a nibble.
    std::optional<board::Square> ep_square;

    // Plies since capture/pawn push
    uint8_t halfmove_clock{};

    // (2-ply) moves since game start
    uint fullmove_number{};

    // Side to move
    board::Colour to_move{};

    // Castling rights
    CastlingRights castling_rights;

    // Position accessors

    constexpr board::Bitboard &get_bitboard(const board::ColouredPiece cp) {
        return m_pieces[static_cast<size_t>(cp.colour)]
                       [static_cast<size_t>(cp.piece)];
    }

    constexpr board::Bitboard copy_bitboard(
        const board::ColouredPiece cp) const {
        return m_pieces[static_cast<size_t>(cp.colour)]
                       [static_cast<size_t>(cp.piece)];
    }

    // Union of piece bitboards, per side
    // WARN: slow, see AugmentedState.
    constexpr board::Bitboard side_occupancy(const board::Colour c) const {
        board::Bitboard ret = 0;

        for (size_t i = 0; i < board::n_pieces; i++) {
            ret |= m_pieces[static_cast<size_t>(c)][i];
        }

        return ret;
    }

    // Union of piece bitboards, both sides
    // WARN: slow, see AugmentedState.
    constexpr board::Bitboard total_occupancy() const {
        return side_occupancy(board::Colour::BLACK) |
               side_occupancy(board::Colour::WHITE);
    }

    // // First piece matching mask
    constexpr std::optional<board::ColouredPiece> const piece_at(
        const board::Bitboard bitset_mask) const {
        for (size_t colourIdx = 0; colourIdx <= 1; colourIdx++) {
            for (size_t pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {
                if (!(m_pieces[colourIdx][pieceIdx] & bitset_mask).empty()) {
                    return {{
                        .colour = (board::Colour)colourIdx,
                        .piece = (board::Piece)pieceIdx,
                    }};
                }
            }
        }
        return {};
    }

    // First piece matching mask of given colour
    constexpr std::optional<board::ColouredPiece> const piece_at(
        board::Bitboard bit, board::Colour colour) const {
        for (size_t pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {
            if (!(m_pieces[static_cast<size_t>(colour)][pieceIdx] & bit)
                     .empty()) {
                return {{.colour = colour, .piece = (board::Piece)pieceIdx}};
            }
        }
        return {};
    }

    // Pretty printing
    std::string pretty() const;
    std::string to_fen() const;

    // Incremental updates
    // (type checked in "incremental.h" to avoid recursive inclusion)

    constexpr void move(const board::Bitboard from_bb,
                        const board::Bitboard to_bb,
                        const board::ColouredPiece(cp)) {
        get_bitboard(cp) ^= (from_bb ^ to_bb);
    }
    constexpr void add(const board::Bitboard loc,
                       const board::ColouredPiece cp) {
        assert(!(get_bitboard(cp) & loc));
        toggle(loc, cp);
    }
    constexpr void remove(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        assert(get_bitboard(cp) & loc);
        toggle(loc, cp);
    }
    constexpr void swap(const board::Bitboard loc,
                        const board::ColouredPiece from,
                        const board::ColouredPiece to) {
        toggle(loc, from);
        toggle(loc, to);
    }
    constexpr void swap_sameside(const board::Bitboard loc,
                                 const board::Colour player,
                                 const board::Piece from,
                                 const board::Piece to) {
        swap(loc, {.colour = player, .piece = from},
             {.colour = player, .piece = to});
    }
    constexpr void swap_oppside(const board::Bitboard loc,
                                const board::ColouredPiece from,
                                const board::ColouredPiece to) {
        swap(loc, from, to);
    }
    constexpr void toggle_castling_rights(state::CastlingRights rights) {
        castling_rights ^= rights;
    }
    constexpr void add_ep_sq(const board::Square ep_sq) { ep_square = {ep_sq}; }
    constexpr void remove_ep_sq(const board::Square ep_sq) {
        (void)ep_sq;
        ep_square = {};
    }
    constexpr void set_to_move(const board::Colour colour) { to_move = colour; }

   private:
    constexpr void toggle(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        get_bitboard(cp) ^= loc;
    }

    // Position of all pieces for each player
    std::array<std::array<board::Bitboard, board::n_pieces>, board::n_colours>
        m_pieces;
};

std::ostream &operator<<(std::ostream &os, const State s);

//============================================================================//
// Augmented board state:
// Contains the basic state, plus some precomputed occpancy values.
// In search, movegen, etc, this is the object which should be passed around.
// Then the underlying state representation can be changed, but accessing any
// added member of this struct (e.g. occupancy) is guaranteed not to require
// recomputation within the same search node.
//============================================================================//

struct AugmentedState {
   public:
    AugmentedState() = default;
    AugmentedState(const State &state)
        : state(state),
          total_occupancy(state.total_occupancy()),
          m_side_occupancy{state.side_occupancy((board::Colour)0),
                           state.side_occupancy((board::Colour)1)} {};

    State state;
    board::Bitboard total_occupancy;

    // Helper accessor for side_occupancy bitsets

    board::Bitboard &side_occupancy(const board::Colour colour) {
        return m_side_occupancy[static_cast<size_t>(colour)];
    }

    board::Bitboard &side_occupancy() { return side_occupancy(state.to_move); }

    board::Bitboard &opponent_occupancy() {
        return side_occupancy(!state.to_move);
    }

    constexpr const board::Bitboard &side_occupancy(
        const board::Colour colour) const {
        return m_side_occupancy[static_cast<size_t>(colour)];
    }

    constexpr const board::Bitboard &side_occupancy() const {
        return side_occupancy(state.to_move);
    }

    constexpr const board::Bitboard &opponent_occupancy() const {
        return side_occupancy(!state.to_move);
    }

    // Incremental updates

    constexpr void move(const board::Bitboard from_bb,
                        const board::Bitboard to_bb,
                        const board::ColouredPiece cp) {
        state.move(from_bb, to_bb, cp);
        side_occupancy(cp.colour) ^= (from_bb ^ to_bb);
        total_occupancy ^= (from_bb ^ to_bb);
    }
    constexpr void add(const board::Bitboard loc,
                       const board::ColouredPiece cp) {
        state.add(loc, cp);
        side_occupancy(cp.colour) ^= loc;
        total_occupancy ^= loc;
    }
    constexpr void remove(const board::Bitboard loc,
                          const board::ColouredPiece cp) {
        state.remove(loc, cp);
        side_occupancy(cp.colour) ^= loc;
        total_occupancy ^= loc;
    }
    constexpr void swap(const board::Bitboard loc,
                        const board::ColouredPiece from,
                        const board::ColouredPiece to) {
        state.swap(loc, from, to);
        if (from.colour != to.colour) {
            swap_oppside(loc, from, to);
        }
    }
    constexpr void swap_oppside(const board::Bitboard loc,
                                const board::ColouredPiece from,
                                const board::ColouredPiece to) {
        state.swap_oppside(loc, from, to);
        side_occupancy() ^= loc;
        opponent_occupancy() ^= loc;
    }
    constexpr void swap_sameside(const board::Bitboard loc,
                                 const board::Colour player,
                                 const board::Piece from,
                                 const board::Piece to) {
        state.swap_sameside(loc, player, from, to);
    }
    constexpr void toggle_castling_rights(CastlingRights rights) {
        state.toggle_castling_rights(rights);
    }
    constexpr void add_ep_sq(const board::Square ep_sq) {
        state.add_ep_sq(ep_sq);
    }
    constexpr void remove_ep_sq(const board::Square ep_sq) {
        state.remove_ep_sq(ep_sq);
    }
    constexpr void set_to_move(const board::Colour colour) {
        state.set_to_move(colour);
    }

   private:
    std::array<board::Bitboard, board::n_colours> m_side_occupancy;
};

}  // namespace state

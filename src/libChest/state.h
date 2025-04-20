#ifndef STATE_H
#define STATE_H

#include "board.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

//
// Defines representations of full, partial, or augemnted game state
//

namespace state {

typedef std::string fen_t; // FEN strings

// Standard game setup
static const fen_t new_game_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Precompute masks/squares for castling
struct CastlingInfo {
  private:
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
    const board::Square w_king_start = board::E1;
    const board::Square b_king_start = board::E8;

    // Locations of rooks w/ castling rights
    const board::Square w_ks_rook_start = board::H1;
    const board::Square w_qs_rook_start = board::A1;
    const board::Square b_ks_rook_start = board::H8;
    const board::Square b_qs_rook_start = board::A8;

    // Masks: squares which must be unobstructed to castle
    const board::Bitboard w_ks_rook_mask =
        board::Bitboard(board::Square(board::F1)) ^
        board::Bitboard(board::Square(board::G1));
    const board::Bitboard w_qs_rook_mask =
        board::Bitboard(board::Square(board::B1)) ^
        board::Bitboard(board::Square(board::C1)) ^
        board::Bitboard(board::Square(board::D1));
    const board::Bitboard b_ks_rook_mask =
        w_ks_rook_mask.shift(0, board::board_size - 1);
    const board::Bitboard b_qs_rook_mask =
        w_qs_rook_mask.shift(0, board::board_size - 1);

    // Masks: squares which must be unchecked to castle
    const board::Bitboard w_ks_king_mask =
        board::Bitboard(board::Square(board::E1)) ^
        board::Bitboard(board::Square(board::F1)) ^
        board::Bitboard(board::Square(board::G1));
    const board::Bitboard w_qs_king_mask =
        board::Bitboard(board::Square(board::E1)) ^
        board::Bitboard(board::Square(board::D1)) ^
        board::Bitboard(board::Square(board::C1));
    const board::Bitboard b_ks_king_mask =
        w_ks_king_mask.shift(0, board::board_size - 1);
    const board::Bitboard b_qs_king_mask =
        w_qs_king_mask.shift(0, board::board_size - 1);

  public:
    // For array sizing
    static constexpr int n_castling_sides = 2;
    // Easy iteration through castles
    static constexpr board::Piece castling_sides[n_castling_sides]{
        board::Piece::QUEEN, board::Piece::KING};

    // Find index for layout of queenside/kingside arrays
    static constexpr int side_idx(board::Piece side) {
        // General layout used for castling-related matters:
        // {black: {qs, ks}, white: {qs, ks}}
        assert(side == board::Piece::QUEEN || side == board::Piece::KING);
        return ((int)(side == board::Piece::KING));
    }

    // Given a square of a rook and a colour, find which side (king/queen) the
    // rook belongs to, if any
    constexpr std::optional<board::Piece> get_side(board::Square square,
                                                   board::Colour colour) const {
        for (board::Piece side : castling_sides) {
            if (square == rook_start[(int)colour][side_idx(side)]) {
                return {side};
            }
        }
        return {};
    };

    // Given a square of a rook, find which side and colour the piece belongs
    // to, if any
    constexpr std::optional<board::Piece> get_side(board::Square square) const {
        std::optional<board::Piece> w_ret =
            get_side(square, board::Colour::WHITE);
        return w_ret.has_value() ? w_ret
                                 : get_side(square, board::Colour::BLACK);
    };

    //
    // Data: raw arrays
    //

    // Final king positions
    static constexpr board::Square
        king_destinations[board::n_colours][n_castling_sides]{
            {b_qs_king_dest, b_ks_king_dest}, {w_qs_king_dest, w_ks_king_dest}};

    // Final rook positions
    static constexpr board::Square
        rook_destinations[board::n_colours][n_castling_sides]{
            {b_qs_rook_dest, b_ks_rook_dest}, {w_qs_rook_dest, w_ks_rook_dest}};

    // Initial king positions
    const board::Square king_start[board::n_colours]{b_king_start,
                                                     w_king_start};

    // Locations of rooks w/ castling rights
    const board::Square rook_start[board::n_colours][n_castling_sides]{
        {b_qs_rook_start, b_ks_rook_start}, {w_qs_rook_start, w_ks_rook_start}};

    // Masks: squares which must be unobstructed to caste
    const board::Bitboard rook_mask[board::n_colours][n_castling_sides]{
        {b_qs_rook_mask, b_ks_rook_mask}, {w_qs_rook_mask, w_ks_rook_mask}};
    // Masks: squares which must be unchecked to caste
    const board::Bitboard king_mask[board::n_colours][n_castling_sides]{
        {b_qs_king_mask, b_ks_king_mask}, {w_qs_king_mask, w_ks_king_mask}};
};

// Store complete (minimal) game state.
// Does not track n-fold repetitions.
// Does not pre-compute any redundant information.
//
// Most members are directly accessible, except:
// * Piece bitboard representation,
// * Castling rights,
//
// which have implementations subject to change.
struct State {

  public:
    // Blank state
    constexpr State() : m_pieces{}, m_castling_rights{} {}

    // State from fen string
    State(const fen_t &fen_string);

    // Helper: default new game state
    static const State new_game() { return State(new_game_fen); }

    // Last en-passant square, and whether current or not
    std::optional<board::Square> ep_square;

    // Plies since capture/pawn push
    uint8_t halfmove_clock;

    // (2-ply) moves since game start
    int fullmove_number;

    // Side to move
    board::Colour to_move;

    // Position accessors

    constexpr board::Bitboard &get_bitboard(board::Piece piece,
                                            board::Colour colour) {
        return m_pieces[(int)colour][(int)piece];
    }

    constexpr board::Bitboard copy_bitboard(board::Piece piece,
                                            board::Colour colour) const {
        return m_pieces[(int)colour][(int)piece];
    }

    // Castling rights accessors

    constexpr bool get_castling_rights(board::Piece side,
                                       board::Colour colour) const {
        return (1) &
               (m_castling_rights >> castling_rights_offset(side, colour));
    }

    // TODO: test, and make it faster
    constexpr void set_castling_rights(board::Piece side, board::Colour colour,
                                       bool rights) {
        int selected_bit = (1 << castling_rights_offset(side, colour));
        int selected_bit_val = (rights << castling_rights_offset(side, colour));
        m_castling_rights =
            (m_castling_rights & ~selected_bit) ^ selected_bit_val;
    }
    // Union of piece bitboards, per side
    constexpr board::Bitboard side_occupancy(board::Colour colour) const {
        board::Bitboard ret = 0;

        for (int i = 0; i < board::n_pieces; i++) {
            ret |= m_pieces[(int)colour][i];
        }

        return ret;
    }

    // Union of piece bitboards, both sides
    constexpr board::Bitboard total_occupancy() const {
        return side_occupancy(board::Colour::BLACK) |
               side_occupancy(board::Colour::WHITE);
    }

    // First bitboard matching mask, given colour
    constexpr board::Bitboard *bitboard_containing(board::Bitboard bit,
                                                   board::Colour colour) {
        for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {
            board::Bitboard *ret = &m_pieces[(int)colour][pieceIdx];
            if (*ret & bit) {
                return ret;
            }
        }
        return nullptr;
    }

    // First bitboard matching mask
    constexpr board::Bitboard *bitboard_containing(board::Bitboard bit) {
        board::Bitboard *ret = bitboard_containing(bit, board::Colour::WHITE);
        return ret ? ret : bitboard_containing(bit, board::Colour::BLACK);
    }

    // First piece matching mask
    constexpr std::optional<board::ColouredPiece> const
    piece_at(board::Bitboard bit) const {
        for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
            for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {

                if (!(m_pieces[colourIdx][pieceIdx] & bit).empty()) {
                    return {{.piece = (board::Piece)pieceIdx,
                             .colour = (board::Colour)colourIdx}};
                }
            }
        }
        return {};
    }

    // First piece matching mask of given colour
    constexpr std::optional<board::Piece> const
    piece_at(board::Bitboard bit, board::Colour colour) const {
        for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {

            if (!(m_pieces[(int)colour][pieceIdx] & bit).empty()) {
                return (board::Piece)pieceIdx;
            }
        }
        return {};
    }

    // Pretty printing
    std::string pretty() const;
    std::string to_fen() const;

    // Stores information other than the from/to squares and move type
    // which is neccessary to unmake a move
    struct IrreversibleInfo {
        board::Piece captured_piece;
        uint8_t halfmove_clock;
        uint8_t castling_rights;
        int8_t ep_file; // set to negative if no square, TODO: maybe just store
                        // ep like this?
    };

    constexpr IrreversibleInfo
    irreversible(board::Piece captured = (board::Piece)0) const {
        return {.captured_piece = captured,
                .halfmove_clock = halfmove_clock,
                .castling_rights = m_castling_rights,
                .ep_file = ep_square.has_value()
                               ? (int8_t)ep_square.value().file()
                               : (int8_t)-1};
    };

    // Assumes the player to move is the one who made the move
    constexpr void reset(IrreversibleInfo info) {
        halfmove_clock = info.halfmove_clock;
        m_castling_rights = info.castling_rights;
        if (info.ep_file >= 0) {
            ep_square =
                board::Square(info.ep_file, board::push_rank[(int)(!to_move)]);
        } else {
            ep_square = {};
        }
    };

  private:
    // Position of all pieces for each player
    board::Bitboard m_pieces[board::n_colours][board::n_pieces];

    // Castling rights for each player, for each side (king/queen)
    uint8_t m_castling_rights;

    // Helper: defines layout of castling rights bitset
    constexpr int castling_rights_offset(board::Piece side,
                                         board::Colour colour) const {

        return (2 * (int)colour) + CastlingInfo::side_idx(side);
    }
};

std::ostream &operator<<(std::ostream &os, const State s);

// Contains the basic state, plus some precomputed values, (which can be
// incrementally updated), future possible uses include hashes, occupancy,
// attack/defend maps, etc.
//
// In search, movegen, etc, this is the object which should be passed around.
// Then the underlying state representation can be changed, but accessing any
// member of this struct (e.g. occupancy) is guaranteed not to require
// recomputation within the same search node.
struct AugmentedState {

  public:
    AugmentedState(State state)
        : state(state), total_occupancy(state.total_occupancy()),
          castling_info{},
          m_side_occupancy{state.side_occupancy((board::Colour)0),
                           state.side_occupancy((board::Colour)1)} {};

    State state;
    board::Bitboard total_occupancy;
    CastlingInfo castling_info;

    // Helper accessor for side_occupancy bitsets
    // TODO: is there a better way to ensure const correctness like this?
    board::Bitboard &side_occupancy(board::Colour colour) {
        return m_side_occupancy[(int)colour];
    }
    board::Bitboard &side_occupancy() { return side_occupancy(state.to_move); }
    board::Bitboard &opponent_occupancy() {
        return side_occupancy(!state.to_move);
    }
    constexpr const board::Bitboard &
    side_occupancy(board::Colour colour) const {
        return m_side_occupancy[(int)colour];
    }
    constexpr const board::Bitboard &side_occupancy() const {
        return side_occupancy(state.to_move);
    }
    constexpr const board::Bitboard &opponent_occupancy() const {
        return side_occupancy(!state.to_move);
    }

  private:
    board::Bitboard m_side_occupancy[board::n_colours];
};
}; // namespace state

#endif

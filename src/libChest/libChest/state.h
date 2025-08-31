#ifndef STATE_H
#define STATE_H

#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include "board.h"
#include "incremental.h"

//
// Defines representations of full, partial, or augemnted game state
//

namespace state {

using fen_t = std::string;  // FEN strings

// Standard game setup
static const fen_t new_game_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Precompute masks/squares for castling
struct CastlingInfo {
   public:
    // For array sizing
    static constexpr int n_castling_sides = 2;
    // Easy iteration through castles
    static constexpr board::Piece castling_sides[n_castling_sides]{
        board::Piece::QUEEN, board::Piece::KING};

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

   public:
    // Given a square of a rook and a colour, find which side (king/queen) the
    // rook belongs to, if any
    static constexpr std::optional<board::Piece> get_side(
        board::Square square, board::Colour colour) {
        switch (colour) {
            case board::Colour::BLACK:
                switch (square) {
                    case b_ks_rook_start:
                        return board::Piece::KING;
                    case b_qs_rook_start:
                        return board::Piece::QUEEN;
                }
                break;
            case board::Colour::WHITE:
                switch (square) {
                    case w_ks_rook_start:
                        return board::Piece::KING;
                    case w_qs_rook_start:
                        return board::Piece::QUEEN;
                }
                break;
        }
        return {};
    };

    // Given colour and castling side, the king's destination
    static constexpr board::Square get_king_destination(
        board::ColouredPiece cp) {
        assert(is_castling_side(cp.piece));
        return (bool)cp.colour
                   ? (cp.piece == board::Piece::KING ? w_ks_king_dest
                                                     : w_qs_king_dest)
                   : (cp.piece == board::Piece::KING ? b_ks_king_dest
                                                     : b_qs_king_dest);
    }

    // Given colour and castling side, the rook's destination
    static constexpr board::Square get_rook_destination(
        board::ColouredPiece cp) {
        assert(is_castling_side(cp.piece));
        return (bool)cp.colour
                   ? (cp.piece == board::Piece::KING ? w_ks_rook_dest
                                                     : w_qs_rook_dest)
                   : (cp.piece == board::Piece::KING ? b_ks_rook_dest
                                                     : b_qs_rook_dest);
    }

    // Given colour and castling side, the rook's starting position
    static constexpr board::Square get_rook_start(board::ColouredPiece cp) {
        assert(is_castling_side(cp.piece));
        return (bool)cp.colour
                   ? (cp.piece == board::Piece::KING ? w_ks_rook_start
                                                     : w_qs_rook_start)
                   : (cp.piece == board::Piece::KING ? b_ks_rook_start
                                                     : b_qs_rook_start);
    }

    // Given colour, the king's starting position
    static constexpr board::Square get_king_start(board::Colour colour) {
        return (bool)colour ? w_king_start : b_king_start;
    }

    // Given colour and castling side, which squares must be unobstructed,
    // including the final destination
    static constexpr board::Bitboard get_rook_mask(board::ColouredPiece cp) {
        assert(is_castling_side(cp.piece));
        return (bool)cp.colour
                   ? (cp.piece == board::Piece::KING ? w_ks_rook_mask
                                                     : w_qs_rook_mask)
                   : (cp.piece == board::Piece::KING ? b_ks_rook_mask
                                                     : b_qs_rook_mask);
    }

    // Given colour and castling side, which squares must be unchecked,
    // including the starting and final king squares
    static constexpr board::Bitboard get_king_mask(board::ColouredPiece cp) {
        assert(is_castling_side(cp.piece));
        return (bool)cp.colour
                   ? (cp.piece == board::Piece::KING ? w_ks_king_mask
                                                     : w_qs_king_mask)
                   : (cp.piece == board::Piece::KING ? b_ks_king_mask
                                                     : b_qs_king_mask);
    }

   private:
    // Is the piece a valid argument to castling accessors?
    static constexpr bool is_castling_side(board::Piece p) {
        return p == board::Piece::KING || p == board::Piece::QUEEN;
    }

    friend struct State;
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

    constexpr board::Bitboard &get_bitboard(board::ColouredPiece cp) {
        return m_pieces[(int)cp.colour][(int)cp.piece];
    }

    constexpr board::Bitboard copy_bitboard(board::ColouredPiece cp) const {
        return m_pieces[(int)cp.colour][(int)cp.piece];
    }

    // Castling rights accessors

    constexpr bool get_castling_rights(board::ColouredPiece cp) const {
        return (1) & (m_castling_rights >> castling_rights_offset(cp));
    }

    // TODO: test, and make it faster
    constexpr void set_castling_rights(board::ColouredPiece cp, bool rights) {
        int selected_bit = (1 << castling_rights_offset(cp));
        int selected_bit_val = (rights << castling_rights_offset(cp));
        m_castling_rights =
            (m_castling_rights & ~selected_bit) ^ selected_bit_val;
    }
    constexpr void set_both_castling_rights(board::Colour colour, bool rights) {
        for (board::Piece side : CastlingInfo::castling_sides) {
            set_castling_rights({.colour = colour, .piece = side}, rights);
        }
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

    // // First piece matching mask
    constexpr std::optional<board::ColouredPiece> const piece_at(
        board::Bitboard bit) const {
        for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
            for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {
                if (!(m_pieces[colourIdx][pieceIdx] & bit).empty()) {
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
        for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {
            if (!(m_pieces[(int)colour][pieceIdx] & bit).empty()) {
                return {{colour, (board::Piece)pieceIdx}};
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
        int8_t ep_file;  // set to negative if no square, TODO: maybe just store
                         // ep like this?
    };

    constexpr IrreversibleInfo irreversible(
        board::Piece captured = (board::Piece)0) const {
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

    // Incremental updates

    constexpr void move(board::Bitboard from_bb, board::Bitboard to_bb,
                        board::ColouredPiece(cp)) {
        get_bitboard(cp) ^= (from_bb ^ to_bb);
    }
    constexpr void add(board::Bitboard loc, board::ColouredPiece cp) {
        assert(!(get_bitboard(cp) & loc));
        toggle(loc, cp);
    }
    constexpr void remove(board::Bitboard loc, board::ColouredPiece cp) {
        assert(get_bitboard(cp) & loc);
        toggle(loc, cp);
    }
    constexpr void swap(board::Bitboard loc, board::ColouredPiece from,
                        board::ColouredPiece to) {
        toggle(loc, from);
        toggle(loc, to);
    }
    constexpr void swap_sameside(board::Bitboard loc, board::Colour player,
                                 board::Piece from, board::Piece to) {
        swap(loc, {player, from}, {player, to});
    }
    constexpr void swap_oppside(board::Bitboard loc, board::ColouredPiece from,
                                board::ColouredPiece to) {
        swap(loc, from, to);
    }
    constexpr void remove_castling_rights(board::ColouredPiece cp) {
        set_castling_rights(cp, false);
    }
    constexpr void remove_castling_rights(board::Colour colour) {
        set_both_castling_rights(colour, false);
    }

   private:
    constexpr void toggle(board::Bitboard loc, board::ColouredPiece cp) {
        get_bitboard(cp) ^= loc;
    }
    // Position of all pieces for each player
    board::Bitboard m_pieces[board::n_colours][board::n_pieces];

    // Castling rights for each player, for each side (king/queen)
    uint8_t m_castling_rights;

    // Helper: defines layout of castling rights bitset
    constexpr int castling_rights_offset(board::ColouredPiece cp) const {
        return (2 * (int)cp.colour) + CastlingInfo::side_idx(cp.piece);
    }
};
static_assert(IncrementallyUpdateable<State>);

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
    AugmentedState() : state::AugmentedState(state::State()) {};
    AugmentedState(State state)
        : state(state),
          total_occupancy(state.total_occupancy()),
          castling_info{},
          m_side_occupancy{state.side_occupancy((board::Colour)0),
                           state.side_occupancy((board::Colour)1)} {};

    // Rule of three: no dynamic memory to move

    ~AugmentedState() {}

    AugmentedState(const AugmentedState &other)
        : state::AugmentedState(other.state) {};

    void operator=(const AugmentedState &other) {
        state = other.state;
        total_occupancy = other.total_occupancy;
        total_occupancy = other.total_occupancy;
        // Castling info has no non-static members currently
        m_side_occupancy[0] = other.m_side_occupancy[0];
        m_side_occupancy[1] = other.m_side_occupancy[1];
    }

    State state;
    board::Bitboard total_occupancy;
    CastlingInfo castling_info;

    // Helper accessor for side_occupancy bitsets
    board::Bitboard &side_occupancy(board::Colour colour) {
        return m_side_occupancy[(int)colour];
    }
    board::Bitboard &side_occupancy() { return side_occupancy(state.to_move); }
    board::Bitboard &opponent_occupancy() {
        return side_occupancy(!state.to_move);
    }
    constexpr const board::Bitboard &side_occupancy(
        board::Colour colour) const {
        return m_side_occupancy[(int)colour];
    }
    constexpr const board::Bitboard &side_occupancy() const {
        return side_occupancy(state.to_move);
    }
    constexpr const board::Bitboard &opponent_occupancy() const {
        return side_occupancy(!state.to_move);
    }

    // Incremental updates

    constexpr void move(board::Bitboard from_bb, board::Bitboard to_bb,
                        board::ColouredPiece cp) {
        state.move(from_bb, to_bb, cp);
        side_occupancy(cp.colour) ^= (from_bb ^ to_bb);
        total_occupancy ^= (from_bb ^ to_bb);
    }
    constexpr void add(board::Bitboard loc, board::ColouredPiece cp) {
        state.add(loc, cp);
        side_occupancy(cp.colour) ^= loc;
        total_occupancy ^= loc;
    }
    constexpr void remove(board::Bitboard loc, board::ColouredPiece cp) {
        state.remove(loc, cp);
        side_occupancy(cp.colour) ^= loc;
        total_occupancy ^= loc;
    }
    constexpr void swap(board::Bitboard loc, board::ColouredPiece from,
                        board::ColouredPiece to) {
        state.swap(loc, from, to);
        if (from.colour != to.colour) {
            swap_oppside(loc, from, to);
        }
    }
    constexpr void swap_oppside(board::Bitboard loc, board::ColouredPiece from,
                                board::ColouredPiece to) {
        state.swap_oppside(loc, from, to);
        side_occupancy() ^= loc;
        opponent_occupancy() ^= loc;
    }
    constexpr void swap_sameside(board::Bitboard loc, board::Colour player,
                                 board::Piece from, board::Piece to) {
        state.swap_sameside(loc, player, from, to);
    }
    constexpr void remove_castling_rights(board::ColouredPiece cp) {
        state.remove_castling_rights(cp);
    }
    constexpr void remove_castling_rights(board::Colour colour) {
        state.remove_castling_rights(colour);
    }

   private:
    board::Bitboard m_side_occupancy[board::n_colours];
};
static_assert(IncrementallyUpdateable<AugmentedState>);
};  // namespace state

#endif

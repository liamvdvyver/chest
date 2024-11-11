#ifndef STATE_H
#define STATE_H

#include "board.h"
#include <string>

namespace state {

typedef std::string fen_t; // FEN strings

// Standard game setup
const fen_t new_game_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Container for en-passant square
typedef struct {
    board::square_t square;
    bool active;
} ep_t;

// Container for piece of a certain colour
typedef struct {
    board::Piece piece;
    board::Colour colour;
} coloured_piece_t;

// Container for an optional piece of a certain colour
typedef struct {
    coloured_piece_t piece;
    bool found;
} opt_coloured_piece_t;

//
// Store complete game state.
// Does not track n-fold repetitions.
//

class State {

    // Position of all pieces for each player
    board::bitboard_t m_pieces[board::n_colours][board::n_pieces];

    static constexpr int n_castling_sides = 2; // For array sizing

    // Castling rights for each player, for each side (king/queen)
    bool m_castling_rights[board::n_colours][n_castling_sides];

    // Last en-passant square, and whether current or not
    ep_t m_ep;

    // Side to move
    board::Colour to_move;

    // Plies since capture/pawn push
    int m_halfmove_clock;

    // (2-ply) moves since game start
    int m_fullmove_number;

  public:
    // Blank state
    State();

    // State from fen string
    State(const fen_t &fen_string);

    // Helper: default new game state
    static State new_game();

    // Piece position accessor
    board::bitboard_t &get_bitboard(board::Piece piece, board::Colour colour);

    // Castling rights accessor
    bool &can_castle(board::Piece side, board::Colour colour);

    // Union of piece bitboards, per side
    board::bitboard_t side_occupancy(board::Colour colour) const;

    // Union of piece bitboards, both sides
    board::bitboard_t total_occupancy() const;

    // First piece matching mask
    opt_coloured_piece_t const piece_at(board::bitboard_t bit) const;

    // Pretty printing
    std::string pretty() const;
};

std::ostream &operator<<(std::ostream &os, State s);

} // namespace state

#endif

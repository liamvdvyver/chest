#ifndef STATE_H
#define STATE_H

#include "board.h"
#include "move.h"
#include <optional>

namespace state {

typedef std::string fen_t; // FEN strings

// Standard game setup
const fen_t new_game_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Container for piece of a certain colour
struct ColouredPiece {
    board::Piece piece;
    board::Colour colour;
};

//
// Store complete game state.
// Does not track n-fold repetitions.
//

class State {

    // Position of all pieces for each player
    board::Bitboard m_pieces[board::n_colours][board::n_pieces];

    static constexpr int n_castling_sides = 2; // For array sizing

    // Castling rights for each player, for each side (king/queen)
    bool m_castling_rights[board::n_colours][n_castling_sides];

    // Last en-passant square, and whether current or not
    std::optional<board::Square> m_ep;

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

    // Side to move
    board::Colour to_move;

    board::Bitboard copy_bitboard(board::Piece piece,
                                  board::Colour colour) const;

    // Piece position accessor
    board::Bitboard &get_bitboard(board::Piece piece, board::Colour colour);

    // Castling rights accessor
    bool &can_castle(board::Piece side, board::Colour colour);

    // Union of piece bitboards, per side
    board::Bitboard side_occupancy(board::Colour colour) const;

    // Union of piece bitboards, both sides
    board::Bitboard total_occupancy() const;

    // First piece matching mask
    std::optional<ColouredPiece> const piece_at(board::Bitboard bit) const;

    // Gets all psuedolegal moves from the current position
    void get_pseudolegal_moves(std::vector<move::Move> &moves);

    // Pretty printing
    std::string pretty() const;

    bool en_passant_active() const;
    board::Square en_passant_square() const;
};

std::ostream &operator<<(std::ostream &os, State s);

} // namespace state

#endif

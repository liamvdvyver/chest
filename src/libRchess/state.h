#ifndef STATE_H
#define STATE_H

#include "board.h"

class State {

    board::Bitboard pieces[board::n_colours][board::n_pieces];
    bool castling_rights[board::n_colours][board::n_castling_sides];
    std::optional<board::Coord> en_passant_squares;
    uint8_t halfmove_clock;
    uint8_t fullmove_number;
    board::Colour to_move;

  public:
    board::Bitboard &get_bitboard(board::Piece piece, board::Colour colour);
    const std::optional<board::Coord> get_en_passant_squares() const;

    void set_bitboard(board::Piece piece, board::Colour colour,
                      board::Bitboard board);

    board::Bitboard total_occupancy();
    board::Bitboard side_occupancy(board::Colour colour);

    // Assumes no double up
    std::optional<std::pair<board::Piece, board::Colour>> const
    piece_at(board::bitboard_t bit) const;

    bool can_castle(board::Piece side, board::Colour colour) const;

    void remove_castling_rights(board::Piece side, board::Colour colour);

    State();
    State(const board::fen_t &fen_string);
};

std::ostream &operator<<(std::ostream &os, State s);

#endif

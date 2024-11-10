#ifndef MAGIC_H
#define MAGIC_H

#include "board.h"
#include <algorithm>
#include <cmath>

typedef board::bitboard_t magic_t;

typedef uint16_t magic_key_t;

class Magics {

  public:
    Magics();
    ~Magics();
    board::Bitboard get_attack_set(board::Piece piece, board::Coord coord,
                                   board::Bitboard occupancy);

  private:
    static constexpr int max_rook_shift = 12;
    static constexpr int max_bishop_shift = 9;

    static const int n_rook_attacks_keys = 1 << (max_rook_shift);
    static const int n_bishop_attack_keys = 1 << (max_bishop_shift);

    static constexpr int n_keys(board::Piece);

    board::Bitboard rook_masks[board::n_squares];
    board::Bitboard bishop_masks[board::n_squares];

    magic_t rook_magics[board::n_squares];
    magic_t bishop_magics[board::n_squares];

    board::Bitboard rook_attacks[board::n_squares][n_rook_attacks_keys];
    board::Bitboard bishop_attacks[board::n_squares][n_bishop_attack_keys];
    board::Bitboard get_attacks(board::Piece piece, board::Coord coord,
                                board::Bitboard blockers);

    magic_t *get_magic_map(board::Piece piece);
    constexpr board::Bitboard *get_attack_map(board::Piece piece,
                                              board::square_t square_no);

    const board::Bitboard &get_mask(board::Piece piece, board::Coord coord);

    magic_t get_magic(board::Piece piece, board::Coord coord);

    int get_shift_width(board::Piece piece, board::Coord coord);

    magic_key_t get_magic_key(board::Piece piece, board::Coord coord,
                              board::Bitboard occupancy);

    magic_key_t get_magic_key(board::Piece piece, board::Coord coord,
                              board::Bitboard occupancy, magic_t magic);

    void init_masks();

    void init_rook_masks();

    static inline bool in_bounds_for_bishop_mask(int i, int j);
    static inline bool in_bounds(int i, int j);

    void init_bishop_masks();

    static board::Bitboard get_rook_attacks(board::Coord coord,
                                            board::Bitboard blockers);
    static board::Bitboard get_bishop_attacks(board::Coord coord,
                                              board::Bitboard blockers);

    bool init_attacks(board::Piece piece, magic_t magic, board::Coord coord);
    bool init_rook_attacks(magic_t magic, board::Coord coord);
    bool init_bishop_attacks(magic_t magic, board::Coord coord);

    void init_magics();
};

#endif

#ifndef BOARD_H
#define BOARD_H

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

//
// Defines basic datatypes for board state
//

namespace board {

// Size of the board (number of ranks or files)
constexpr int board_size = 8;

// Size of the board (total number of squares)
constexpr int n_squares = board_size * board_size;

//
// Colour
//

enum class Colour : bool { BLACK, WHITE };

// Number of colours, for sizing arrays
static constexpr int n_colours = 2;

//
// Coordinates
//

// LERF enumeration
typedef uint8_t square_t;

// Extract file (x-coord) from LERF enumerated squre
constexpr int file(const square_t sq) { return sq % board_size; }

// Extract rank (y-coord) from LERF enumerated squre
constexpr int rank(const square_t sq) { return sq / board_size; }

// Bounds check file and rank
constexpr bool is_legal_square(const int f, const int r) {
    return (r < board_size && f < board_size && r >= 0 && f >= 0);
}

// Bounds check square number
constexpr bool is_legal_square(const square_t sq) {
    return (sq >= 0 && sq < n_squares);
}

// Enumerate LERF from cartesian coordinates
constexpr square_t to_square(int f, int r) {
    assert(is_legal_square(f, r));
    return r * board_size + f;
}

// Type of algebraic square names
typedef std::string alg_t;

// Parse (case-insensitive) algebraic notation
square_t to_square(const alg_t &sq);

// Give algebraic square name
alg_t algebraic(const square_t sq);

// LERF enumeration: explicit names
enum algebraic : square_t {
    // clang-format off
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
    // clang-format off
};

//
// Directions
//
//

enum class Direction { N, S, E, W, NE, NW, SE, SW };

//
// Bitboards
//

// LERF bitset
typedef uint64_t bitboard_t;

// Convert square number to singeton bitboard
constexpr bitboard_t to_bitboard(const square_t sq) {
    assert(is_legal_square(sq));
    return (bitboard_t)1 << (sq);
}

// Generate a full rank mask
constexpr bitboard_t rank_mask(int r) {
    assert(r >= 0 && r <= board_size);
    bitboard_t ret = 0;
    for (int f = 0; f < board_size; f++) {
        ret |= to_bitboard(to_square(f, r));
    }
    return ret;
}

// Generate a full file mask
constexpr bitboard_t file_mask(int f) {
    assert(f >= 0 && f <= board_size);
    bitboard_t ret = 0;
    for (int r = 0; r < board_size; r++) {
        ret |= to_bitboard(to_square(f, r));
    }
    return ret;
}

// Get the least significant one
constexpr bitboard_t ls1b(const bitboard_t b) { return b & -b; };

// Removes the least significant one to zero
constexpr bitboard_t reset_ls1b(const bitboard_t b) { return b & (b - 1); };

// Logical bitset difference
constexpr bitboard_t setdiff(bitboard_t b1, const bitboard_t b2) {
    b1 |= b2;
    b1 ^= b2;
    return b1;
}

// Iterate through subsets with the carry-ripler trick
constexpr bitboard_t next_subset_of(const bitboard_t subset,
                                    const bitboard_t set) {
    return (subset - set) & set;
}

// Compute cardinality with Kerighan's method
constexpr uint8_t size(bitboard_t b) {
    uint8_t ret = 0;
    while (b) {
        ret++;
        b = reset_ls1b(b);
    }
    return ret;
}

constexpr bitboard_t shift(bitboard_t b, int d_file = 0, int d_rank = 0) {
    b = d_file > 0 ? b << d_file : b >> (-d_file);
    b = d_rank > 0 ? b << (board_size * d_rank) : b >> (-board_size * d_rank);
    return b;
}

constexpr inline bitboard_t shift(bitboard_t b, Direction d) {
    switch (d) {
    case Direction::N:
        return b << board_size;
    case Direction::S:
        return b >> board_size;
    case Direction::E:
        return b << 1;
    case Direction::W:
        return b >> 1;
    case Direction::NE:
        return b << (board_size + 1);
    case Direction::NW:
        return b << (board_size - 1);
    case Direction::SE:
        return b >> (board_size - 1);
    case Direction::SW:
        return b >> (board_size + 1);
    }
}

// As above, but prevent any wrap around.
// More expensive.
constexpr bitboard_t shift_no_wrap(bitboard_t b, Direction d) {
    bitboard_t mask = 0;
    switch (d) {
    case Direction::N:
        mask = rank_mask(board_size - 1);
    case Direction::S:
        mask = rank_mask(0);
    case Direction::E:
        mask = file_mask(board_size - 1);
    case Direction::W:
        mask = file_mask(0);
    case Direction::NE:
        mask = rank_mask(board_size - 1) | file_mask(board_size - 1);
    case Direction::NW:
        mask = rank_mask(board_size - 1) | file_mask(0);
    case Direction::SE:
        mask = rank_mask(0) | file_mask(board_size - 1);
    case Direction::SW:
        mask = rank_mask(0) | file_mask(0);
    }

    return shift(setdiff(b, mask), d);
}

//
// Piece
//

enum class Piece : uint8_t { KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN };

static constexpr int n_pieces = 6; // For array sizing

// Get algebraic piece name
constexpr const char to_char(const Piece p) {
    switch (p) {
    case Piece::KING:
        return 'k';
        break;
    case Piece::QUEEN:
        return 'q';
        break;
    case Piece::BISHOP:
        return 'b';
        break;
    case Piece::KNIGHT:
        return 'n';
        break;
    case Piece::ROOK:
        return 'r';
        break;
    case Piece::PAWN:
        return 'p';
        break;
    }
}

// Parse algebraic piece name (case insensitive)
constexpr const Piece from_char(const char c) {
    switch (tolower(c)) {
    case 'k':
        return Piece::KING;
        break;
    case 'q':
        return Piece::QUEEN;
        break;
    case 'b':
        return Piece::BISHOP;
        break;
    case 'n':
        return Piece::KNIGHT;
        break;
    case 'r':
        return Piece::ROOK;
        break;
    case 'p':
        return Piece::PAWN;
        break;
    default:
        throw std::invalid_argument(std::to_string(c) +
                                    " is not a valid piece name");
    }
}

// Pretty print bitbord
std::string pretty(const bitboard_t b);

} // namespace board

#endif

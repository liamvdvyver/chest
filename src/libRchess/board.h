#ifndef BOARD_H
#define BOARD_H

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
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
    A1,
    B1,
    C1,
    D1,
    E1,
    F1,
    G1,
    H1,
    A2,
    B2,
    C2,
    D2,
    E2,
    F2,
    G2,
    H2,
    A3,
    B3,
    C3,
    D3,
    E3,
    F3,
    G3,
    H3,
    A4,
    B4,
    C4,
    D4,
    E4,
    F4,
    G4,
    H4,
    A5,
    B5,
    C5,
    D5,
    E5,
    F5,
    G5,
    H5,
    A6,
    B6,
    C6,
    D6,
    E6,
    F6,
    G6,
    H6,
    A7,
    B7,
    C7,
    D7,
    E7,
    F7,
    G7,
    H7,
    A8,
    B8,
    C8,
    D8,
    E8,
    F8,
    G8,
    H8
};

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

constexpr bitboard_t shift_up(bitboard_t b, int d = 1) {
    return b << (board_size * d);
}

constexpr bitboard_t shift_down(bitboard_t b, int d = 1) {
    return b >> (board_size * d);
}

constexpr bitboard_t shift_left(bitboard_t b, int d = 1) { return b >> d; }

constexpr bitboard_t shift_right(bitboard_t b, int d = 1) { return b << d; }

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

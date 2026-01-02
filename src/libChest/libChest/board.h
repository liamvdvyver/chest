//============================================================================//
// Board representation.
// All types are immutable through their public member functions.
// Mutable through operators only.
//============================================================================//

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "build.h"
#include "wrapper.h"

namespace board::io {};
namespace board {

// Size of the board (number of ranks or files)
constexpr const int board_size = 8;

// Size of the board (total number of squares)
constexpr int n_squares = board_size * board_size;

//============================================================================//
// Colour:
// Representation as bool, and white as true is guaranteed not to change.
//============================================================================//

enum class Colour : bool { BLACK, WHITE };

constexpr Colour operator!(const Colour c) { return (Colour) !((bool)c); };

// Number of colours, for sizing arrays
static constexpr size_t n_colours = 2;

// For iteration
static constexpr std::array<board::Colour, n_colours> colours = {Colour::BLACK,
                                                                 Colour::WHITE};

// Guaranteed not to change.
static_assert(static_cast<bool>(Colour::WHITE));

//============================================================================//
// Squares:
// Encoded by RF enumeration, i.e. from zero: A1, B1, ... H1, A2, ..., G8, H8.
//============================================================================//

using coord_t = uint8_t;
using Coords = std::pair<coord_t, coord_t>;

// LERF enumeration: max legal square is 64 (2^6).
using square_t = uint8_t;

struct Square : public Wrapper<square_t, Square> {
    using Wrapper::Wrapper;

    // Allow implicit type conversion.
    constexpr operator square_t() const { return value; };

    // Enumerate LERF from cartesian coordinates
    constexpr Square(const uint f, const uint r) : Square(r * board_size + f) {
        assert(is_legal(f, r));
    }

    // Extract file (x-coord) from LERF enumerated square
    constexpr coord_t file() const { return value % board_size; }

    // Extract rank (y-coord) from LERF enumerated square
    constexpr coord_t rank() const { return value / board_size; }

    // Extract (file, rank) / (x, y) coordinates from LERF enumerated square
    constexpr Coords coords() const { return {file(), rank()}; }

    // Bounds check file and rank
    static constexpr bool is_legal(const coord_t f, const coord_t r) {
        return (r < board_size && f < board_size);
    }

    // Bounds check square number
    constexpr bool is_legal() const { return (value < n_squares); }

    // Flip over horizonal midpoints, i.e. flip perspective.
    constexpr Square flip() const {
        static constexpr square_t A1_flipped = 56;
        return value ^ A1_flipped;
    }

    struct AllSquareIterator;

};  // namespace board

struct Square::AllSquareIterator {
    constexpr Square::AllSquareIterator begin() { return *this; }
    constexpr Square::AllSquareIterator end() const {
        return {Square(n_squares)};
    }
    constexpr operator Square() const { return sq; }
    constexpr operator Square &() { return sq; }
    constexpr Square operator*() const { return sq; }
    constexpr bool operator!=(const AllSquareIterator &other) const {
        return sq.value != other.sq.value;
    }
    constexpr Square::AllSquareIterator operator++() {
        sq.value++;
        return *this;
    }
    constexpr AllSquareIterator() = default;

   private:
    constexpr AllSquareIterator(Square sq) : sq{sq} {};
    Square sq{0};
};

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
    // clang-format on
};

//============================================================================//
// Directions
//============================================================================//

enum class Direction : uint8_t { N, S, E, W, NE, NW, SE, SW };

//============================================================================//
// Bitboards: L(ittle-)E(ndian)RF bitsets.
//============================================================================//

using bitboard_t = uint64_t;
struct Bitboard : public Wrapper<bitboard_t, Bitboard> {
    using Wrapper::Wrapper;

   public:
    // Construct singleton
    constexpr Bitboard(const Square sq) : Bitboard((bitboard_t)1 << sq) {
        assert(sq.is_legal());
    }

    // Logical bitset difference
    constexpr Bitboard setdiff(const Bitboard other) const {
        return value & ~other;
    }

    // Check membership
    constexpr bool empty() const { return !value; };

    constexpr Bitboard shift(const int d_file = 0, const int d_rank = 0) const {
        Bitboard ret = d_file > 0 ? value << d_file : value >> (-d_file);
        ret = d_rank > 0 ? ret << (board_size * d_rank)
                         : ret >> (-board_size * d_rank);
        return ret;
    }

    // Generate a full rank mask
    static constexpr Bitboard rank_mask(const coord_t r) {
        assert(r >= 0 && r <= board_size);
        static constexpr Bitboard rank_zero = 0b11111111;
        return rank_zero.shift(0, r);
    }

    // Generate a full file mask
    static constexpr Bitboard file_mask(int f) {
        assert(f >= 0 && f <= board_size);
        static constexpr Bitboard file_zero =
            0b1'00000001'00000001'00000001'00000001'00000001'00000001'00000001;
        return file_zero.shift(f, 0);
    }

    // Get the least significant one
    constexpr Bitboard ls1b() const { return value & -value; };

    // Removes the least significant one to zero
    constexpr Bitboard reset_ls1b() const { return value & (value - 1); };

    // Return the least significant one, and set it to zero
    constexpr Bitboard pop_ls1b() {
        Bitboard ret = ls1b();
        value = reset_ls1b().value;
        return ret;
    };

    // Compute cardinality with Kerighan's method, or hardware if available.
    constexpr uint8_t size() const {
#if POPCOUNT()
        return __builtin_popcountll(value);
#else
        uint8_t ret = 0;
        Bitboard b = *this;
        while (b.value) {
            ret++;
            b = b.reset_ls1b();
        }
        return ret;
#endif
    }

    // Shift once in a direction, with wrapping.
    constexpr Bitboard shift(Direction d) const {
        switch (d) {
            case Direction::N:
                return value << board_size;
            case Direction::S:
                return value >> board_size;
            case Direction::E:
                return value << 1;
            case Direction::W:
                return value >> 1;
            case Direction::NE:
                return value << (board_size + 1);
            case Direction::NW:
                return value << (board_size - 1);
            case Direction::SE:
                return value >> (board_size - 1);
            case Direction::SW:
                return value >> (board_size + 1);
        }
        std::unreachable();
    }

    // Shift once in a direction, prevent any wrap around.
    constexpr Bitboard shift_no_wrap(Direction d) const {
        return setdiff(shift_mask(d)).shift(d);
    }

    // Shift, without wrapping.
    // WARN: More expensive, requires iteration.
    constexpr Bitboard shift_no_wrap(int d_file = 0, int d_rank = 0) const {
        Bitboard ret = value;

        for (; d_rank > 0; d_rank--) {
            ret = ret.shift_no_wrap(Direction::N);
        }

        for (; d_rank < 0; d_rank++) {
            ret = ret.shift_no_wrap(Direction::S);
        }

        for (; d_file > 0; d_file--) {
            ret = ret.shift_no_wrap(Direction::E);
        }

        for (; d_file < 0; d_file++) {
            ret = ret.shift_no_wrap(Direction::W);
        }

        return ret;
    }

    // Assumes b is a power of two (i.e. a singly occupied bitboard)
    constexpr Square single_bitscan_forward() const {
#if BITSCAN()
        return __builtin_ctzll(value);
#else
        return de_brujin_map[((value ^ (value - 1)) * debruijn64) >> 58];
#endif
    }

    std::string pretty() const;

    // Iterate through power set
    struct SubsetIterator;
    constexpr SubsetIterator subsets() const;

    // Iterate through singleton elements
    struct ElementIterator;
    constexpr ElementIterator singletons() const;

    // bitboard_t board;

   private:
    static constexpr bitboard_t debruijn64 = 0x03f79d71b4cb0a89;
    static constexpr std::array<uint8_t, n_squares> de_brujin_map{
        0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61,
        54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4,  62,
        46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5,  63};

    // Iterate through subsets with the carry-ripler trick
    constexpr Bitboard next_subset_of(const Bitboard superset) const {
        return (value - superset.value) & superset.value;
    }

    // Mask rank/file for single shift
    constexpr Bitboard shift_mask(Direction d) const {
        switch (d) {
            case Direction::N:
                return rank_mask(board_size - 1);
            case Direction::S:
                return rank_mask(0);
            case Direction::E:
                return file_mask(board_size - 1);
            case Direction::W:
                return file_mask(0);
            case Direction::NE:
                return rank_mask(board_size - 1) | file_mask(board_size - 1);
            case Direction::NW:
                return rank_mask(board_size - 1) | file_mask(0);
            case Direction::SE:
                return rank_mask(0) | file_mask(board_size - 1);
            case Direction::SW:
                return rank_mask(0) | file_mask(0);
            default:
                std::unreachable();
        }
    }
};

//----------------------------------------------------------------------------//
// Iteration
//----------------------------------------------------------------------------//
struct Bitboard::SubsetIterator {
    constexpr SubsetIterator(Bitboard b) : val(0), b(b), done(false) {};

    // Not semantically correct, only used for ranges
    // No need to perform comparison to determine if == end()
    constexpr bool operator!=(SubsetIterator const &other) {
        (void)other;  // unused
        return !done || !val.empty();
    }
    constexpr const SubsetIterator begin() { return *this; }
    constexpr const SubsetIterator end() { return SubsetIterator(0, 0, true); }
    constexpr operator Bitboard() const { return val; }
    constexpr operator Bitboard &() { return val; }
    constexpr Bitboard operator*() const { return val; }
    constexpr Bitboard &operator++() {
        val = val.next_subset_of(b);
        done = true;
        return *this;
    }

   private:
    constexpr SubsetIterator(Bitboard val, Bitboard b, bool done)
        : val(val), b(b), done(done) {};
    Bitboard val;
    Bitboard b;
    bool done;  // When we see the empty set, is it for the first time?
};

constexpr Bitboard::SubsetIterator Bitboard::subsets() const { return {*this}; }

struct Bitboard::ElementIterator {
    constexpr ElementIterator(Bitboard b) : val(b) {};
    constexpr bool operator!=(ElementIterator const &other) {
        return val != other.val;
    }
    constexpr const ElementIterator begin() { return *this; }
    constexpr const ElementIterator end() { return ElementIterator(0); }
    constexpr operator Bitboard() const { return val.ls1b(); }
    constexpr Bitboard operator*() const { return val.ls1b(); }
    constexpr Bitboard operator++() { return val.pop_ls1b(); }

   private:
    Bitboard val;
};

constexpr Bitboard::ElementIterator Bitboard::singletons() const {
    return {*this};
}

//============================================================================//
// Pieces
//============================================================================//

// Requirements on ordering:
// * Should follow same order as promoted pieces in move.h
// * King must be last, as in eval.h.
// * Should be in order of value for move ordering.
enum class Piece : uint8_t { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

static constexpr size_t n_pieces = 6;  // For array sizing

// Container for piece of a certain colour
struct ColouredPiece {
    board::Colour colour;
    board::Piece piece;
};

// For iteration, avoid memory lookups
struct PieceTypesIterator {
    constexpr PieceTypesIterator begin() { return *this; }
    constexpr PieceTypesIterator end() const {
        return {static_cast<Piece>(n_pieces)};
    }
    constexpr operator Piece() const { return p; }
    constexpr operator Piece &() { return p; }
    constexpr Piece operator*() const { return p; }
    constexpr bool operator!=(const PieceTypesIterator &other) {
        return p != other.p;
    }
    constexpr PieceTypesIterator operator++() {
        p = static_cast<Piece>(static_cast<uint8_t>(p) + 1);
        return *this;
    }

    constexpr PieceTypesIterator() = default;

   private:
    constexpr PieceTypesIterator(Piece p) : p{p} {};
    Piece p{0};
};

//============================================================================//
// IO, e.g. pretty printing (in implementation file).
//============================================================================//
namespace io {

// Type of algebraic square names
using alg_t = std::string;

// Parse (case-insensitive) algebraic notation
board::Square to_square(const alg_t &sq);

// Give algebraic square name
alg_t algebraic(const Square sq);

// Get algebraic piece name
//
// TODO: lookup into array
constexpr char to_char(const Piece p) {
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

    return (0);  // silence warnings
}

// Unicode representations.
// WARN: assumes black (terminal) background.
constexpr std::string to_uni(const ColouredPiece cp) {
    switch (cp.colour) {
        case Colour::WHITE:
            switch (cp.piece) {
                case Piece::KING:
                    return "♚";
                case Piece::QUEEN:
                    return "♛";
                case Piece::BISHOP:
                    return "♝";
                case Piece::KNIGHT:
                    return "♞";
                case Piece::ROOK:
                    return "♜";
                case Piece::PAWN:
                    return "♟";
                default:
                    std::unreachable();
            }
        case Colour::BLACK:
            switch (cp.piece) {
                case Piece::KING:
                    return "♔";
                case Piece::QUEEN:
                    return "♕";
                case Piece::BISHOP:
                    return "♗";
                case Piece::KNIGHT:
                    return "♘";
                case Piece::ROOK:
                    return "♖";
                case Piece::PAWN:
                    return "♙";
                default:
                    std::unreachable();
            }
    }
};

// Parse algebraic piece name (case insensitive)
constexpr Piece from_char(const char c) {
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

}  // namespace io

//============================================================================//
// Rank constants: for convinience
//============================================================================//

namespace ranks {

constexpr uint8_t home_rank(Colour c) {
    return static_cast<bool>(c) ? 0 : board_size - 1;
};
constexpr uint8_t pawn_rank(Colour c) {
    return static_cast<bool>(c) ? 1 : board_size - 2;
};
constexpr uint8_t push_rank(Colour c) {
    return static_cast<bool>(c) ? 2 : board_size - 3;
};
constexpr uint8_t double_push_rank(Colour c) {
    return static_cast<bool>(c) ? 3 : board_size - 4;
};
constexpr uint8_t pre_promote_rank(Colour c) {
    return static_cast<bool>(c) ? board_size - 2 : 1;
};
constexpr uint8_t back_rank(Colour c) {
    return static_cast<bool>(c) ? board_size - 1 : 0;
};
}  // namespace ranks

}  // namespace board

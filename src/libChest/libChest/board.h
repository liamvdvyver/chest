#ifndef BOARD_H
#define BOARD_H

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "wrapper.h"

//
// Defines basic datatypes for board state
//

namespace board::io {};
namespace board {

// Size of the board (number of ranks or files)
constexpr int board_size = 8;

// Size of the board (total number of squares)
constexpr int n_squares = board_size * board_size;

//
// Colour
//

enum class Colour : bool { BLACK, WHITE };

constexpr Colour operator!(const Colour c) { return (Colour) !((bool)c); };

// Number of colours, for sizing arrays
static constexpr int n_colours = 2;

//
// Coordinates
//

// LERF enumeration
typedef uint8_t square_t;

struct Square : Wrapper<square_t, Square> {

    using Wrapper::Wrapper;
    constexpr Square(const Wrapper &w) : Wrapper(w) {};

    // Convert (unwrap) type
    constexpr operator square_t() const { return value; };

    // Enumerate LERF from cartesian coordinates
    constexpr Square(uint f, uint r) : Square(r * board_size + f) {
        assert(is_legal(f, r));
    }

    // Extract file (x-coord) from LERF enumerated squre
    constexpr uint8_t file() const { return value % board_size; }

    // Extract rank (y-coord) from LERF enumerated squre
    constexpr int8_t rank() const { return value / board_size; }

    // Bounds check file and rank
    static constexpr bool is_legal(const uint f, const uint r) {
        return (r < board_size && f < board_size);
    }

    // Bounds check square number
    constexpr bool is_legal() const { return (value < n_squares); }

    struct AllSquareIterator;

}; // namespace board

struct Square::AllSquareIterator {
    constexpr Square::AllSquareIterator begin() { return *this; }
    constexpr Square::AllSquareIterator end() const {
        return AllSquareIterator(Square(n_squares));
    }
    constexpr operator Square() const { return sq; }
    constexpr operator Square &() { return sq; }
    constexpr Square operator*() const { return sq; }
    constexpr bool operator!=(const AllSquareIterator &other) {
        return sq.value != other.sq.value;
    }
    constexpr Square::AllSquareIterator operator++() {
        sq.value++;
        return *this;
    }
    constexpr AllSquareIterator() {};

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

// Thin wrapper around a bitboard_t (uint64_t)
//
// All methods are contexpr,
// everything is immutable
struct Bitboard : Wrapper<bitboard_t, Bitboard> {

    using Wrapper::Wrapper;
    constexpr Bitboard(const Wrapper &v) : Wrapper(v) {};

  public:
    // Construction
    constexpr Bitboard(const Square sq) : Bitboard((bitboard_t)1 << sq) {
        assert(sq.is_legal());
    }

    // constexpr bitboard_t operator () () {
    //     return board;
    // };

    // Logical bitset difference
    constexpr Bitboard setdiff(const Bitboard other) const {
        return value & ~other;
    }

    // Check membership
    constexpr bool empty() const { return !value; };

    // Generate a full rank mask
    static constexpr Bitboard rank_mask(int r) {
        assert(r >= 0 && r <= board_size);
        Bitboard ret{(bitboard_t)0};
        for (int f = 0; f < board_size; f++) {
            ret |= Bitboard(Square(f, r)).value;
        }
        return ret;
    }

    // Generate a full file mask
    static constexpr Bitboard file_mask(int f) {
        assert(f >= 0 && f <= board_size);
        Bitboard ret{0};
        for (int r = 0; r < board_size; r++) {
            ret |= Bitboard(Square(f, r));
        }
        return ret;
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

    // Compute cardinality with Kerighan's method
    constexpr uint8_t size() const {
#if __has_builtin(__builtin_popcountll)
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

    constexpr Bitboard shift(int d_file = 0, int d_rank = 0) const {
        Bitboard ret = d_file > 0 ? value << d_file : value >> (-d_file);
        ret.value = d_rank > 0 ? ret.value << (board_size * d_rank)
                               : ret.value >> (-board_size * d_rank);
        return ret;
    }

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

    // As above, but prevent any wrap around.
    // More expensive.
    constexpr Bitboard shift_no_wrap(Direction d) const {
        return setdiff(shift_mask(d)).shift(d);
    }

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

    // TODO: profile
    // Might be very slow
    // for the moment, probably pre-generate/cache moves and look up as needed
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
#if __has_builtin(__builtin_ctzll)
        return __builtin_ctzll(value);
#else
        return de_brujin_map[((value ^ (value - 1)) * debruijn64) >> 58];
#endif
    }

    std::string pretty() const;

    struct SubsetIterator;
    constexpr SubsetIterator subsets() const;

    struct ElementIterator;
    constexpr ElementIterator singletons() const;

    // bitboard_t board;

  private:
    static const bitboard_t debruijn64 = 0x03f79d71b4cb0a89;
    static constexpr int de_brujin_map[64] = {
        0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61,
        54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4,  62,
        46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5,  63};

    // Iterate through subsets with the carry-ripler trick
    constexpr Bitboard next_subset_of(const Bitboard superset) const {
        return (value - superset.value) & superset.value;
    }
};

//
// Subset iteration
//
struct Bitboard::SubsetIterator {
    constexpr SubsetIterator(Bitboard b) : val(0), b(b), done(false) {};

    // Not semantically correct, only used for ranges
    // No need to perform comparison to determine if == end()
    constexpr bool operator!=(SubsetIterator const &other) {
        (void)other; // unused
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
    bool done; // When we see the empty set, is it for the first time?
};

constexpr Bitboard::SubsetIterator Bitboard::subsets() const {
    return Bitboard::SubsetIterator(*this);
}

//
// Element iteration
//
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
    return Bitboard::ElementIterator(*this);
}

//
// Piece
//

// Lowest valued members should follow same order as promoted pieces in move.h
// King must be last, as in eval.h
enum class Piece : uint8_t { KNIGHT, BISHOP, ROOK, QUEEN, PAWN, KING };

static constexpr int n_pieces = 6; // For array sizing

// Container for piece of a certain colour
struct ColouredPiece {
    board::Colour colour;
    board::Piece piece;
};

// IO in implementation
namespace io {

// Type of algebraic square names
typedef std::string alg_t;

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

    return (0); // silence warnings
}

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

} // namespace io

//
// Rank constants: for convinience
//

constexpr uint8_t home_rank[n_colours]{board_size - 1, 0};
constexpr uint8_t pawn_rank[n_colours]{board_size - 2, 1};
constexpr uint8_t push_rank[n_colours]{board_size - 3, 2};
constexpr uint8_t double_push_rank[n_colours]{board_size - 4, 3};
constexpr uint8_t pre_promote_rank[n_colours]{1, board_size - 2};
constexpr uint8_t back_rank[n_colours]{0, board_size - 1};

} // namespace board

#endif

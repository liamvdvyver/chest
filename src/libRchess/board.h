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

// using namespace std;

namespace board {

constexpr int board_size = 8;
constexpr int n_pieces = 6;
constexpr int n_colours = 2;
constexpr int n_castling_sides = 2;
constexpr int n_squares = board_size * board_size;

typedef uint64_t bitboard_t;
typedef uint8_t square_t;
typedef uint16_t move_t;
typedef std::string fen_t;
typedef std::string alg_t;

enum class Colour : bool { BLACK, WHITE };

class Coord {
    square_t value;

  public:
    Coord(const square_t board_no) : value{board_no} {
        if (value >= n_squares)
            throw std::invalid_argument("Too large!");
    };

    Coord(const int x, const int y) : Coord(y * board_size + x) {
        if (x >= board_size || y >= board_size)
            throw std::invalid_argument("Too large!");
        if (x < 0 || y < 0)
            throw std::invalid_argument("Too small!");
    };

    Coord(const alg_t &square_name) {
        if (square_name.length() != 2) {
            std::invalid_argument("Coord(const alg_t &)");
        }

        int x = (tolower(square_name.at(0)) - 'a');
        int y = (square_name.at(1) - '1');

        if (x >= board_size || y >= board_size)
            throw std::invalid_argument("Too large!");
        if (x < 0 || y < 0)
            throw std::invalid_argument("Too small!");

        value = y * board_size + x;
    }

    int get_x() const { return value % board_size; }

    int get_y() const { return value / board_size; }

    square_t get_square() const { return value; }

    alg_t algerbraic() {
        alg_t ret = "";

        ret += get_x() + 'A';
        ret += get_y() + '1';

        return ret;
    }
};

class Bitboard {

    bitboard_t board = 0;

    // static utils
    static bitboard_t ls1b(const bitboard_t b) { return b & -b; };
    static bitboard_t reset_ls1b(const bitboard_t b) { return b & (b - 1); };
    static bool is_empty(const bitboard_t b) { return b == 0; };

    static bitboard_t of(const Coord coord) {
        if (coord.get_square() >= n_squares) {
            throw std::invalid_argument("coordinate must be less than " +
                                        std::to_string(n_squares));
        }
        return (bitboard_t)1 << coord.get_square();
    };

  public:
    Bitboard(bitboard_t board) : board{board} {};
    Bitboard(){};
    Bitboard(const Coord c) : Bitboard(of(c)){};

    void add(const Coord c) { add_all(of(c)); };

    void rem(const Coord c) { rem_all(of(c)); };

    void add_all(const Bitboard &b) { board |= b.get_board(); }
    void rem_all(const Bitboard &b) {
        board |= b.get_board();
        board ^= b.get_board();
    }

    // Kernighan's method
    uint8_t size() const {
        uint8_t ret = 0;
        bitboard_t board_cpy = board;
        while (!is_empty(board_cpy)) {
            ret++;
            board_cpy = reset_ls1b(board_cpy);
        }
        return ret;
    }

    bitboard_t get_board() const { return board; };

    // Iterate over subsets using Carry-Ripler trick
    // TODO: implement as iterator
    // this->board becomes the next subset of b
    // will wrap back around to 0
    void next_subset_of(const Bitboard b) {
        board = (board - b.get_board()) & b.get_board();
    }
};

enum class Piece : uint8_t { KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN };

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

constexpr const Piece from_char(const char ch) {
    switch (tolower(ch)) {
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
        throw std::invalid_argument(std::to_string(ch) +
                                    " is not a valid piece name");
    }
}

std::ostream &operator<<(std::ostream &os, const Bitboard &b);

} // namespace board

#endif

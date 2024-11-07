#ifndef BOARD_H
#define BOARD_H

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

// using namespace std;

#define BOARD_SIZE 8
#define LG_BOARD_SIZE 3
#define N_PIECES 6
#define N_COLOURS 2
#define N_CASTLING_SIDES 2

constexpr int n_squares = BOARD_SIZE * BOARD_SIZE;

typedef uint64_t bitboard_t;
typedef uint8_t square_t;
typedef uint16_t move_t;
typedef std::string fen_t;

class Coord {
  square_t value;

public:
  Coord(const square_t board_no) : value{board_no} {};
  Coord(const int x, const int y) : Coord(y * BOARD_SIZE + x){};

  int get_x() const { return value % BOARD_SIZE; }

  int get_y() const { return value / BOARD_SIZE; }

  square_t get_square() const { return value; }
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
};

enum class Piece { KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN };

const char to_char(const Piece p);

const Piece from_char(const char ch);

enum class Colour { WHITE, BLACK };

class State {

  Bitboard pieces[N_COLOURS][N_PIECES];
  bool castling_rights[N_COLOURS][N_CASTLING_SIDES];

public:
  Bitboard &get_bitboard(Piece piece, Colour colour) {
    return pieces[(int)colour][(int)piece];
  }

  void set_bitboard(Piece piece, Colour colour, Bitboard board) {
    pieces[(int)colour][(int)piece] = board;
  }

  // Assumes no double up
  std::optional<std::pair<Piece, Colour>> const piece_at(bitboard_t bit) const {
    for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
      for (int pieceIdx = 0; pieceIdx < N_PIECES; pieceIdx++) {

        if ((pieces[colourIdx][pieceIdx].get_board() & bit) != 0) {
          return std::pair((Piece)pieceIdx, (Colour)colourIdx);
        }
      }
    }
    return std::optional<std::pair<Piece, Colour>>();
  }

  bool can_castle(Piece side, Colour colour) const {
    if (side != Piece::QUEEN && side != Piece::KING)
      throw std::invalid_argument("Side must be KING or QUEEN");

    return castling_rights[(int)side][(int)colour];
  }

  void remove_castling_rights(Piece side, Colour colour) {
    if (side != Piece::QUEEN && side != Piece::KING)
      throw std::invalid_argument("Side must be KING or QUEEN");

    castling_rights[(int)side][(int)colour] = false;
  }

  // Assume no capture
  // void make_move(const move_t &move, const Piece &piece, const Colour
  // &colour,
  //                const Piece &captured_piece) {
  //   get_bitboard(piece, colour).rem(move.first);
  //   get_bitboard(piece, colour).add(move.first);
  // }

  State() {

    // give castling rights
    for (int i = 0; i < N_COLOURS; i++) {
      for (int j = 0; j < N_CASTLING_SIDES; j++) {
        castling_rights[i][j] = true;
      }
    };

    // init to empty board
    memset(pieces, 0, sizeof(pieces));
  }

  State(const fen_t &fen_string) {

    // TODO: get rest of fen string
    const std::string &placements = fen_string;

    int rowIdx = (BOARD_SIZE - 1), colIdx = 0;

    for (int charIdx = 0; charIdx < placements.length(); charIdx++) {

      assert(rowIdx <= BOARD_SIZE);
      assert(colIdx <= BOARD_SIZE);

      if (isdigit(placements[charIdx])) {
        int n_space = stoi(placements.substr(charIdx, 1));
        colIdx += n_space;

      } else if (placements[charIdx] == '/') {
        assert(colIdx == BOARD_SIZE);
        colIdx = 0;
        rowIdx--;

      } else {

        assert(isalpha(placements[charIdx]));
        Colour colour =
            isupper(placements[charIdx]) ? Colour::WHITE : Colour::BLACK;
        Piece piece = from_char(placements[charIdx]);
        get_bitboard(piece, colour).add(Coord(colIdx, rowIdx));
        colIdx++;
      };
    }
  }
};

std::ostream &operator<<(std::ostream &os, State s);

std::ostream &operator<<(std::ostream &os, const Bitboard &b);

#endif

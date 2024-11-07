#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "board.h"

const char to_char(const Piece p) {
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

const Piece from_char(const char ch) {
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

// PRETTY PRINTING

std::ostream &operator<<(std::ostream &os, State s) {
  std::string ret = "";
  for (int r = BOARD_SIZE - 1; r >= 0; r--) {
    for (int c = 0; c < BOARD_SIZE; c++) {

      bitboard_t b1 = Bitboard(Coord(c, r)).get_board();
      Bitboard b = Bitboard(Coord(c, r)).get_board();

      std::optional<std::pair<Piece, Colour>> atLoc =
          s.piece_at(Bitboard(Coord(c, r)).get_board());
      if (atLoc.has_value()) {
        char retChar = to_char(atLoc->first);
        if (atLoc->second == Colour::WHITE) {
          retChar = toupper(retChar);
        }
        ret += retChar;
      } else
        ret += ".";
    }
    ret += "\n";
  }
  return (os << ret);
};

std::ostream &operator<<(std::ostream &os, const Bitboard &b) {
  std::string ret = "";
  for (int r = BOARD_SIZE - 1; r >= 0; r--) {
    for (int c = 0; c < BOARD_SIZE; c++) {
      switch (1 & b.get_board() >> (BOARD_SIZE * r + c)) {
      case 1:
        ret += "1";
        break;
      case 0:
        ret += ".";
        break;
      }
    }
    ret += "\n";
  }
  return (os << ret);
}

int main(int argc, char **argv) {
  State s = std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
  std::cout << s;
  s.get_bitboard(Piece::PAWN, Colour::WHITE).rem(Coord(5, 1));
  std::cout << s;
};

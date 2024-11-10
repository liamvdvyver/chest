#include "libRchess/board.h"
#include "libRchess/magic.h"
#include "libRchess/state.h"
#include <iostream>
#include <ostream>
#include <string>

using namespace board;

int main(int argc, char **argv) {
    std::cout << std::to_string(sizeof(Magics)) << std::endl;
    Magics m = Magics();
    State s = State("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    std::cout << s.get_en_passant_squares().value_or(Coord(7, 7)).algerbraic()
              << std::endl;
    std::cout << Bitboard(s.get_en_passant_squares().value_or(Coord(7, 7)))
              << std::endl;
    Bitboard occ = s.total_occupancy();
    std::cout << occ;
    std::cout << m.get_attack_set(Piece::ROOK, Coord(4, 4), occ);
    std::cout << m.get_attack_set(Piece::BISHOP, Coord(3, 5), occ);
}

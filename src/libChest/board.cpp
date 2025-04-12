#include "board.h"

//
// Defines IO for board state
// all other code is contexpr in the main module.
//

board::Square board::io::to_square(const alg_t &sq) {
    if (sq.length() != 2) {
        std::invalid_argument("Coord(const alg_t &)");
    }

    int rank = (tolower(sq.at(0)) - 'a');
    int file = (sq.at(1) - '1');

    if (!board::Square::is_legal(rank, file)) {
        throw std::invalid_argument("to_square(const alg_t &)");
    };

    return Square(rank, file);
}

board::io::alg_t algebraic(const board::Square sq) {
    board::io::alg_t ret = "";

    ret += sq.file() + 'A';
    ret += sq.rank() + '1';

    return ret;
}

std::string board::Bitboard::pretty() const {
    std::string ret = "";
    for (int r = board::board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board::board_size; c++) {
            switch (1 & board >> (board::board_size * r + c)) {
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
    return ret;
}

#include "board.h"

namespace board {

square_t to_square(const alg_t &sq) {
    if (sq.length() != 2) {
        std::invalid_argument("Coord(const alg_t &)");
    }

    int rank = (tolower(sq.at(0)) - 'a');
    int file = (sq.at(1) - '1');

    if (!is_legal_square(rank, file)) {
        throw std::invalid_argument("to_square(const alg_t &)");
    };

    return to_square(rank, file);
}

alg_t algebraic(const square_t sq) {
    alg_t ret = "";

    ret += file(sq) + 'A';
    ret += rank(sq) + '1';

    return ret;
}

std::string pretty(const bitboard_t b) {
    std::string ret = "";
    for (int r = board::board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board::board_size; c++) {
            switch (1 & b >> (board::board_size * r + c)) {
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
} // namespace board

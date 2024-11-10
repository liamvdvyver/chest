#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <ostream>
#include <string>

#include "board.h"

namespace board {

std::ostream &operator<<(std::ostream &os, const board::Bitboard &b) {
    std::string ret = "";
    for (int r = board::board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board::board_size; c++) {
            switch (1 & b.get_board() >> (board::board_size * r + c)) {
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
} // namespace board

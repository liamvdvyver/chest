#include "libRchess/board.h"
#include "libRchess/magic.h"
#include "libRchess/state.h"
#include <iostream>
#include <ostream>
#include <string>

using namespace board;
using namespace std;
using namespace state;

int main(int argc, char **argv) {
    State s = State(new_game_fen);
    Magics m = Magics();
    cout << pretty(m.get_attack_set(board::Piece::BISHOP, to_square(4, 4), s.total_occupancy()));
}

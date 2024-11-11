#include "libChest/board.h"
#include "libChest/magic.h"
#include "libChest/state.h"
#include <iostream>

using namespace board;
using namespace std;
using namespace state;
using namespace move::magic;

int main(int argc, char **argv) {
    State s = State(new_game_fen);
    Magics m = Magics();
    cout << pretty(m.get_attack_set(board::Piece::BISHOP, to_square(4, 4), s.total_occupancy()));
}

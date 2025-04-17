#include "libChest/board.h"
#include "libChest/move.h"
#include "libChest/movegen.h"
#include "libChest/state.h"
#include <iostream>
#include <vector>

int main(int argc, char **argv) {

    // Load up a new game
    state::State st = state::State::new_game();
    std::cout << st.pretty() << std::endl;

    // Get a MoveGenerator
    move::movegen::AllMoveGenerator mover;

    // Get all starting moves
    std::vector<move::Move> mvs{};
    mover.get_all_moves(st, mvs);

    // All the generated moves
    for (auto &mv : mvs) {
        std::cout
            << (board::Bitboard(mv.from()) | board::Bitboard(mv.to())).pretty()
            << std::endl;
    }
}

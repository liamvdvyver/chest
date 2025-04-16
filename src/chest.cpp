#include "libChest/attack.h"
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

    // Find some pawn moves
    move::movegen::PawnMoveGenerator<board::Colour::WHITE> white_pawn_mover;
    std::vector<move::Move> mv;
    white_pawn_mover.get_quiet_moves(st, mv);

    // Count moves
    std::cout << mv.size() << std::endl;

    // Generate magic bitboards (look how quick!)
    move::attack::BishopAttackGenerator bishop_magics;
    move::attack::RookAttackGenerator rook_magics;

    // Look how correct!
    std::cout << rook_magics
                     .get_attack_set(board::Square(4, 4), st.total_occupancy())
                     .pretty()
              << std::endl;
}

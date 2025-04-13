#include "move.h"

namespace move {

Move::Move(board::Square from, board::Square to, MoveType type) {
    value = ((sixbit_mask & (move_t)from) << from_offset) |
            ((sixbit_mask & (move_t)to) << to_offset) |
            (fourbit_mask & (move_t)type);
}

} // namespace move

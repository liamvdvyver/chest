#include "move.h"

namespace move {

Move::Move(board::Square from, board::Square to, MoveType type) {
    m_move = ((sixbit_mask & from.square) << from_offset) |
             ((sixbit_mask & to.square) << to_offset) | (fourbit_mask & (move_t)type);
}

} // namespace move

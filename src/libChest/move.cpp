#include "move.h"
#include "board.h"

namespace move {

Move::Move(board::square_t from, board::square_t to, MoveType type) {
    m_move = ((sixbit_mask & from) << from_offset) |
             ((sixbit_mask & to) << to_offset) | (fourbit_mask & (move_t)type);
}

// Gets the origin square
constexpr board::square_t Move::from() const {
    return (sixbit_mask & (m_move >> from_offset));
}

// Gets the destination square
constexpr board::square_t Move::to() const {
    return (sixbit_mask & (m_move >> to_offset));
}

// Gets the move type
constexpr MoveType Move::type() const {
    return (MoveType)(m_move & fourbit_mask);
};
} // namespace move

#ifndef MOVE_H
#define MOVE_H

#include "board.h"

namespace move {

// All information required for minimal move
typedef uint16_t move_t;

//
// Move types
//

// All information additional to origin/destination.
// Must fit in a nibble (half a byte).
enum class MoveType {
    NORMAL = 0,
    CAPTURE,
    CASTLE_QUEENSIDE,
    CASTLE_KINGSIDE,
    SINGLE_PUSH,
    DOUBLE_PUSH,
    CAPTURE_EP,
    PROMOTE_KNIGHT,
    PROMOTE_ROOK,
    PROMOTE_BISHOP,
    PROMOTE_QUEEN,
    PROMOTE_CAPTURE_KNIGHT,
    PROMOTE_CAPTURE_ROOK,
    PROMOTE_CAPTURE_BISHOP,
    PROMOTE_CAPTURE_QUEEN,
    MAX
};

// Is material captured?
constexpr bool is_capture(MoveType type) {
    switch (type) {
    case MoveType::CAPTURE:
        return true;
    case MoveType::CAPTURE_EP:
        return true;
    case MoveType::PROMOTE_CAPTURE_KNIGHT:
        return true;
    case MoveType::PROMOTE_CAPTURE_ROOK:
        return true;
    case MoveType::PROMOTE_CAPTURE_BISHOP:
        return true;
    case MoveType::PROMOTE_CAPTURE_QUEEN:
        return true;
    default:
        return false;
    }
};

// Is a pawn promoted?
constexpr bool is_promotion(MoveType type) {
    switch (type) {
    case MoveType::PROMOTE_KNIGHT:
        return true;
    case MoveType::PROMOTE_ROOK:
        return true;
    case MoveType::PROMOTE_BISHOP:
        return true;
    case MoveType::PROMOTE_QUEEN:
        return true;
    case MoveType::PROMOTE_CAPTURE_KNIGHT:
        return true;
    case MoveType::PROMOTE_CAPTURE_ROOK:
        return true;
    case MoveType::PROMOTE_CAPTURE_BISHOP:
        return true;
    case MoveType::PROMOTE_CAPTURE_QUEEN:
        return true;
    default:
        return false;
    }
};

// Is this a castle?
constexpr bool is_castle(MoveType type) {
    return type == (MoveType::CASTLE_KINGSIDE) ||
           type == (MoveType::CASTLE_QUEENSIDE);
}

// What is the result of promotion?
// Assumes move is a promotion (otherwise UB).
constexpr board::Piece promoted_piece(MoveType type) {
    switch (type) {
    case MoveType::PROMOTE_KNIGHT:
        return board::Piece::KNIGHT;
    case MoveType::PROMOTE_ROOK:
        return board::Piece::ROOK;
    case MoveType::PROMOTE_BISHOP:
        return board::Piece::BISHOP;
    case MoveType::PROMOTE_QUEEN:
        return board::Piece::QUEEN;
    case MoveType::PROMOTE_CAPTURE_KNIGHT:
        return board::Piece::KNIGHT;
    case MoveType::PROMOTE_CAPTURE_ROOK:
        return board::Piece::ROOK;
    case MoveType::PROMOTE_CAPTURE_BISHOP:
        return board::Piece::BISHOP;
    case MoveType::PROMOTE_CAPTURE_QUEEN:
        return board::Piece::QUEEN;
    default:
        return (board::Piece)0;
    }
}

//
// Encodes any move, legal or otherwise
//

class Move {

    // Contains all neccesary information
    move_t m_move = 0;

    //
    // Size checking
    //

    static constexpr int move_width = 16;  // Bits in a move
    static constexpr int square_width = 6; // Bits needed to encode a square
    static constexpr int extra_width = 4;  // Remaining bits for MoveType

    // LSB of origin
    static constexpr int from_offset = move_width - square_width;

    // LSB of destination
    static constexpr int to_offset = from_offset - square_width;

    // LSB of MoveType
    static constexpr int extra_offset = to_offset - extra_width;

    // Check sizes are correct
    static_assert(extra_offset == 0);

    // Static bitmask helpers
    static constexpr int sixbit_mask = 0b111111;
    static constexpr int fourbit_mask = 0b1111;

    // Check extra info fits in the space we have for it
    static_assert((int)MoveType::MAX < (1 << (extra_width)));

  public:
    Move(board::Square from, board::Square to,
         MoveType type = MoveType::NORMAL);

    // Gets the origin square
    constexpr board::Square from() const {
        return (sixbit_mask & (m_move >> from_offset));
    }

    // Gets the destination square
    constexpr board::Square to() const {
        return (sixbit_mask & (m_move >> to_offset));
    }

    // Gets the move type
    constexpr MoveType type() const {
        return (MoveType)(m_move & fourbit_mask);
    };
};

} // namespace move

#endif

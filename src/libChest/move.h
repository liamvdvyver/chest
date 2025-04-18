#ifndef MOVE_H
#define MOVE_H

#include "board.h"
#include "wrapper.h"

namespace move {

// All information required for minimal move
typedef uint16_t move_t;

//
// Move types
//

// All information additional to origin/destination.
// Must fit in a nibble (half a byte).
//
// Encoded as follows (checked with static asserts):
// TODO: write static asserts
//
// type[0-1]: information
// type[3]:   capture flag
// type[2]:   promotion flag
//
// MoveType != 0 iff move is irreversible (resets halfmove clock)
// Can just sort by movetype to get a rough move ordering,
// filter zeros for loud-line generation.
typedef int movetype_t;
enum class MoveType : movetype_t {
    // clang-format off
    /*0b0000*/ NORMAL                 = 0b0000,
    /*0b0001*/ CASTLE                 = 0b0001,
    /*0b0010*/ SINGLE_PUSH            = 0b0010,
    /*0b0011*/ DOUBLE_PUSH            = 0b0011,
    /*0b0100*/ PROMOTE_KNIGHT         = 0b0100,
    /*0b0101*/ PROMOTE_BISHOP         = 0b0101,
    /*0b0110*/ PROMOTE_ROOK           = 0b0110,
    /*0b0111*/ PROMOTE_QUEEN          = 0b0111,
    /*0b1000*/ CAPTURE                = 0b1000,
    /*0b1001*/ CAPTURE_EP             = 0b1001,
    /*0b1010*/
    /*0b1011*/
    /*0b1100*/ PROMOTE_CAPTURE_KNIGHT = 0b1100,
    /*0b1101*/ PROMOTE_CAPTURE_BISHOP = 0b1101,
    /*0b1110*/ PROMOTE_CAPTURE_ROOK   = 0b1110,
    /*0b1111*/ PROMOTE_CAPTURE_QUEEN  = 0b1111,
    // clang-format on
};

// TODO: determine at compile time?
static constexpr MoveType max = MoveType::PROMOTE_CAPTURE_QUEEN;
static constexpr movetype_t capture_mask = 0b1000;
static constexpr movetype_t promo_flag_mask = 0b0100;
static constexpr movetype_t promo_move_mask = 0b0011;

// Is material captured?
constexpr bool is_capture(MoveType type) {
    return capture_mask & (movetype_t)type;
};

// Is a pawn promoted?
constexpr bool is_promotion(MoveType type) {
    return promo_flag_mask & (movetype_t)type;
};

// Is this a castle?
constexpr bool is_castle(MoveType type) { return type == MoveType::CASTLE; }

// What is the result of promotion?
// Assumes move is a promotion (otherwise UB).
constexpr board::Piece promoted_piece(MoveType type) {
    assert(is_promotion(type));
    return (board::Piece)(promo_move_mask & (movetype_t)type);
}

//
// Encodes any move, legal or otherwise
//
struct Move : Wrapper<move_t, Move> {

  public:
    using Wrapper::Wrapper;
    constexpr Move(const Wrapper &val) : Wrapper(val) {};

    constexpr Move(board::Square from, board::Square to, MoveType type)
        : Move(((sixbit_mask & (move_t)from) << from_offset) |
               ((sixbit_mask & (move_t)to) << to_offset) |
               ((fourbit_mask & (move_t)type) << movetype_offset)) {};

    // Gets the origin square
    constexpr board::Square from() const {
        return (sixbit_mask & (value >> from_offset));
    }

    // Gets the destination square
    constexpr board::Square to() const {
        return (sixbit_mask & (value >> to_offset));
    }

    // Gets the move type
    constexpr MoveType type() const {
        return (MoveType)(fourbit_mask & (value >> movetype_offset));
    };

  private:
    //
    // Size checking
    //

    static constexpr int move_width = 16;    // Bits in a move
    static constexpr int square_width = 6;   // Bits needed to encode a square
    static constexpr int movetype_width = 4; // Remaining bits for MoveType

    // LSB of MoveType
    static constexpr int movetype_offset = move_width - movetype_width;

    // LSB of origin
    static constexpr int from_offset = movetype_offset - square_width;

    // LSB of destination
    static constexpr int to_offset = from_offset - square_width;

    // Check sizes are correct
    static_assert(to_offset == 0);

    // Static bitmask helpers
    static constexpr int sixbit_mask = 0b111111;
    static constexpr int fourbit_mask = 0b1111;

    // Check extra info fits in the space we have for it
    static_assert((movetype_t)max <= (1 << (movetype_width)));
};

} // namespace move

#endif

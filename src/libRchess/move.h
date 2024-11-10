#ifndef MOVE_H
#define MOVE_H

#include "board.h"
#include "magic.h"
#include <functional>
#include <optional>

using namespace board;

#define MOVE_WIDTH = 5

namespace move {

enum class MoveType {
    NORMAL = 0,
    CAPTURE,
    CASTLE_QUEENSIDE,
    CASTLE_KINGSIDE,
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

class Move {

    uint16_t move = 0;
    // Piece captured;

  private:
    static constexpr int move_width = 16;
    static constexpr int square_width = 6;
    static constexpr int extra_width = 4;
    static constexpr int from_offset = move_width - square_width;
    static constexpr int to_offset = from_offset - square_width;
    static constexpr int extra_offset = to_offset - extra_width;
    static_assert(extra_offset == 0);
    static constexpr int sixbit_mask = 0b111111;
    static constexpr int fourbit_mask = 0b1111;

    // Check extra info fits in the space we have for it
    static_assert((int)MoveType::MAX < (1 << (extra_width)));

    constexpr MoveType extra_info() const {
        return (MoveType)(move & fourbit_mask);
    };

  public:
    Move(Coord from, Coord to) {
        move = ((sixbit_mask & from.get_square()) << from_offset) |
               ((sixbit_mask & to.get_square()) << to_offset);
    }

    Coord from() const { return (sixbit_mask & (move >> from_offset)); }

    Coord to() const { return (sixbit_mask & (move >> to_offset)); }

    constexpr bool is_capture() const {
        switch (extra_info()) {
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

    constexpr bool is_en_passant() const {
        return extra_info() == MoveType::CAPTURE_EP;
    };

    constexpr bool is_promotion() const {
        switch (extra_info()) {
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

    constexpr bool is_castle() {
        return extra_info() == (MoveType::CASTLE_KINGSIDE) ||
               extra_info() == (MoveType::CASTLE_QUEENSIDE);
    }

    // Return pawn on no promotion
    constexpr std::optional<Piece> promoted_piece() const {
        switch (extra_info()) {
        case MoveType::PROMOTE_KNIGHT:
            return Piece::KNIGHT;
        case MoveType::PROMOTE_ROOK:
            return Piece::ROOK;
        case MoveType::PROMOTE_BISHOP:
            return Piece::BISHOP;
        case MoveType::PROMOTE_QUEEN:
            return Piece::QUEEN;
        case MoveType::PROMOTE_CAPTURE_KNIGHT:
            return Piece::KNIGHT;
        case MoveType::PROMOTE_CAPTURE_ROOK:
            return Piece::ROOK;
        case MoveType::PROMOTE_CAPTURE_BISHOP:
            return Piece::BISHOP;
        case MoveType::PROMOTE_CAPTURE_QUEEN:
            return Piece::QUEEN;
        default:
            return std::optional<Piece>();
        }
    }
};

} // namespace move

#endif

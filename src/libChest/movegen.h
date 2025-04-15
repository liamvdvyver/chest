#include "board.h"
#include "jumping.h"

namespace move::movegen {

// Generates movable squares given a starting square, and total board occupancy,
// for a particular movement pattern.
//
// Will generally be used to pre-compute moves, so doesn't have to be very fast.
// Fast methods may be used elsewhere by client classes.
//
// TODO: implement all
namespace detail {

// Get the attack set for a piece at a position, given occupancy (relevant
// blockers or total occupancy)
constexpr static board::Bitboard get_bishop_attacks(board::Square sq,
                                                    board::Bitboard blk) {

    board::Bitboard ret = 0;
    int f = sq.file();
    int r = sq.rank();
    board::Bitboard cur = board::Bitboard(sq);
    board::Bitboard next;

    // Assign down, left
    next = cur;
    for (int d = 1; board::Square::is_legal(f - d, r - d); d++) {
        next = next.shift(board::Direction::SW);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign down, right
    next = cur;
    for (int d = 1; board::Square::is_legal(f + d, r - d); d++) {
        next = next.shift(board::Direction::SE);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign up, left
    next = cur;
    for (int d = 1; board::Square::is_legal(f - d, r + d); d++) {
        next = next.shift(board::Direction::NW);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign up, right
    next = cur;
    for (int d = 1; board::Square::is_legal(f + d, r + d); d++) {
        next = next.shift(board::Direction::NE);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    return ret;
};

// Get the attack set for a piece at a position, given occupancy (relevant
// blockers or total occupancy)
constexpr static board::Bitboard get_rook_attacks(board::Square sq,
                                                  board::Bitboard blk) {

    board::Bitboard ret = 0;
    int f = sq.file();
    int r = sq.rank();
    board::Bitboard cur = board::Bitboard(sq);
    board::Bitboard next;

    int d_to_left = f;
    int d_to_right = board::board_size - f - 1;
    int d_to_bottom = r;
    int d_to_top = board::board_size - r - 1;

    // Attacks to left
    next = cur;
    for (int d = 1; d <= d_to_left; d++) {
        next = next.shift(board::Direction::W);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks to right
    next = cur;
    for (int d = 1; d <= d_to_right; d++) {
        next = next.shift(board::Direction::E);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks down
    next = cur;
    for (int d = 1; d <= d_to_bottom; d++) {
        next = next.shift(board::Direction::S);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks up
    next = cur;
    for (int d = 1; d <= d_to_top; d++) {
        next = next.shift(board::Direction::N);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    return ret;
};

// Jumping: returns full (correct) sets given multiple starting pieces
constexpr static board::Bitboard get_king_moves(board::Bitboard starting) {
    board::Bitboard ret = 0;
    ret |= starting.shift_no_wrap(board::Direction::N);
    ret |= starting.shift_no_wrap(board::Direction::S);
    ret |= starting.shift_no_wrap(board::Direction::E);
    ret |= starting.shift_no_wrap(board::Direction::W);
    return ret;
};

constexpr static board::Bitboard get_knight_moves(board::Bitboard starting) {
    board::Bitboard ret = 0;
    ret |= starting.shift_no_wrap(2, 1);
    ret |= starting.shift_no_wrap(2, -1);
    ret |= starting.shift_no_wrap(-2, 1);
    ret |= starting.shift_no_wrap(-2, -1);
    ret |= starting.shift_no_wrap(1, 2);
    ret |= starting.shift_no_wrap(1, -2);
    ret |= starting.shift_no_wrap(-1, 2);
    ret |= starting.shift_no_wrap(-1, -2);
    return ret;
};

constexpr static board::Direction get_forward_direction(board::Colour to_move) {
    return (bool)to_move ? board::Direction::N : board::Direction::S;
}

// 3 types of pawn moves
constexpr static board::Bitboard
get_pawn_single_pushes(board::Bitboard starting, board::Colour to_move) {

    const board::Bitboard back_rank = (bool)to_move
                                          ? board::Bitboard::rank_mask(7)
                                          : board::Bitboard::rank_mask(0);

    return (starting & back_rank).shift(get_forward_direction(to_move));
};

constexpr static board::Bitboard
get_pawn_double_pushes(board::Bitboard starting, board::Colour to_move) {

    const board::Bitboard starting_rank = (bool)to_move
                                              ? board::Bitboard::rank_mask(1)
                                              : board::Bitboard::rank_mask(6);
    return (starting & starting_rank)
        .shift(get_forward_direction(to_move))
        .shift(get_forward_direction(to_move));
};

constexpr static board::Bitboard get_pawn_all_pushes(board::Bitboard starting,
                                                     board::Colour to_move) {
    return get_pawn_single_pushes(starting, to_move) |
           get_pawn_double_pushes(starting, to_move);
};

constexpr static board::Bitboard get_pawn_attacks(board::Bitboard starting,
                                                  board::Colour to_move) {

    board::Bitboard single_push = get_pawn_single_pushes(starting, to_move);
    return single_push.shift_no_wrap(board::Direction::E) |
           single_push.shift_no_wrap(board::Direction::W);
};

} // namespace detail

} // namespace move::movegen

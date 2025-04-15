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

//
// Simple jumping moves (not colour dependent)
// TODO: I just want to see how abstract classes work in C++.
// Consider changing this if it seems hacky/sucky, which I predict it will.
// Maybe get rid of the free functions and just provide them in subclasses of a
// base Mover class.
// I suspect vtables shouldn't cause any issues here.
//
class PrecomputedMoveGenerator {
  public:
    // Give function pointer to get_moves method, compute maps.
    PrecomputedMoveGenerator() { init_attack_sets(); };

    board::Bitboard get_attack_set(board::Square starting) const {
        return m_attack_sets[starting];
    };

  private:
    // Provided at runtime
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) = 0;
    board::Bitboard m_attack_sets[board::n_squares];
    void init_attack_sets() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[sq] = gen_attack_set(board::Bitboard(sq));
        }
    };
};

class KingMoveGenerator : PrecomputedMoveGenerator {
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) {
        return detail::get_king_moves(starting);
    };
};

class KnightMoveGenerator : PrecomputedMoveGenerator {
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) {
        return detail::get_knight_moves(starting);
    };
};

//
// Pawns are colour dependant, and move in different ways.
// Setup a PrecomputedMover for each colour, for each direction.
//
// This does mean that the pre-computed moves are not stored together for each
// colour/movment type.
//
// TODO: test if the following is true:
// Since the move tables are (8 * 64) bytes (bigger than a cache line),
// and we can process one type of move at a time (using the same table as much
// as possible), the hit to cache locality shouldn't be a massive problem

template<board::Colour c>
class PawnPushGenerator : PrecomputedMoveGenerator {
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) {
        return detail::get_pawn_all_pushes(starting, c);
    }
};

template<board::Colour c>
class PawnAttackGenerator : PrecomputedMoveGenerator {
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) {
        return detail::get_pawn_attacks(starting, c);
    }
};

// Concrete precomputed pawn movers
PawnPushGenerator<board::Colour::WHITE> WhitePawnPusher;
PawnPushGenerator<board::Colour::BLACK> BlackPawnPusher;
PawnAttackGenerator<board::Colour::WHITE> WhitePawnAttacker;
PawnAttackGenerator<board::Colour::BLACK> BlackPawnAttacker;

} // namespace move::movegen

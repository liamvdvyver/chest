#include "board.h"

#include <iostream>
#include <random>

namespace move::movegen {

// Generates movable squares given a starting square, and total board occupancy,
// for a particular movement pattern.
//
// Will generally be used to pre-compute moves, so doesn't have to be very fast.
// Fast methods may be used elsewhere by client classes.
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

    return (starting & ~back_rank).shift(get_forward_direction(to_move));
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
    PrecomputedMoveGenerator() {};

    board::Bitboard get_attack_set(board::Square starting) const {
        return m_attack_sets[starting];
    };

  protected:
    board::Bitboard m_attack_sets[board::n_squares];

    virtual board::Bitboard gen_attack_set(board::Bitboard starting) = 0;
    void init_attack_sets() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[sq] = gen_attack_set(board::Bitboard(sq));
        }
    };
};

class KingMoveGenerator : public PrecomputedMoveGenerator {
  public:
    KingMoveGenerator() { init_attack_sets(); }

  private:
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) override {
        return detail::get_king_moves(starting);
    };
};

class KnightMoveGenerator : public PrecomputedMoveGenerator {
  public:
    KnightMoveGenerator() { init_attack_sets(); }

  private:
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) override {
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

template <board::Colour c>
class PawnPushGenerator : public PrecomputedMoveGenerator {
  public:
    PawnPushGenerator() { init_attack_sets(); }

  private:
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) override {
        return detail::get_pawn_all_pushes(starting, c);
    }
};

template <board::Colour c>
class PawnAttackGenerator : public PrecomputedMoveGenerator {
  public:
    PawnAttackGenerator() { init_attack_sets(); }

  private:
    virtual board::Bitboard gen_attack_set(board::Bitboard starting) override {
        return detail::get_pawn_attacks(starting, c);
    }
};

// Concrete precomputed pawn movers
class WhitePawnPusher : public PawnPushGenerator<board::Colour::WHITE> {};
class BlackPawnPusher : public PawnPushGenerator<board::Colour::BLACK> {
  public:
    using PawnPushGenerator::PawnPushGenerator;
};
class WhitePawnAttacker : public PawnAttackGenerator<board::Colour::WHITE> {};
class BlackPawnAttacker : public PawnAttackGenerator<board::Colour::BLACK> {};

//
// Sliding piece move generation: magic bitboards
//

// TODO: similar to the pawn case, test whether separating out
// bishop/rook has an effect from cache locality.
// Suspect vtables should have minimal effect, only initialisation is virtual.

// Magic numbers
typedef board::Bitboard magic_t;

// Attack table keys
typedef uint16_t magic_key_t;

// Parameterise key (bit) size for attacks to make array size concrete.
template <int max_shift> class MagicMoveGenerator {

  public:
    MagicMoveGenerator() : m_masks(), m_attacks(), m_magics(), m_shifts() {};

    void init() {
        init_masks();
        init_shifts();
        init_magics();
    }

    // Get all squares attacked by a piece.
    // May include own pieces.
    board::Bitboard get_attack_set(board::Square sq,
                                   board::Bitboard occ) const {
        return m_attacks[sq][get_magic_key(sq, occ)];
    };

  private:
    //
    // Piece-specific.
    //

    // Number of possible attack keys
    static const int n_attack_keys = 1 << (max_shift);

    // Generate blocker mask
    virtual board::Bitboard const gen_mask(board::Square sq) = 0;

    // Generate attacks given attacker square and (relevant or total) blocker
    // occupancy
    virtual board::Bitboard const gen_attacks(board::Square sq,
                                              board::Bitboard blk) = 0;

    //
    // Precomputed-data
    //

    // Relevant blocking mask (per-position)
    board::Bitboard m_masks[board::n_squares];

    // Attack maps (per-position, per-key)
    board::Bitboard m_attacks[board::n_squares][n_attack_keys];

    // Magic bitboards (per-position)
    magic_t m_magics[board::n_squares];

    // Key sizes
    int m_shifts[board::n_squares];

    //
    // Initialisation
    //

    // Populate attack maps for a position, given a candidate magic number.
    // Returns whether successful.
    bool init_attacks(magic_t magic, board::Square sq,
                      std::vector<magic_key_t> &visited) {
        visited.clear();

        board::Bitboard *coord_attack_map = m_attacks[sq];
        board::Bitboard all_blockers = m_masks[sq];

        magic_key_t key = 0;

        for (board::Bitboard blocker_subset : all_blockers.subsets()) {

            board::Bitboard attacked = gen_attacks(sq, blocker_subset);
            key = get_magic_key(sq, blocker_subset, magic);

            board::Bitboard &cur_elm = coord_attack_map[key];

            // Collision with different val
            if (!cur_elm.empty() && cur_elm != attacked) {

                for (const magic_key_t visited_key : visited) {
                    coord_attack_map[visited_key] = 0;
                }

                return false;
            }

            visited.push_back(key);
            cur_elm = attacked;
        }

        // Success
        std::cout << "Found magic: " << (board::bitboard_t)magic
                  << " for piece: " << "BRUIHHH TODO: WTf"
                  << " at square: " << std::to_string(sq) << std::endl;

        return true;
    };

    // Populate all blocker masks
    void init_masks() {
        int max_shift_found = 0;
        for (board::Square sq : board::Square::AllSquareIterator()) {
            board::Bitboard mask = gen_mask(sq);
            m_masks[sq] = mask;

            if (mask.size() > max_shift_found)
                max_shift_found = mask.size();
        }
        assert(max_shift_found == max_shift);
    };

    // Populate all magic numbers
    void init_magics() {

        magic_t magic_num;
        bool done;

        // IDK WTF THIS DOES
        std::random_device rd;
        std::mt19937_64 eng(rd());

        std::uniform_int_distribution<board::bitboard_t> rand;

        std::vector<magic_key_t> visited;

        for (board::Square sq : board::Square::AllSquareIterator()) {
            done = false;
            do {
                magic_num = rand(eng) & rand(eng) & rand(eng);
                done = init_attacks(magic_num, sq, visited);
            } while (!done);
            m_magics[sq] = magic_num;
        }
    };

    // Populate all attack map key sizes
    // TODO: test if faster to generate on the fly.
    void init_shifts() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_shifts[sq] = m_masks[sq].size();
        }
    };

    //
    // Lookup
    //

    // Get the attack map key, given position, blockers (relevant or total)
    magic_key_t get_magic_key(board::Square sq, board::Bitboard occ) const {
        return get_magic_key(sq, occ, m_magics[sq]);
    };

    // Helper
    int shift_width(board::Square sq) const {
        return board::n_squares - m_shifts[sq];
    }

    // Get the attack map key, given position, blockers (relevant or total), and
    // a magic number
    magic_key_t get_magic_key(board::Square sq, board::Bitboard occ,
                              magic_t magic) const {
        board::Bitboard mask = m_masks[sq];
        return (board::bitboard_t)((occ & mask) * magic) >> shift_width(sq);
    };
};

static const int max_rook_shift = 12;
class RookMoveGenerator : public MagicMoveGenerator<max_rook_shift> {
  public:
    RookMoveGenerator() : MagicMoveGenerator() { init(); }

  private:
    virtual board::Bitboard const gen_mask(board::Square sq) override {
        board::Bitboard ret = 0;

        int f = sq.file();
        int r = sq.rank();

        // Assign along rank
        for (int f_cur = 1; f_cur < board::board_size - 1; f_cur++) {

            // Skip on same file
            if (f_cur == f)
                continue;

            ret |= board::Bitboard(board::Square(f_cur, r));
        }

        // Assign along file
        for (int r_cur = 1; r_cur < board::board_size - 1; r_cur++) {

            // Skip on same rank
            if (r_cur == r)
                continue;

            ret |= board::Bitboard(board::Square(f, r_cur));
        }

        return ret;
    };

    virtual board::Bitboard const gen_attacks(board::Square sq,
                                              board::Bitboard blk) override {
        return detail::get_rook_attacks(sq, blk);
    };
};

static const int max_bishop_shift = 9;
class BishopMoveGenerator : public MagicMoveGenerator<max_bishop_shift> {
  public:
    BishopMoveGenerator() : MagicMoveGenerator() { init(); }

  private:
    virtual board::Bitboard const gen_mask(board::Square sq) override {
        board::Bitboard ret = 0;

        int f = sq.file();
        int r = sq.rank();

        int n_to_bot_left = std::min(f - 1, r - 1);
        int n_to_bot_right = std::min(board::board_size - f - 2, r - 1);
        int n_to_top_left = std::min(f - 1, board::board_size - r - 2);
        int n_to_top_right =
            std::min(board::board_size - f - 2, board::board_size - r - 2);

        // Assign down, left
        board::Bitboard b = board::Bitboard(sq);
        for (int i = 0; i < n_to_bot_left; i++) {
            b = b.shift(-1, -1);
            ret |= b;
        }

        // Assign down, right
        b = board::Bitboard(sq);
        for (int i = 0; i < n_to_bot_right; i++) {
            b = b.shift(1, -1);
            ret |= b;
        }

        // Assign up, left
        b = board::Bitboard(sq);
        for (int i = 0; i < n_to_top_left; i++) {
            b = b.shift(-1, 1);
            ret |= b;
        }

        // Assign up, right
        b = board::Bitboard(sq);
        for (int i = 0; i < n_to_top_right; i++) {
            b = b.shift(1, 1);
            ret |= b;
        }

        return ret;
    };

    virtual board::Bitboard const gen_attacks(board::Square sq,
                                              board::Bitboard blk) override {
        return detail::get_bishop_attacks(sq, blk);
    };
};


} // namespace move::movegen

#ifndef ATTACK_H
#define ATTACK_H

#include "board.h"

#include <iostream>
#include <random>

//
// Functions to find squares targeted by pieces, given position, piece type,
// move type.
//

namespace move::attack {

// We will use these functions to construct pre-computed move generators,
// so implement them as functors and define some concepts here:

template <typename T>
concept Attacker = requires(T t, board::Square from) {
    { t(from) } -> std::same_as<board::Bitboard>;
};

template <typename T>
concept SlidingAttacker =
    requires(T t, board::Square from, board::Bitboard blk) {
        { t(from, blk) } -> std::same_as<board::Bitboard>;
    };

template <typename T>
concept MultiAttacker = requires(T t, board::Bitboard attackers) {
    { t(attackers) } -> std::same_as<board::Bitboard>;
};

template <typename T>
concept ColouredAttacker =
    requires(T t, board::Square from, board::Colour colour) {
        { t(from, colour) } -> std::same_as<board::Bitboard>;
    };

template <typename T>
concept ColouredMultiAttacker =
    requires(T t, board::Bitboard attackers, board::Colour colour) {
        { t(attackers, colour) } -> std::same_as<board::Bitboard>;
    };

// Generates movable squares given a starting square, and total board occupancy,
// for a particular movement pattern.
//
// Will generally be used to pre-compute moves, so doesn't have to be very fast.
// Fast methods may be used elsewhere by client classes.
namespace detail {

// Get the attack set for a piece at a position, given occupancy (relevant
// blockers or total occupancy)
//
// TODO: make operators static (C++23)
//
struct GenBishopAttacks {
    constexpr static board::Bitboard operator()(board::Square sq,
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
};
static_assert(SlidingAttacker<GenBishopAttacks>);

struct GenRookAttacks {
    constexpr board::Bitboard static operator()(board::Square sq,
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
    }
};
static_assert(SlidingAttacker<GenRookAttacks>);

//
// Jumping: returns full (correct) sets given multiple starting pieces
//

// Generate movable squares given king occupancy
struct GenKingAttacks {
    constexpr static board::Bitboard operator()(board::Bitboard starting) {
        board::Bitboard ret = 0;
        ret |= starting.shift_no_wrap(board::Direction::N);
        ret |= starting.shift_no_wrap(board::Direction::S);
        ret |= starting.shift_no_wrap(board::Direction::E);
        ret |= starting.shift_no_wrap(board::Direction::W);
        return ret;
    }
};
static_assert(MultiAttacker<GenKingAttacks>);

// Generate movable squares given knight occupancy
struct GenKnightAttacks {
    constexpr static board::Bitboard operator()(board::Bitboard starting) {
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
    }
};
static_assert(MultiAttacker<GenKnightAttacks>);

//
// Pawn moves (several types, different per colour)
//

// Helper: direction pawns move in given colour
constexpr board::Direction forward_direction(board::Colour to_move) {
    return (bool)to_move ? board::Direction::N : board::Direction::S;
}

struct GenPawnSinglePushes {
    constexpr static board::Bitboard operator()(board::Bitboard starting,
                                                board::Colour to_move) {

        const board::Bitboard back_rank = (bool)to_move
                                              ? board::Bitboard::rank_mask(7)
                                              : board::Bitboard::rank_mask(0);

        return (starting & ~back_rank).shift(forward_direction(to_move));
    }
};
static_assert(ColouredMultiAttacker<GenPawnSinglePushes>);

struct GenPawnDoublePushes {
    constexpr static board::Bitboard operator()(board::Bitboard starting,
                                                board::Colour to_move) {

        const board::Bitboard starting_rank =
            (bool)to_move ? board::Bitboard::rank_mask(1)
                          : board::Bitboard::rank_mask(6);
        return (starting & starting_rank)
            .shift(forward_direction(to_move))
            .shift(forward_direction(to_move));
    }
};
static_assert(ColouredMultiAttacker<GenPawnDoublePushes>);

struct GenAllPawnPushes {
    constexpr static board::Bitboard operator()(board::Bitboard starting,
                                                board::Colour to_move) {
        return GenPawnSinglePushes()(starting, to_move) |
               GenPawnDoublePushes()(starting, to_move);
    }
};
static_assert(ColouredMultiAttacker<GenAllPawnPushes>);
struct GenPawnAttacks {
    constexpr board::Bitboard operator()(board::Bitboard starting,
                                         board::Colour to_move) {

        board::Bitboard single_push = GenPawnSinglePushes()(starting, to_move);
        return single_push.shift_no_wrap(board::Direction::E) |
               single_push.shift_no_wrap(board::Direction::W);
    }
};
static_assert(ColouredMultiAttacker<GenPawnAttacks>);

//
// Functors to generate blocker masks for sliding pieces
//

struct GenRookMask {
    constexpr static board::Bitboard operator()(board::Square sq) {
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
    }
};
static_assert(Attacker<GenRookMask>);

struct GenBishopMask {
    constexpr static board::Bitboard operator()(board::Square sq) {
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
    }
};
static_assert(Attacker<GenBishopMask>);

} // namespace detail

//
// Simple attack generators
//

// For type checking, use the following default concept specialisers:
// * Attacker -> {}
// * SlidingAttacker -> GenBishopAttacks
// * MultiAttacker -> GenKnightAttacks
// * ColouredMultiAttacker -> GenPawnSinglePushes

template <MultiAttacker T> class PrecomputedMultiAttacker {
  public:
    constexpr board::Bitboard operator()(board::Square starting) const {
        return m_attack_sets[starting];
    };
    PrecomputedMultiAttacker() { init_attack_sets(); };

  private:
    board::Bitboard m_attack_sets[board::n_squares];
    T m_attack_generator;
    void init_attack_sets() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[sq] = m_attack_generator(board::Bitboard(sq));
        }
    };
};
static_assert(Attacker<PrecomputedMultiAttacker<detail::GenKnightAttacks>>);

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

template <ColouredMultiAttacker T> class PrecomputedColouredMultiAttacker {
  public:
    constexpr board::Bitboard operator()(board::Square starting,
                                         board::Colour colour) const {
        return m_attack_sets[(int)colour][starting];
    };
    PrecomputedColouredMultiAttacker() { init_attack_sets(); };

  private:
    board::Bitboard m_attack_sets[board::n_colours][board::n_squares];
    T m_attack_generator;
    void init_attack_sets() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[(int)board::Colour::BLACK][sq] =
                m_attack_generator(board::Bitboard(sq), board::Colour::BLACK);
            m_attack_sets[(int)board::Colour::WHITE][sq] =
                m_attack_generator(board::Bitboard(sq), board::Colour::WHITE);
        }
    }
};
static_assert(ColouredAttacker<
              PrecomputedColouredMultiAttacker<detail::GenPawnSinglePushes>>);

// TODO: similar to the pawn case, test whether separating out
// bishop/rook has an effect from cache locality.
// Suspect vtables should have minimal effect, only initialisation is virtual.

// Magic numbers
typedef board::Bitboard magic_t;

// Attack table keys
typedef uint16_t magic_key_t;

// Parameterise key (bit) size for attacks to make array size concrete.
template <int max_shift, SlidingAttacker TAttacker, Attacker TMasker>
class MagicAttacker {

  public:
    MagicAttacker() : m_masks(), m_attacks(), m_magics(), m_shifts() {
        init();
    };

    void init() {
        init_masks();
        init_shifts();
        init_magics();
    }

    // Get all squares attacked by a piece.
    // May include own pieces.
    board::Bitboard operator()(board::Square sq, board::Bitboard occ) const {
        return m_attacks[sq][get_magic_key(sq, occ)];
    };

  private:
    //
    // Piece-specific.
    //

    // Number of possible attack keys
    static const int n_attack_keys = 1 << (max_shift);

    // Generate blocker mask
    TMasker m_masker;
    board::Bitboard const gen_mask(board::Square sq) { return m_masker(sq); };

    // Generate attacks given attacker square and (relevant or total) blocker
    // occupancy
    TAttacker m_attacker;
    board::Bitboard const gen_attacks(board::Square sq, board::Bitboard blk) {
        return m_attacker(sq, blk);
    };

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

//
// Create concrete instances
//

typedef PrecomputedMultiAttacker<detail::GenKingAttacks> kingAttacker;
typedef PrecomputedMultiAttacker<detail::GenKnightAttacks> knightAttacker;

typedef PrecomputedColouredMultiAttacker<detail::GenPawnSinglePushes>
    pawnSinglePusher;
typedef PrecomputedColouredMultiAttacker<detail::GenPawnDoublePushes>
    pawnDoublePusher;
typedef PrecomputedColouredMultiAttacker<detail::GenAllPawnPushes> pawnPusher;
typedef PrecomputedColouredMultiAttacker<detail::GenPawnAttacks> pawnAttacker;

typedef MagicAttacker<9, detail::GenBishopAttacks, detail::GenBishopMask>
    bishopAttacker;
typedef MagicAttacker<12, detail::GenRookAttacks, detail::GenRookMask>
    rookAttacker;
static_assert(SlidingAttacker<bishopAttacker>);
static_assert(SlidingAttacker<rookAttacker>);

} // namespace move::attack

#endif

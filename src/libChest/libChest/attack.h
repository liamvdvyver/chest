#ifndef ATTACK_H
#define ATTACK_H

#include "board.h"
#if __has_include(<immintrin.h>)
#include <immintrin.h>
#endif
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
        ret |= starting.shift_no_wrap(board::Direction::NE);
        ret |= starting.shift_no_wrap(board::Direction::SE);
        ret |= starting.shift_no_wrap(board::Direction::NW);
        ret |= starting.shift_no_wrap(board::Direction::SW);
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

// Given a MultiAttacker, create an attacker which stores precomputed moves.
template <MultiAttacker T> class PrecomputedMultiAttacker {
  public:
    constexpr board::Bitboard operator()(board::Square starting) const {
        return m_attack_sets[starting];
    };
    constexpr PrecomputedMultiAttacker() { init_attack_sets(); };

  private:
    board::Bitboard m_attack_sets[board::n_squares];
    T m_attack_generator;
    constexpr void init_attack_sets() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[sq] = m_attack_generator(board::Bitboard(sq));
        }
    };
};
static_assert(Attacker<PrecomputedMultiAttacker<detail::GenKnightAttacks>>);

// Precompute moves, allow lookup by square and colour.
template <ColouredMultiAttacker T> class PrecomputedColouredMultiAttacker {
  public:
    constexpr board::Bitboard operator()(board::Square starting,
                                         board::Colour colour) const {
        return m_attack_sets[(int)colour][starting];
    };
    constexpr PrecomputedColouredMultiAttacker() { init_attack_sets(); };

  private:
    board::Bitboard m_attack_sets[board::n_colours][board::n_squares];
    T m_attack_generator;
    constexpr void init_attack_sets() {
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

// Magic numbers
typedef board::Bitboard magic_t;

// Attack table keys
typedef uint16_t magic_key_t;

// Given attack and mask generators, implement plain magic bitboards.
// Uses PEXT bitboards instead if available.
// Parameterise key (bit) size for attacks to make array size concrete.
// TODO: write two types (pext/magic) and typedef w/ directive
template <int max_shift, SlidingAttacker TAttacker, Attacker TMasker>
class MagicAttacker {

  public:
    constexpr MagicAttacker()
        : m_masks(), m_attacks(),
#if !__has_include(<immintrin.h>)
          m_magics(),
#endif
          m_shifts() {
        init();
    };

    constexpr void init() {
        init_masks();
        init_shifts();
        init_magics();
    }

    // Get all squares attacked by a piece.
    // May include own pieces.
    constexpr board::Bitboard operator()(board::Square sq,
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
    TMasker m_masker;
    constexpr board::Bitboard const gen_mask(board::Square sq) {
        return m_masker(sq);
    };

    // Generate attacks given attacker square and (relevant or total) blocker
    // occupancy
    TAttacker m_attacker;
    constexpr board::Bitboard const gen_attacks(board::Square sq,
                                                board::Bitboard blk) {
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
#if !__has_include(<immintrin.h>)
    magic_t m_magics[board::n_squares];
#endif

    // Key sizes
    int m_shifts[board::n_squares];

    //
    // Initialisation
    //

    // Populate attack maps for a position, given a candidate magic number.
    // Returns whether successful.
    constexpr bool init_attacks(magic_t magic, board::Square sq,
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

        return true;
    };

    // Populate all blocker masks
    constexpr void init_masks() {
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
    constexpr void init_magics() {

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
#if !__has_include(<immintrin.h>)
            m_magics[sq] = magic_num;
#endif
        }
    };

    // Populate all attack map key sizes
    constexpr void init_shifts() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            m_shifts[sq] = m_masks[sq].size();
        }
    };

    //
    // Lookup
    //

    // Get the attack map key, given position, blockers (relevant or total)
    constexpr magic_key_t get_magic_key(board::Square sq,
                                        board::Bitboard occ) const {
#if __has_include(<immintrin.h>)
        board::Bitboard mask = m_masks[sq];
        return _pext_u64((board::bitboard_t)occ, (board::bitboard_t)mask);
#else
        return get_magic_key(sq, occ, m_magics[sq]);
#endif
    };

    // Helper
    // Plain magic bitboard -> constant shift
    constexpr int shift_width(board::Square sq) const {
        (void)sq;
        return board::n_squares - max_shift;
    }

    // Get the attack map key, given position, blockers (relevant or total), and
    // a magic number
    constexpr magic_key_t get_magic_key(board::Square sq, board::Bitboard occ,
                                        magic_t magic) const {
        board::Bitboard mask = m_masks[sq];
#if __has_include(<immintrin.h>)
        (void)magic;
        return _pext_u64((board::bitboard_t)occ, (board::bitboard_t)mask);
#else
        return (board::bitboard_t)((occ & mask) * magic) >> shift_width(sq);
#endif
    };
};

//
// Create concrete instances
//

typedef PrecomputedMultiAttacker<detail::GenKingAttacks> KingAttacker;
typedef PrecomputedMultiAttacker<detail::GenKnightAttacks> KnightAttacker;

typedef PrecomputedColouredMultiAttacker<detail::GenPawnSinglePushes>
    PawnSinglePusher;
typedef PrecomputedColouredMultiAttacker<detail::GenPawnDoublePushes>
    PawnDoublePusher;
typedef PrecomputedColouredMultiAttacker<detail::GenAllPawnPushes> PawnPusher;
typedef PrecomputedColouredMultiAttacker<detail::GenPawnAttacks> PawnAttacker;

typedef MagicAttacker<9, detail::GenBishopAttacks, detail::GenBishopMask>
    BishopAttacker;
typedef MagicAttacker<12, detail::GenRookAttacks, detail::GenRookMask>
    RookAttacker;
static_assert(SlidingAttacker<BishopAttacker>);
static_assert(SlidingAttacker<RookAttacker>);

} // namespace move::attack

#endif

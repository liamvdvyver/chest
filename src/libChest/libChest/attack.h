//============================================================================//
// Attack generation:
// Find squares targeted by pieces, given position, piece type, move type.
//============================================================================//

#pragma once

#include "board.h"

// If x86 pext instruction is present,
// it is used for generating precomputed attack keys.
#if defined(__BMI2__)
#define USE_PEXT
#include <immintrin.h>
#endif

#include <random>

namespace move::attack {

//============================================================================//
// Attackers:
// Functors satisfying these concepts provide implementations of attack
// generation.
//============================================================================//

// Generate attacks for a single (jumping) piece given origin.
template <typename T>
concept Attacker = requires(T t, const board::Square from) {
    { t(from) } -> std::same_as<board::Bitboard>;
};

// Generate attacks for a single (sliding) piece given origin and blocker set.
template <typename T>
concept SlidingAttacker =
    requires(T t, const board::Square from, const board::Bitboard blk) {
        { t(from, blk) } -> std::same_as<board::Bitboard>;
    };

// Generate attacks for a multiple (jumping) pieces given origin set.
template <typename T>
concept MultiAttacker = requires(T t, const board::Bitboard attackers) {
    { t(attackers) } -> std::same_as<board::Bitboard>;
};

// Generate attacks for a single (jumping) piece given origin and colour
// (i.e. pawns).
template <typename T>
concept ColouredAttacker =
    requires(T t, board::Square const from, const board::Colour colour) {
        { t(from, colour) } -> std::same_as<board::Bitboard>;
    };

// Generate attacks for a multiple (jumping) piece given origin.
// In practice, used for pawn generation.
template <typename T>
concept ColouredMultiAttacker =
    requires(T t, const board::Bitboard attackers, const board::Colour colour) {
        { t(attackers, colour) } -> std::same_as<board::Bitboard>;
    };

//============================================================================//
// Basic piece attackers:
// Will generally be used to pre-compute moves, so doesn't have to be very fast.
// Fast methods may be used elsewhere by client classes.
//============================================================================//

namespace detail {

//----------------------------------------------------------------------------//
// Sliding pieces: scanning (slow) SlidingAttackers.
//----------------------------------------------------------------------------//

struct GenBishopAttacks {
    constexpr static board::Bitboard operator()(const board::Square sq,
                                                const board::Bitboard blk) {
        board::Bitboard ret = 0;
        const auto [f, r] = sq.coords();
        board::Bitboard next;

        // Assign down, left
        next = sq;
        for (board::coord_t d = 1; board::Square::is_legal(f - d, r - d); d++) {
            next = next.shift(board::Direction::SW);
            ret |= next;
            if (blk & next) {
                break;
            }
        }

        // Assign down, right
        next = sq;
        for (board::coord_t d = 1; board::Square::is_legal(f + d, r - d); d++) {
            next = next.shift(board::Direction::SE);
            ret |= next;
            if (blk & next) {
                break;
            }
        }

        // Assign up, left
        next = sq;
        for (board::coord_t d = 1; board::Square::is_legal(f - d, r + d); d++) {
            next = next.shift(board::Direction::NW);
            ret |= next;
            if (blk & next) {
                break;
            }
        }

        // Assign up, right
        next = sq;
        for (board::coord_t d = 1; board::Square::is_legal(f + d, r + d); d++) {
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
    constexpr board::Bitboard static operator()(const board::Square sq,
                                                const board::Bitboard blk) {
        board::Bitboard ret = 0;
        const auto [f, r] = sq.coords();
        board::Bitboard next;

        const board::coord_t d_to_left = f;
        const board::coord_t d_to_right = board::board_size - f - 1;
        const board::coord_t d_to_bottom = r;
        const board::coord_t d_to_top = board::board_size - r - 1;

        // Attacks to left
        next = sq;
        for (board::coord_t d = 1; d <= d_to_left; d++) {
            next = next.shift(board::Direction::W);
            ret |= next;
            if (blk & next) {
                break;
            }
        }

        // Attacks to right
        next = sq;
        for (board::coord_t d = 1; d <= d_to_right; d++) {
            next = next.shift(board::Direction::E);
            ret |= next;
            if (blk & next) {
                break;
            }
        }

        // Attacks down
        next = sq;
        for (board::coord_t d = 1; d <= d_to_bottom; d++) {
            next = next.shift(board::Direction::S);
            ret |= next;
            if (blk & next) {
                break;
            }
        }

        // Attacks up
        next = sq;
        for (board::coord_t d = 1; d <= d_to_top; d++) {
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

//----------------------------------------------------------------------------//
// Jumping: MultiAttackers shift origin set.
//----------------------------------------------------------------------------//

// Generate movable squares given king occupancy
struct GenKingAttacks {
    constexpr static board::Bitboard operator()(
        const board::Bitboard starting) {
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
    constexpr static board::Bitboard operator()(
        const board::Bitboard starting) {
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

//----------------------------------------------------------------------------//
// Pawn moves: ColouredMultiAttackers for each mode of movement.
//----------------------------------------------------------------------------//

// Helper: direction pawns move in given colour
constexpr board::Direction forward_direction(const board::Colour to_move) {
    return static_cast<bool>(to_move) ? board::Direction::N
                                      : board::Direction::S;
}

struct GenPawnSinglePushes {
    constexpr static board::Bitboard operator()(const board::Bitboard starting,
                                                const board::Colour to_move) {
        const board::Bitboard back_rank =
            static_cast<bool>(to_move)
                ? board::Bitboard::rank_mask(board::board_size - 1)
                : board::Bitboard::rank_mask(0);

        return (starting & ~back_rank).shift(forward_direction(to_move));
    }
};
static_assert(ColouredMultiAttacker<GenPawnSinglePushes>);

struct GenPawnDoublePushes {
    constexpr static board::Bitboard operator()(const board::Bitboard starting,
                                                const board::Colour to_move) {
        const board::Bitboard starting_rank =
            static_cast<bool>(to_move)
                ? board::Bitboard::rank_mask(1)
                : board::Bitboard::rank_mask(board::board_size - 2);
        return (starting & starting_rank)
            .shift(forward_direction(to_move))
            .shift(forward_direction(to_move));
    }
};
static_assert(ColouredMultiAttacker<GenPawnDoublePushes>);

struct GenPawnCaptures {
    constexpr static board::Bitboard operator()(const board::Bitboard starting,
                                                const board::Colour to_move) {
        const board::Bitboard single_push =
            GenPawnSinglePushes()(starting, to_move);
        return single_push.shift_no_wrap(board::Direction::E) |
               single_push.shift_no_wrap(board::Direction::W);
    }
};
static_assert(ColouredMultiAttacker<GenPawnCaptures>);

//----------------------------------------------------------------------------//
// Sliding piece blocker masks:
// All squares which may block a sliding pieces' movement,
// i.e. all attacked squares except the final positions on the rim.
//----------------------------------------------------------------------------//

struct GenRookMask {
    constexpr static board::Bitboard operator()(const board::Square sq) {
        board::Bitboard rim_final_targets = 0;

        constexpr std::array<board::Bitboard, 4> rim_sides = {
            board::Bitboard::rank_mask(0),
            board::Bitboard::file_mask(0),
            board::Bitboard::rank_mask(board::board_size - 1),
            board::Bitboard::file_mask(board::board_size - 1),
        };
        for (const board::Bitboard side : rim_sides) {
            if (!(board::Bitboard{sq} & side)) {
                rim_final_targets |= side;
            }
        }
        return GenRookAttacks()(sq, 0).setdiff(rim_final_targets);
    }
};
static_assert(Attacker<GenRookMask>);

struct GenBishopMask {
    constexpr static board::Bitboard operator()(const board::Square sq) {
        constexpr board::Bitboard rim =
            board::Bitboard::rank_mask(0) | board::Bitboard::file_mask(0) |
            board::Bitboard::rank_mask(board::board_size - 1) |
            board::Bitboard::file_mask(board::board_size - 1);

        return GenBishopAttacks()(sq, 0).setdiff(rim);
    }
};
static_assert(Attacker<GenBishopMask>);

}  // namespace detail

//============================================================================//
// Templated precomputed attack generators.
//============================================================================//

//----------------------------------------------------------------------------//
// Simple precomputed attack generators: per-piece lookup.
//----------------------------------------------------------------------------//

// Precomputed per-square.
template <MultiAttacker T>
class PrecomputedMultiAttacker {
   public:
    constexpr board::Bitboard operator()(const board::Square starting) const {
        return m_attack_sets[starting];
    };
    constexpr PrecomputedMultiAttacker() { init_attack_sets(); };

   private:
    std::array<board::Bitboard, board::n_squares> m_attack_sets{};
    T m_attack_generator;
    constexpr void init_attack_sets() {
        for (const board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[sq] = m_attack_generator(board::Bitboard(sq));
        }
    };
};
static_assert(Attacker<PrecomputedMultiAttacker<detail::GenKnightAttacks>>);

// Precomputed per-colour-per-square.
template <ColouredMultiAttacker T>
class PrecomputedColouredMultiAttacker {
   public:
    constexpr board::Bitboard operator()(const board::Square starting,
                                         const board::Colour colour) const {
        return m_attack_sets[static_cast<size_t>(colour)][starting];
    };
    constexpr PrecomputedColouredMultiAttacker() { init_attack_sets(); };

   private:
    std::array<std::array<board::Bitboard, board::n_squares>, board::n_colours>
        m_attack_sets{};
    T m_attack_generator;
    constexpr void init_attack_sets() {
        for (const board::Square sq : board::Square::AllSquareIterator()) {
            m_attack_sets[static_cast<size_t>(board::Colour::BLACK)][sq] =
                m_attack_generator(board::Bitboard(sq), board::Colour::BLACK);
            m_attack_sets[static_cast<size_t>(board::Colour::WHITE)][sq] =
                m_attack_generator(board::Bitboard(sq), board::Colour::WHITE);
        }
    }
};
static_assert(ColouredAttacker<
              PrecomputedColouredMultiAttacker<detail::GenPawnSinglePushes>>);

//----------------------------------------------------------------------------//
// Blocker-keyed precomputed (sliding) attack generators:
// Lookup attacks per-square-per-blocker-mask-subset, using:
// * PEXT bitboards if x86 BMI2 is available,
// * Plain (fixed-shift) magics otherwise.
//----------------------------------------------------------------------------//

// Plain (fixed-shift) magic bitboard attacker.
template <SlidingAttacker TAttacker, Attacker TMasker>
class PlainMagicAttacker {
   public:
    constexpr PlainMagicAttacker() {
        init_masks();
        init_all_attacks();
    }

    constexpr board::Bitboard operator()(const board::Square sq,
                                         const board::Bitboard occ) const {
        return m_attacks[sq][magic_key(sq, occ, m_magics[sq])];
    }

   private:
    // Magic numbers
    using magic_t = board::Bitboard;
    // Attack table keys
    using magic_key_t = size_t;

    // Populate attack maps for a position, given a candidate magic number.
    // Returns whether successful, i.e., no collision occurred.
    constexpr bool init_position_attacks(const magic_t magic,
                                         const board::Square sq,
                                         std::vector<magic_key_t> &visited) {
        visited.clear();

        for (board::Bitboard blocker_subset : m_masks[sq].subsets()) {
            board::Bitboard attacked = m_attacker(sq, blocker_subset);
            const magic_key_t key = magic_key(sq, blocker_subset, magic);

            board::Bitboard &cur_elm = m_attacks[sq][key];

            // Collision with different val
            if (!cur_elm.empty() && cur_elm != attacked) {
                for (const magic_key_t visited_key : visited) {
                    m_attacks[sq][visited_key] = 0;
                }

                return false;
            }

            visited.push_back(key);
            cur_elm = attacked;
        }

        return true;
    }

    // Populate all blocker masks
    constexpr void init_masks() {
        size_t max_shift_found = 0;
        for (board::Square sq : board::Square::AllSquareIterator()) {
            board::Bitboard mask = m_masker(sq);
            m_masks[sq] = mask;

            if (mask.size() > max_shift_found) max_shift_found = mask.size();
        }
        assert(max_shift_found == max_mask_sz());
    }

    // Populate all attacks, and keys if using magic bitboards.
    constexpr void init_all_attacks() {
        std::random_device rd;
        std::mt19937_64 eng(rd());
        std::uniform_int_distribution<board::bitboard_t> rand;

        std::vector<magic_key_t> visited;

        for (board::Square sq : board::Square::AllSquareIterator()) {
            magic_t magic_num;
            for (bool done = false; !done;) {
                magic_num = rand(eng) & rand(eng) & rand(eng);
                done = init_position_attacks(magic_num, sq, visited);
            };
            m_magics[sq] = magic_num;
        }
    }

    // WARN: Requires fast initilialisation of m_masker!
    consteval static size_t max_mask_sz() {
        size_t max = 0;
        for (const board::Square sq : board::Square::AllSquareIterator()) {
            const size_t cur_sz = TMasker()(sq).size();
            max = cur_sz > max ? cur_sz : max;
        }
        return max;
    }

    constexpr size_t shift_width(const board::Square sq) const {
        (void)sq;
        return board::n_squares - max_mask_sz();
    }

    constexpr magic_key_t magic_key(const board::Square sq, board::Bitboard occ,
                                    const magic_t magic) const {
        board::Bitboard mask = m_masks[sq];
        return (board::bitboard_t)((occ & mask) * magic) >> shift_width(sq);
    }

    TAttacker m_attacker;
    TMasker m_masker;

    // Relevant blocker mask (per-position)
    std::array<board::Bitboard, board::n_squares> m_masks{};

    // Attack maps (per-position, per-key)
    std::array<std::array<board::Bitboard, 1 << max_mask_sz()>,
               board::n_squares>
        m_attacks;

    // Magic bitboards (per-position)
    std::array<magic_t, board::n_squares> m_magics;
};

#ifdef USE_PEXT
// PEXT (variable shift) attacker.
template <SlidingAttacker TAttacker, Attacker TMasker>
class PextAttacker {
   public:
    constexpr PextAttacker() {
        size_t cur_sq_base_idx = 0;
        for (board::Square sq : board::Square::AllSquareIterator()) {
            board::Bitboard cur_sq_blocker_mask = m_masker(sq);

            m_masks[sq] = cur_sq_blocker_mask;
            m_bases[sq] = cur_sq_base_idx;

            for (board::Bitboard blocker_subset :
                 cur_sq_blocker_mask.subsets()) {
                board::Bitboard &cur_attacks =
                    m_attacks[cur_sq_base_idx +
                              attack_key(blocker_subset, cur_sq_blocker_mask)];
                assert(!cur_attacks);
                cur_attacks = m_attacker(sq, blocker_subset);
            }

            cur_sq_base_idx += (2 << cur_sq_blocker_mask.size());
        }
        assert(cur_sq_base_idx == map_sz());
    };

    constexpr board::Bitboard operator()(const board::Square sq,
                                         const board::Bitboard occ) const {
        return m_attacks[m_bases[sq] + attack_key(occ, m_masks[sq])];
    }

   private:
    constexpr size_t attack_key(const board::Bitboard occ,
                                const board::Bitboard mask) const {
        return _pext_u64((board::bitboard_t)occ, (board::bitboard_t)mask);
    }

    // WARN: Requires fast initilialisation of m_masker!
    consteval static size_t map_sz() {
        size_t ret = 0;
        for (board::Square sq : board::Square::AllSquareIterator()) {
            ret += 2 << TMasker()(sq).size();
        }
        return ret;
    }

    TAttacker m_attacker{};
    TMasker m_masker{};

    // Relevant blocker mask (per-position)
    std::array<board::Bitboard, board::n_squares> m_masks{};

    // Attack masks: per position, per blocker subset
    std::array<board::Bitboard, map_sz()> m_attacks{};

    // Attack map base addresses per square
    std::array<size_t, board::n_squares> m_bases{};
};
#endif

//============================================================================//
// Concrete instances.
//============================================================================//

using KingAttacker = PrecomputedMultiAttacker<detail::GenKingAttacks>;
using KnightAttacker = PrecomputedMultiAttacker<detail::GenKnightAttacks>;

using PawnSinglePusher =
    PrecomputedColouredMultiAttacker<detail::GenPawnSinglePushes>;
using PawnDoublePusher =
    PrecomputedColouredMultiAttacker<detail::GenPawnDoublePushes>;
using PawnAttacker = PrecomputedColouredMultiAttacker<detail::GenPawnCaptures>;

// PEXT is faster if available.
#ifdef USE_PEXT
using BishopAttacker =
    PextAttacker<detail::GenBishopAttacks, detail::GenBishopMask>;
using RookAttacker = PextAttacker<detail::GenRookAttacks, detail::GenRookMask>;
#else
using BishopAttacker =
    PlainMagicAttacker<detail::GenBishopAttacks, detail::GenBishopMask>;
using RookAttacker =
    PlainMagicAttacker<detail::GenRookAttacks, detail::GenRookMask>;
#endif

static_assert(SlidingAttacker<BishopAttacker>);
static_assert(SlidingAttacker<RookAttacker>);

}  // namespace move::attack

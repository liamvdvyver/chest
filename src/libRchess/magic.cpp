#include <bitset>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "board.h"
#include "magic.h"

using namespace board;

//
// Public
//

Magics::Magics() {
    init_masks();
    init_shifts();
    init_magics();
};

Magics::~Magics() {
    // TODO: check automatically freed
    // delete[] rook_masks;
    // delete[] bishop_masks;
    // delete[] rook_magics;
    // delete[] bishop_magics;
}

bitboard_t Magics::get_attack_set(Piece p, square_t sq, bitboard_t occ) const {
    bitboard_t *attack_map = get_attack_map(p, sq);
    if (!attack_map) {
        throw std::invalid_argument("get_attack_set()");
    }
    return attack_map[get_magic_key(p, sq, occ)];
};

//
// Attack map sizing
//

constexpr int Magics::n_keys(Piece p) {
    switch (p) {
    case Piece::ROOK:
        return n_rook_attacks_keys;
    case Piece::BISHOP:
        return n_bishop_attack_keys;
    default:
        return -1;
    }
};

//
// Blocker masks
//

// TODO: optimise (low-priority)
void Magics::init_masks() {
    init_rook_masks();
    init_bishop_masks();
}

void Magics::init_rook_masks() {

    int max_shift_found = 0;

    for (int sq = 0; sq < n_squares; sq++) {

        bitboard_t &mask = m_rook_masks[sq];
        mask = 0;

        int f = file(sq);
        int r = rank(sq);

        // Assign along rank
        for (int f_cur = 1; f_cur < board_size - 1; f_cur++) {

            // Skip on same file
            if (f_cur == f)
                continue;

            mask |= to_bitboard(to_square(f_cur, r));
        }

        // Assign along file
        for (int r_cur = 1; r_cur < board_size - 1; r_cur++) {

            // Skip on same rank
            if (r_cur == r)
                continue;

            mask |= to_bitboard(to_square(f, r_cur));
        }

        if (size(mask) > max_shift_found)
            max_shift_found = size(mask);
    }

    assert(max_shift_found == max_rook_shift);
};

// Helper: provide boundary conditions for iteration in generation of bishop
// blocker masks.
constexpr bool in_bounds_for_bishop_mask(int f, int r) {
    return (f > 0 && r > 0 && f < board_size - 1 && r < board_size - 1);
}

void Magics::init_bishop_masks() {

    int max_shift_found = 0;
    for (int sq = 0; sq < n_squares; sq++) {

        bitboard_t &mask = m_bishop_masks[sq];
        mask = 0;

        int f = file(sq);
        int r = rank(sq);

        // Assign down, left
        for (int d = 1; in_bounds_for_bishop_mask(f - d, r - d); d++) {
            mask |= to_bitboard(to_square(f - d, r - d));
        }

        // Assign down, right
        for (int d = 1; in_bounds_for_bishop_mask(f + d, r - d); d++) {
            mask |= to_bitboard(to_square(f + d, r - d));
        }

        // Assign up, left
        for (int d = 1; in_bounds_for_bishop_mask(f - d, r + d); d++) {
            mask |= to_bitboard(to_square(f - d, r + d));
        }

        // Assign up, left
        for (int d = 1; in_bounds_for_bishop_mask(f + d, r + d); d++) {
            mask |= to_bitboard(to_square(f + d, r + d));
        }
        if (size(mask) > max_shift_found)
            max_shift_found = size(mask);
    }

    assert(max_shift_found == max_bishop_shift);
};

const bitboard_t &Magics::get_mask(Piece p, square_t sq) const {

    // Get masks for piece
    const bitboard_t *masks;
    switch (p) {
    case Piece::ROOK:
        masks = m_rook_masks;
        break;
    case Piece::BISHOP:
        masks = m_bishop_masks;
        break;
    default:
        throw std::invalid_argument("get_mask()");
    }

    // Get mask for square
    return masks[sq];
}

//
// Attack maps
//

bool Magics::init_attacks(Piece p, magic_t magic, square_t sq) {

    std::vector<magic_key_t> visited;

    bitboard_t *coord_attack_map = get_attack_map(p, sq);

    bitboard_t all_blockers = get_mask(p, sq);

    bitboard_t blocker_subset = 0;
    magic_key_t key = 0;

    do {
        bitboard_t attacked = get_attacks(p, sq, blocker_subset);
        key = get_magic_key(p, sq, blocker_subset, magic);
        visited.push_back(key);

        bitboard_t &cur_elm = coord_attack_map[key];

        // Collision with different val
        if (cur_elm && cur_elm != attacked) {

            for (const magic_key_t visited_key : visited) {
                coord_attack_map[visited_key] = 0;
            }

            return false;
        }

        cur_elm = attacked;
        blocker_subset = next_subset_of(blocker_subset, all_blockers);

    } while (blocker_subset);

    // Success
    std::cout << "Found magic: " << std::bitset<8 * sizeof(magic_t)>(magic)
              << " for piece: " << to_char(p)
              << " at square: " << std::to_string(sq) << std::endl;

    return true;
};

bitboard_t Magics::get_attacks(Piece p, square_t sq, bitboard_t blk) {
    switch (p) {
    case Piece::ROOK:
        return get_rook_attacks(sq, blk);
    case Piece::BISHOP:
        return get_bishop_attacks(sq, blk);
    default:
        throw std::invalid_argument("get_attacks()");
    }
}

board::bitboard_t Magics::get_rook_attacks(board::square_t sq,
                                           board::bitboard_t blk) {

    bitboard_t ret = 0;
    int f = file(sq);
    int r = rank(sq);
    bitboard_t cur = to_bitboard(sq);
    bitboard_t next;

    int d_to_left = f;
    int d_to_right = board_size - f - 1;
    int d_to_bottom = r;
    int d_to_top = board_size - r - 1;

    // Attacks to left
    next = cur;
    for (int d = 1; d <= d_to_left; d++) {
        next = shift_left(next);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks to right
    next = cur;
    for (int d = 1; d <= d_to_right; d++) {
        next = shift_right(next);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks down
    next = cur;
    for (int d = 1; d <= d_to_bottom; d++) {
        next = shift_down(next);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks up
    next = cur;
    for (int d = 1; d <= d_to_top; d++) {
        next = shift_up(next);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    return ret;
};

board::bitboard_t Magics::get_bishop_attacks(board::square_t sq,
                                             board::bitboard_t blk) {

    bitboard_t ret = 0;
    int f = file(sq);
    int r = rank(sq);
    bitboard_t cur = to_bitboard(sq);
    bitboard_t next;

    // Assign down, left
    next = cur;
    for (int d = 1; is_legal_square(f - d, r - d); d++) {
        next = shift_down(shift_left(next));
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign down, right
    next = cur;
    for (int d = 1; is_legal_square(f + d, r - d); d++) {
        next = shift_down(shift_right(next));
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign up, left
    next = cur;
    for (int d = 1; is_legal_square(f - d, r + d); d++) {
        next = shift_up(shift_left(next));
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign up, right
    next = cur;
    for (int d = 1; is_legal_square(f + d, r + d); d++) {
        next = shift_up(shift_right(next));
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    return ret;
};

bitboard_t *Magics::get_attack_map(Piece p, square_t sq) const {
    if (sq >= n_squares)
        return nullptr;
    switch (p) {
    case Piece::ROOK:
        return (bitboard_t *)rook_attacks[sq];
    case Piece::BISHOP:
        return (bitboard_t *)bishop_attacks[sq];
    default:
        return nullptr;
    }
}

//
// Magic numbers
//

void Magics::init_magics() {

    magic_t rook_magic_num;
    magic_t bishop_magic_num;
    bool rook_done;
    bool bishop_done;

    // IDK WTF THIS DOES
    std::random_device rd;
    std::mt19937_64 eng(rd());

    std::uniform_int_distribution<magic_t> rand;

    for (int sq = 0; sq < n_squares; sq++) {

        rook_done = false;
        bishop_done = false;

        do {
            rook_magic_num = rand(eng) & rand(eng) & rand(eng);
            rook_done = init_attacks(Piece::ROOK, rook_magic_num, square_t(sq));
        } while (!rook_done);
        rook_magics[sq] = rook_magic_num;

        do {
            bishop_magic_num = rand(eng) & rand(eng) & rand(eng);
            bishop_done =
                init_attacks(Piece::BISHOP, bishop_magic_num, square_t(sq));
        } while (!bishop_done);
        bishop_magics[sq] = bishop_magic_num;
    }
}

magic_t Magics::get_magic(Piece p, square_t sq) const {
    const magic_t *magic_map = get_magic_map(p);
    if (magic_map) {
        return magic_map[sq];
    }
    throw std::invalid_argument("get_magic()");
}

magic_t *Magics::get_magic_map(Piece p) const {
    switch (p) {
    case Piece::ROOK:
        return (magic_t *)rook_magics;
    case Piece::BISHOP:
        return (magic_t *)bishop_magics;
    default:
        return nullptr;
    }
}

//
// Keys
//

void Magics::init_shifts() {
    for (square_t sq = 0; sq < n_squares; sq++) {
        m_rook_shifts[sq] = size(get_mask(Piece::ROOK, sq));
        m_bishop_shifts[sq] = size(get_mask(Piece::BISHOP, sq));
    }
}

int Magics::get_shift_width(Piece p, square_t sq) const {
    switch (p) {
    case Piece::ROOK:
        return n_squares - m_rook_shifts[sq];
        break;
    case Piece::BISHOP:
        return n_squares - m_bishop_shifts[sq];
    default:
        throw std::invalid_argument("get_shift_width()");
    }
}

magic_key_t Magics::get_magic_key(Piece p, square_t sq, bitboard_t occ) const {
    return get_magic_key(p, sq, occ, get_magic(p, sq));
}

magic_key_t Magics::get_magic_key(Piece p, square_t sq, bitboard_t occ,
                                  magic_t magic) const {
    bitboard_t mask = get_mask(p, sq);
    return ((occ & mask) * magic) >> get_shift_width(p, sq);
}

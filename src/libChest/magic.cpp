#include <iostream>
#include <random>

#include "magic.h"

using namespace board;

namespace move {
namespace movegen {

//
// Public
//

// Singleton
Magics &Magics::get_instance() {
    static Magics instance;
    return instance;
}

Magics::Magics()
    : m_rook_masks{}, m_bishop_masks{}, rook_attacks{}, bishop_attacks{},
      rook_magics{}, bishop_magics{}, m_rook_shifts{}, m_bishop_shifts{} {
    init_masks();
    init_shifts();
    init_magics();
};

Bitboard Magics::get_attack_set(Piece p, Square sq, Bitboard occ) const {
    Bitboard *attack_map = get_attack_map(p, sq);
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

void Magics::init_masks() {
    init_rook_masks();
    init_bishop_masks();
}

void Magics::init_rook_masks() {

    int max_shift_found = 0;

    for (Square sq : Square::AllSquareIterator()) {

        Bitboard &mask = m_rook_masks[sq];
        mask = 0;

        int f = sq.file();
        int r = sq.rank();

        // Assign along rank
        for (int f_cur = 1; f_cur < board_size - 1; f_cur++) {

            // Skip on same file
            if (f_cur == f)
                continue;

            mask |= Bitboard(Square(f_cur, r));
        }

        // Assign along file
        for (int r_cur = 1; r_cur < board_size - 1; r_cur++) {

            // Skip on same rank
            if (r_cur == r)
                continue;

            mask |= Bitboard(Square(f, r_cur));
        }

        if (mask.size() > max_shift_found)
            max_shift_found = mask.size();
    }

    assert(max_shift_found == max_rook_shift);
};

void Magics::init_bishop_masks() {

    int max_shift_found = 0;
    for (Square sq : Square::AllSquareIterator()) {

        Bitboard &mask = m_bishop_masks[sq];
        mask = 0;

        int f = sq.file();
        int r = sq.rank();

        int n_to_bot_left = std::min(f - 1, r - 1);
        int n_to_bot_right = std::min(board_size - f - 2, r - 1);
        int n_to_top_left = std::min(f - 1, board_size - r - 2);
        int n_to_top_right = std::min(board_size - f - 2, board_size - r - 2);

        // Assign down, left
        Bitboard b = Bitboard(sq);
        for (int i = 0; i < n_to_bot_left; i++) {
            b = b.shift(-1, -1);
            mask |= b;
        }

        // Assign down, right
        b = Bitboard(sq);
        for (int i = 0; i < n_to_bot_right; i++) {
            b = b.shift(1, -1);
            mask |= b;
        }

        // Assign up, left
        b = Bitboard(sq);
        for (int i = 0; i < n_to_top_left; i++) {
            b = b.shift(-1, 1);
            mask |= b;
        }

        // Assign up, right
        b = Bitboard(sq);
        for (int i = 0; i < n_to_top_right; i++) {
            b = b.shift(1, 1);
            mask |= b;
        }
        if (mask.size() > max_shift_found)
            max_shift_found = mask.size();
    }

    assert(max_shift_found == max_bishop_shift);
};

const Bitboard &Magics::get_mask(Piece p, Square sq) const {

    // Get masks for piece
    const Bitboard *masks;
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

bool Magics::init_attacks(Piece p, magic_t magic, Square sq,
                          std::vector<magic_key_t> &visited) {

    visited.clear();

    Bitboard *coord_attack_map = get_attack_map(p, sq);

    Bitboard all_blockers = get_mask(p, sq);

    magic_key_t key = 0;

    for (Bitboard blocker_subset : all_blockers.subsets()) {

        Bitboard attacked = get_attacks(p, sq, blocker_subset);
        key = get_magic_key(p, sq, blocker_subset, magic);

        Bitboard &cur_elm = coord_attack_map[key];

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
    std::cout << "Found magic: " << (bitboard_t)magic
              << " for piece: " << io::to_char(p)
              << " at square: " << std::to_string(sq) << std::endl;

    return true;
};

Bitboard Magics::get_attacks(Piece p, Square sq, Bitboard blk) {
    switch (p) {
    case Piece::ROOK:
        return get_rook_attacks(sq, blk);
    case Piece::BISHOP:
        return get_bishop_attacks(sq, blk);
    default:
        throw std::invalid_argument("get_attacks()");
    }
}

board::Bitboard Magics::get_rook_attacks(board::Square sq,
                                         board::Bitboard blk) {

    Bitboard ret = 0;
    int f = sq.file();
    int r = sq.rank();
    Bitboard cur = Bitboard(sq);
    Bitboard next;

    int d_to_left = f;
    int d_to_right = board_size - f - 1;
    int d_to_bottom = r;
    int d_to_top = board_size - r - 1;

    // Attacks to left
    next = cur;
    for (int d = 1; d <= d_to_left; d++) {
        next = next.shift(Direction::W);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks to right
    next = cur;
    for (int d = 1; d <= d_to_right; d++) {
        next = next.shift(Direction::E);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks down
    next = cur;
    for (int d = 1; d <= d_to_bottom; d++) {
        next = next.shift(Direction::S);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Attacks up
    next = cur;
    for (int d = 1; d <= d_to_top; d++) {
        next = next.shift(Direction::N);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    return ret;
};

board::Bitboard Magics::get_bishop_attacks(board::Square sq,
                                           board::Bitboard blk) {

    Bitboard ret = 0;
    int f = sq.file();
    int r = sq.rank();
    Bitboard cur = Bitboard(sq);
    Bitboard next;

    // Assign down, left
    next = cur;
    for (int d = 1; Square::is_legal(f - d, r - d); d++) {
        next = next.shift(Direction::SW);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign down, right
    next = cur;
    for (int d = 1; Square::is_legal(f + d, r - d); d++) {
        next = next.shift(Direction::SE);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign up, left
    next = cur;
    for (int d = 1; Square::is_legal(f - d, r + d); d++) {
        next = next.shift(Direction::NW);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    // Assign up, right
    next = cur;
    for (int d = 1; Square::is_legal(f + d, r + d); d++) {
        next = next.shift(Direction::NE);
        ret |= next;
        if (blk & next) {
            break;
        }
    }

    return ret;
};

Bitboard *Magics::get_attack_map(Piece p, Square sq) const {
    if (sq >= n_squares)
        return nullptr;
    switch (p) {
    case Piece::ROOK:
        return (Bitboard *)rook_attacks[sq];
    case Piece::BISHOP:
        return (Bitboard *)bishop_attacks[sq];
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

    std::uniform_int_distribution<bitboard_t> rand;

    std::vector<magic_key_t> visited;

    for (int sq = 0; sq < n_squares; sq++) {
        rook_done = false;
        do {
            rook_magic_num = rand(eng) & rand(eng) & rand(eng);
            rook_done =
                init_attacks(Piece::ROOK, rook_magic_num, Square(sq), visited);
        } while (!rook_done);
        rook_magics[sq] = rook_magic_num;
    }

    for (int sq = 0; sq < n_squares; sq++) {
        bishop_done = false;
        do {
            bishop_magic_num = rand(eng) & rand(eng) & rand(eng);
            bishop_done = init_attacks(Piece::BISHOP, bishop_magic_num,
                                       Square(sq), visited);
        } while (!bishop_done);
        bishop_magics[sq] = bishop_magic_num;
    }
}

magic_t Magics::get_magic(Piece p, Square sq) const {
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
    for (Square sq : Square::AllSquareIterator()) {
        m_rook_shifts[sq] = get_mask(Piece::ROOK, sq).size();
        m_bishop_shifts[sq] = get_mask(Piece::BISHOP, sq).size();
    }
}

int Magics::get_shift_width(Piece p, Square sq) const {
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

magic_key_t Magics::get_magic_key(Piece p, Square sq, Bitboard occ) const {
    return get_magic_key(p, sq, occ, get_magic(p, sq));
}

magic_key_t Magics::get_magic_key(Piece p, Square sq, Bitboard occ,
                                  magic_t magic) const {
    Bitboard mask = get_mask(p, sq);
    return (bitboard_t)((occ & mask) * magic) >> get_shift_width(p, sq);
}
} // namespace magic
} // namespace move

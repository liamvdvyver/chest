#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>

#include "board.h"
#include "magic.h"

using namespace board;

// -1 on bad input
constexpr int Magics::n_keys(Piece piece) {
    switch (piece) {
    case Piece::ROOK:
        return n_rook_attacks_keys;
    case Piece::BISHOP:
        return n_bishop_attack_keys;
    default:
        return -1;
    }
};

// Can be null
magic_t *Magics::get_magic_map(Piece piece) {
    switch (piece) {
    case Piece::ROOK:
        return rook_magics;
    case Piece::BISHOP:
        return bishop_magics;
    default:
        return nullptr;
    }
}

// Can be null
constexpr Bitboard *Magics::get_attack_map(Piece piece, square_t square_no) {
    if (square_no >= n_squares)
        return nullptr;
    switch (piece) {
    case Piece::ROOK:
        return rook_attacks[square_no];
    case Piece::BISHOP:
        return bishop_attacks[square_no];
    default:
        return nullptr;
    }
}

// Can be null
const Bitboard &Magics::get_mask(Piece piece, Coord coord) {

    // Get masks for piece
    const Bitboard *masks;
    switch (piece) {
    case Piece::ROOK:
        masks = rook_masks;
        break;
    case Piece::BISHOP:
        masks = bishop_masks;
        break;
    default:
        throw std::invalid_argument("get_mask()");
    }

    // Get mask for square
    return masks[coord.get_square()];
}

magic_t Magics::get_magic(Piece piece, Coord coord) {
    magic_t *magic_map = get_magic_map(piece);
    if (magic_map) {
        return magic_map[coord.get_square()];
    }
    throw std::invalid_argument("get_magic()");
}

int Magics::get_shift_width(Piece piece, Coord coord) {
    return get_mask(piece, coord).size();
}

magic_key_t Magics::get_magic_key(Piece piece, Coord coord, Bitboard occupancy,
                                  magic_t key) {
    return ((occupancy.get_board() & get_mask(piece, coord).get_board()) *
            key) >>
           (n_squares - get_shift_width(piece, coord));
}

magic_key_t Magics::get_magic_key(Piece piece, Coord coord,
                                  Bitboard occupancy) {
    return get_magic_key(piece, coord, occupancy, get_magic(piece, coord));
}

// TODO: profile this shit
void Magics::init_masks() {
    init_rook_masks();
    init_bishop_masks();
}

void Magics::init_rook_masks() {

    int max_shift_found = 0;

    for (int s = 0; s < n_squares; s++) {

        rook_masks[s] = Bitboard();
        Bitboard &b = rook_masks[s];
        Coord c = Coord(s);

        int c_i = c.get_x();
        int c_j = c.get_y();

        // Assign along rank
        for (int i = 1; i < board_size - 1; i++) {

            // Skip on same file
            if (i == c_i)
                continue;

            b.add(Coord(i, c_j));
        }

        // Assign along file
        for (int j = 1; j < board_size - 1; j++) {

            // Skip on same rank
            if (j == c_j)
                continue;

            b.add(Coord(c_i, j));
        }

        if (b.size() > max_shift_found)
            max_shift_found = b.size();
    }

    assert(max_shift_found == max_rook_shift);
};

inline bool Magics::in_bounds_for_bishop_mask(int i, int j) {
    return (i > 0 && j > 0 && i < board_size - 1 && j < board_size - 1);
}

inline bool Magics::in_bounds(int i, int j) {
    return (i >= 0 && j >= 0 && i < board_size && j < board_size);
}

void Magics::init_bishop_masks() {

    int max_shift_found = 0;
    for (int s = 0; s < n_squares; s++) {
        bishop_masks[s] = Bitboard();
        Bitboard &b = bishop_masks[s];
        Coord c = Coord(s);

        int c_i = c.get_x();
        int c_j = c.get_y();

        // Assign down, left
        for (int d = 1; in_bounds_for_bishop_mask(c_i - d, c_j - d); d++) {
            b.add(Coord(c_i - d, c_j - d));
        }

        // Assign down, right
        for (int d = 1; in_bounds_for_bishop_mask(c_i + d, c_j - d); d++) {
            b.add(Coord(c_i + d, c_j - d));
        }

        // Assign up, left
        for (int d = 1; in_bounds_for_bishop_mask(c_i - d, c_j + d); d++) {
            b.add(Coord(c_i - d, c_j + d));
        }

        // Assign up, left
        for (int d = 1; in_bounds_for_bishop_mask(c_i + d, c_j + d); d++) {
            b.add(Coord(c_i + d, c_j + d));
        }
        if (b.size() > max_shift_found)
            max_shift_found = b.size();
    }

    assert(max_shift_found == max_bishop_shift);
};

board::Bitboard Magics::get_rook_attacks(board::Coord coord,
                                         board::Bitboard blockers) {

    Bitboard ret = 0;

    int d_to_left = coord.get_x();
    int d_to_right = board_size - coord.get_x() - 1;
    int d_to_bottom = coord.get_y();
    int d_to_top = board_size - coord.get_y() - 1;

    for (int d_i = 1; d_i <= d_to_left; d_i++) {
        Bitboard next = Bitboard(Coord(coord.get_x() - d_i, coord.get_y()));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    for (int d_i = 1; d_i <= d_to_right; d_i++) {
        Bitboard next = Bitboard(Coord(coord.get_x() + d_i, coord.get_y()));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    for (int d_j = 1; d_j <= d_to_bottom; d_j++) {
        Bitboard next = Bitboard(Coord(coord.get_x(), coord.get_y() - d_j));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    for (int d_j = 1; d_j <= d_to_top; d_j++) {
        Bitboard next = Bitboard(Coord(coord.get_x(), coord.get_y() + d_j));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    return ret;
};

board::Bitboard Magics::get_bishop_attacks(board::Coord coord,
                                           board::Bitboard blockers) {

    Bitboard ret = 0;
    int c_i = coord.get_x();
    int c_j = coord.get_y();

    // Assign down, left
    for (int d = 1; in_bounds(c_i - d, c_j - d); d++) {
        Bitboard next = Bitboard(Coord(c_i - d, c_j - d));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    // Assign down, right
    for (int d = 1; in_bounds(c_i + d, c_j - d); d++) {
        Bitboard next = Bitboard(Coord(c_i + d, c_j - d));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    // Assign up, left
    for (int d = 1; in_bounds(c_i - d, c_j + d); d++) {
        Bitboard next = Bitboard(Coord(c_i - d, c_j + d));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    // Assign up, right
    for (int d = 1; in_bounds(c_i + d, c_j + d); d++) {
        Bitboard next = Bitboard(Coord(c_i + d, c_j + d));
        ret.add_all(next);
        if (blockers.get_board() & next.get_board()) {
            break;
        }
    }

    return ret;
};

Bitboard Magics::get_attacks(Piece piece, Coord coord, Bitboard blockers) {
    switch (piece) {
    case Piece::ROOK:
        return get_rook_attacks(coord, blockers);
    case Piece::BISHOP:
        return get_bishop_attacks(coord, blockers);
    default:
        throw std::invalid_argument("get_attacks()");
    }
}

bool Magics::init_attacks(Piece piece, magic_t magic, Coord coord) {

    Bitboard *coord_attack_map = get_attack_map(piece, coord.get_square());

    Bitboard all_blockers = get_mask(piece, coord);

    Bitboard blocker_subset = 0;
    magic_key_t key = 0;

    do {
        Bitboard attacked = get_attacks(piece, coord, blocker_subset);
        key = get_magic_key(piece, coord, blocker_subset, magic);

        Bitboard &cur_elm = coord_attack_map[key];

        // Collision with different val
        if (cur_elm.get_board() &&
            cur_elm.get_board() != attacked.get_board()) {

            memset(coord_attack_map, 0, sizeof(Bitboard) * n_keys(piece));
            return false;
        }

        cur_elm = attacked;
        blocker_subset.next_subset_of(all_blockers);

    } while (blocker_subset.get_board());

    // success: return true
    std::cout << "Finished attacks, magic: " << std::to_string(magic)
              << ", squares: " << std::to_string(coord.get_square())
              << std::endl;

    return true;
};

void Magics::init_magics() {

    magic_t rook_magic_num;
    magic_t bishop_magic_num;
    bool rook_done;
    bool bishop_done;

    // IDK WTF THIS DOES
    std::random_device
        rd; // Get a random seed from the OS entropy device, or whatever
    std::mt19937_64 eng(rd()); // Use the 64-bit Mersenne Twister 19937
                               // generator and seed it with entropy.

    std::uniform_int_distribution<magic_t> rand;

    for (int sq = 0; sq < n_squares; sq++) {

        rook_done = false;
        bishop_done = false;

        do {
            rook_magic_num = rand(eng) & rand(eng) & rand(eng);
            rook_done = init_attacks(Piece::ROOK, rook_magic_num, Coord(sq));
        } while (!rook_done);
        rook_magics[sq] = rook_magic_num;

        do {
            bishop_magic_num = rand(eng) & rand(eng) & rand(eng);
            bishop_done =
                init_attacks(Piece::BISHOP, bishop_magic_num, Coord(sq));
        } while (!bishop_done);
        bishop_magics[sq] = bishop_magic_num;

        std::cout << "finished square: " << std::to_string(sq) << std::endl;
    }
}

Magics::Magics() {

    init_masks();

    init_magics();
};

Magics::~Magics() {
    // TODO: check automatically freed
    // delete[] rook_masks;
    // delete[] bishop_masks;
    // delete[] rook_magics;
    // delete[] bishop_magics;
}

Bitboard Magics::get_attack_set(Piece piece, Coord coord, Bitboard occupancy) {
    Bitboard *attack_map = get_attack_map(piece, coord.get_square());
    if (!attack_map) {
        throw std::invalid_argument("get_attack_set()");
    }
    return attack_map[get_magic_key(piece, coord, occupancy)];
};

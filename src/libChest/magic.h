#ifndef MAGIC_H
#define MAGIC_H

#include <vector>

#include "board.h"

namespace move {
namespace magic {

// Magic numbers
typedef board::Bitboard magic_t;

// Attack table keys
typedef uint16_t magic_key_t;

//
// Allows fast (constant) sliding piece move generation
//

class Magics {

  public:
    // Singleton accessor
    static Magics &get_instance();

    // Delete copy constructor/assignment operator
    // Magics(const Magics &) = delete;
    // Magics &operator=(const Magics &) = delete;

    // Get all squares attacked by a piece.
    // May include own pieces.
    board::Bitboard get_attack_set(board::Piece p, board::Square sq,
                                   board::Bitboard occ) const;

  private:
    // Singeton instance
    static Magics *m_instance;

    // No external creation/deletion
    Magics();
    ~Magics();

    //
    // Attack map sizing
    //

    // Maximum key (bit) size for a rook attack
    static constexpr int max_rook_shift = 12;

    // Maximum key (bit) size for a bishop attack
    static constexpr int max_bishop_shift = 9;

    // Number of possible rook attack keys
    static const int n_rook_attacks_keys = 1 << (max_rook_shift);

    // Number of possible bishop attack keys
    static const int n_bishop_attack_keys = 1 << (max_bishop_shift);

    // Number of possible keys for a type of piece
    // Returns -1 on bad input.
    static constexpr int n_keys(board::Piece);

    //
    // Blocker masks
    //

    // Relevant rook-blocking mask (per-position)
    board::Bitboard m_rook_masks[board::n_squares];

    // Relevant bishop-blocking mask (per-position)
    board::Bitboard m_bishop_masks[board::n_squares];

    // Populate blocker masks
    void init_masks();

    // Populate rook blocker mask
    void init_rook_masks();

    // Populate bishop blocker mask
    void init_bishop_masks();

    // Get the blocker mask for piece at a position
    const board::Bitboard &get_mask(board::Piece p, board::Square sq) const;

    //
    // Attack maps
    //

    // Rook attack maps (per-position, per key)
    board::Bitboard rook_attacks[board::n_squares][n_rook_attacks_keys];

    // Bishop attack maps (per-position, per key)
    board::Bitboard bishop_attacks[board::n_squares][n_bishop_attack_keys];

    // Populate attack maps for a position, given a candidate magic number.
    // Returns whether successful.
    bool init_attacks(board::Piece p, magic_t magic, board::Square sq,
                      std::vector<magic_key_t> &visited);

    // Get the attack set for a piece at a position, given occupancy (relevant
    // blockers or total occupancy)
    static board::Bitboard get_attacks(board::Piece p, board::Square sq,
                                       board::Bitboard blk);

    // Get the attack set for a rook at a position, given occupancy (relevant
    static board::Bitboard get_rook_attacks(board::Square sq,
                                            board::Bitboard blk);

    // Get the attack set for a bishop at a position, given occupancy (relevant
    static board::Bitboard get_bishop_attacks(board::Square sq,
                                              board::Bitboard blk);

    // Helper: Get the attack map for a given piece at a given position.
    // May return null.
    board::Bitboard *get_attack_map(board::Piece p, board::Square sq) const;

    //
    // Magic numbers
    //

    // Populate all magic numbers
    void init_magics();

    // Rook magic number (per-position)
    magic_t rook_magics[board::n_squares];

    // Bishop magic number (per-position)
    magic_t bishop_magics[board::n_squares];

    // Get the magic number for a piece at a given position
    magic_t get_magic(board::Piece p, board::Square sq) const;

    // Helper: Get the magic number map for a given piece.
    // May return null.
    magic_t *get_magic_map(board::Piece p) const;

    //
    // Keys
    //

    // Rook attack map key sizes (per-position)
    int m_rook_shifts[board::n_squares];

    // Bishop attack map key sizes (per-position)
    int m_bishop_shifts[board::n_squares];

    // Populate attack map key sizes
    void init_shifts();

    // Get the shift width for a piece at a given position
    int get_shift_width(board::Piece p, board::Square sq) const;

    // Get the attack map key, given piece, position, blockers (relevant or
    // total)
    magic_key_t get_magic_key(board::Piece p, board::Square sq,
                              board::Bitboard occ) const;

    // Get the attack map key, given piece, position, blockers (relevant or
    // total), and a magic number
    magic_key_t get_magic_key(board::Piece p, board::Square sq,
                              board::Bitboard occ, magic_t magic) const;
};
} // namespace magic
} // namespace move

#endif

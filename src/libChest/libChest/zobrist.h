#pragma once

#include <array>
#include <bitset>
#include <concepts>
#include <iostream>
#include <random>

#include "board.h"
#include "incremental.h"
#include "state.h"
#include "wrapper.h"

using zobrist_t = uint64_t;

// Contains random numbers for zobrist hash generation.
// Actual hash generation/update is done by Zobrist class.
class ZobristRandoms {
   public:
    ZobristRandoms() {
        std::random_device rd;
        std::mt19937_64 eng(rd());
        std::uniform_int_distribution<zobrist_t> rand;

        for (const board::Colour c : board::colours) {
            for (const board::Piece p : board::PieceTypesIterator()) {
                for (const board::Square sq :
                     board::Square::AllSquareIterator()) {
                    assert(m_piece_hashes[piece_hash_idx({c, p}, sq)] == 0);
                    m_piece_hashes[piece_hash_idx({c, p}, sq)] = rand(eng);
                }
            }
        }

        m_black_hash = rand(eng);

        // Init single-square castling hashes
        for (const board::ColouredPiece cp :
             state::CastlingInfo::castling_squares) {
            uint8_t mask =
                static_cast<uint8_t>(state::CastlingRights::square_mask(cp));
            m_castling_hashes[mask] = rand(eng);
        }

        // for (zobrist_t &hash : m_castling_hashes) {
        for (uint8_t i = 1; i < m_castling_hashes.size(); i++) {
            // Not yet set -> not a singleton
            if (!m_castling_hashes[i]) {
                // xor of all members contained
                for (const board::ColouredPiece cp :
                     state::CastlingInfo::castling_squares) {
                    // If cp is contained in the castling rights bitset
                    if ((state::CastlingRights(i)).get_square_rights(cp)) {
                        const uint8_t square_mask = static_cast<uint8_t>(
                            state::CastlingRights::square_mask(cp));
                        m_castling_hashes[i] ^= m_castling_hashes[square_mask];
                    }
                }
            }
        }

        for (zobrist_t &hash : m_ep_hashes) {
            hash = rand(eng);
        }
    }

    constexpr zobrist_t get_piece_hash(const board::ColouredPiece cp,
                                       const board::Square sq) const {
        return m_piece_hashes[piece_hash_idx(cp, sq)];
    }

    constexpr zobrist_t get_to_move_hash(const board::Colour c) const {
        return c == board::Colour::BLACK ? m_black_hash : 0;
    }

    constexpr zobrist_t get_castling_rights_hash(
        const state::CastlingRights rights) const {
        return m_castling_hashes[static_cast<uint8_t>(rights)];
    }

    constexpr zobrist_t get_ep_file_hash(const board::Square sq) const {
        return m_ep_hashes[sq.file()];
    }

   private:
    static constexpr size_t piece_hash_idx(board::ColouredPiece cp,
                                           board::Square sq) {
        return static_cast<size_t>(sq) +
               board::n_squares *
                   (static_cast<size_t>(cp.piece) +
                    board::n_pieces * static_cast<size_t>(cp.colour));
    };
    std::array<zobrist_t, board::n_colours * board::n_pieces * board::n_squares>
        m_piece_hashes{};
    zobrist_t m_black_hash;
    std::array<zobrist_t, board::board_size> m_ep_hashes;
    std::array<zobrist_t, state::CastlingRights::max + 1> m_castling_hashes{0};

    friend struct Zobrist;
};

struct Zobrist : Wrapper<zobrist_t, Zobrist> {
   public:
    // Must initialise with a hash generator
    constexpr Zobrist() : Wrapper() {};

    constexpr Zobrist(const state::State &state) : Wrapper(0) {
        // Add piece values
        for (const board::Colour c : board::colours) {
            for (const board::Piece p : board::PieceTypesIterator()) {
                for (const board::Bitboard loc :
                     state.copy_bitboard({c, p}).singletons()) {
                    value ^= s_hasher.get_piece_hash(
                        {c, p}, loc.single_bitscan_forward());
                }
            }
        }

        // Add side to move
        value ^= s_hasher.get_to_move_hash(state.to_move);

        // Add castling rights
        // value ^= s_hasher.get_castling_rights_hash(state.castling_rights);
        toggle_castling_rights(state.castling_rights);

        if (state.ep_square.has_value()) {
            value ^= s_hasher.get_ep_file_hash(state.ep_square.value());
        }
    }

    constexpr Zobrist(const state::AugmentedState &astate)
        : Zobrist(astate.state) {};

    // Make incrementally updateable

    constexpr void add(board::Bitboard loc, board::ColouredPiece cp) {
        value ^= s_hasher.get_piece_hash(cp, loc.single_bitscan_forward());
    };
    constexpr void remove(board::Bitboard loc, board::ColouredPiece cp) {
        value ^= s_hasher.get_piece_hash(cp, loc.single_bitscan_forward());
    };
    constexpr void move(board::Bitboard from, board::Bitboard to,
                        board::ColouredPiece cp) {
        remove(from, cp);
        add(to, cp);
    };
    constexpr void swap(board::Bitboard loc, board::ColouredPiece from,
                        board::ColouredPiece to) {
        remove(loc, from);
        add(loc, to);
    };
    constexpr void swap_oppside(board::Bitboard loc, board::ColouredPiece from,
                                board::ColouredPiece to) {
        swap(loc, from, to);
    };
    constexpr void swap_sameside(board::Bitboard loc, board::Colour side,
                                 board::Piece from, board::Piece to) {
        swap(loc, {side, from}, {side, to});
    };
    constexpr void toggle_castling_rights(state::CastlingRights rights) {
        value ^= s_hasher.get_castling_rights_hash(rights);
    };
    constexpr void add_ep_sq(board::Square sq) {
        value ^= s_hasher.get_ep_file_hash(sq);
    }
    constexpr void remove_ep_sq(board::Square sq) {
        value ^= s_hasher.get_ep_file_hash(sq);
    }

    constexpr void set_to_move(board::Colour to_move) {
        (void)to_move;
        value ^= s_hasher.m_black_hash;
    }

   private:
    // HACK:to make zobrist hash constructible just by state,
    // provide a default instance.
    // static const ZobristRandoms{default_zobrist_randoms};
    // const ZobristRandoms &m_hasher{default_zobrist_randoms};
    inline static const ZobristRandoms s_hasher{};
};
static_assert(IncrementallyUpdateable<Zobrist>);

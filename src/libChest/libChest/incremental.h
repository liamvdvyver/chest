#ifndef INCREMENTAL_H
#define INCREMENTAL_H

#include <concepts>

#include "board.h"
#include "state.h"

//
// Types which support incremental updates.
//

// As moves are made/unmade during the process of search, many different
// structures (i.e. occupancy, hashes, eval, etc.) might be able to be
// incrementally updated.
//
// Any such structure must support the following standard incremental
// operations, then, they can be made/unmade.
template <typename T>
concept IncrementallyUpdateable =
    requires(T t, const board::Bitboard from, const board::Bitboard to,
             const board::Bitboard loc, const board::Square ep_sq,
             const board::Colour colour, const board::Piece piece,
             const board::ColouredPiece cp) {
        // State should be initialised from AugmentedState.
        { std::constructible_from<state::AugmentedState> };

        // Change a piece's location on the same bitboard
        { t.move(from, to, cp) } -> std::same_as<void>;

        // Add a square on a bitboard (assumed to be empty)
        { t.add(loc, cp) } -> std ::same_as<void>;

        // Remove a square on a bitboard (assumed to be filled)
        { t.remove(loc, cp) } -> std ::same_as<void>;

        // Swap a piece from one bitboard to another (general case)
        { t.swap(loc, cp, cp) } -> std::same_as<void>;

        // Swap a piece from one bitboard to another (belonging to the other
        // player)
        { t.swap_oppside(loc, cp, cp) } -> std::same_as<void>;

        // Swap a piece from one bitboard to another (belonging to the same
        // player)
        { t.swap_sameside(loc, colour, piece, piece) } -> std::same_as<void>;

        // Update castling rights

        { t.add_castling_rights(cp) } -> std::same_as<void>;
        { t.add_castling_rights(colour) } -> std::same_as<void>;
        { t.remove_castling_rights(cp) } -> std::same_as<void>;
        { t.remove_castling_rights(colour) } -> std::same_as<void>;

        { t.add_ep_sq(ep_sq) } -> std::same_as<void>;
        { t.remove_ep_sq(ep_sq) } -> std::same_as<void>;

        // Increment move counters,
        // Called before set_to_move()
        // { t.increment_halfmove() } -> std::same_as<void>;
        // { t.decrement_halfmove() } -> std::same_as<void>;

        { t.set_to_move(colour) } -> std::same_as<void>;
    };

// When a type receives updates, but it is not favourable to incrementally
// update, ignore updates.
template <typename T>
class IgnoreUpdates {
   public:
    constexpr void add(board::Bitboard loc, board::ColouredPiece cp) const {
        (void)loc;
        (void)cp;
    };
    constexpr void remove(board::Bitboard loc, board::ColouredPiece cp) const {
        (void)loc;
        (void)cp;
    };
    constexpr void move(board::Bitboard from, board::Bitboard to,
                        board::ColouredPiece cp) const {
        (void)from;
        (void)to;
        (void)cp;
    };
    constexpr void swap(board::Bitboard loc, board::ColouredPiece from,
                        board::ColouredPiece to) const {
        (void)loc;
        (void)from;
        (void)to;
    };
    constexpr void swap_oppside(board::Bitboard loc, board::ColouredPiece from,
                                board::ColouredPiece to) const {
        (void)loc;
        (void)from;
        (void)to;
    };
    constexpr void swap_sameside(board::Bitboard loc, board::Colour side,
                                 board::Piece from, board::Piece to) const {
        (void)loc;
        (void)side;
        (void)from;
        (void)to;
    };
    constexpr void add_castling_rights(board::ColouredPiece cp) const {
        (void)cp;
    };
    constexpr void add_castling_rights(board::Colour colour) const {
        (void)colour;
    };
    constexpr void remove_castling_rights(board::ColouredPiece cp) const {
        (void)cp;
    };
    constexpr void remove_castling_rights(board::Colour colour) const {
        (void)colour;
    };

    constexpr void add_ep_sq(board::Square ep_sq) const { (void)ep_sq; }
    constexpr void remove_ep_sq(board::Square ep_sq) const { (void)ep_sq; }

    // constexpr void increment_halfmove() const {}
    // constexpr void decrement_halfmove() const {}

    constexpr void set_to_move(const board::Colour to_move) const {
        (void)to_move;
    }
};
static_assert(IncrementallyUpdateable<IgnoreUpdates<int>>);

static_assert(IncrementallyUpdateable<state::State>);
static_assert(IncrementallyUpdateable<state::AugmentedState>);

#endif

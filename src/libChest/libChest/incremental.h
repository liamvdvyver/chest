#ifndef INCREMENTAL_H
#define INCREMENTAL_H

#include <concepts>

#include "board.h"

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
    requires(T t, board::Bitboard bb, board::ColouredPiece cp,
             board::Colour colour, board::Piece piece) {
        // Change a piece's location on the same bitboard
        { t.move(bb, bb, cp) } -> std::same_as<void>;

        // Add a square on a bitboard (assumed to be empty)
        { t.add(bb, cp) } -> std ::same_as<void>;

        // Remove a square on a bitboard (assumed to be filled)
        { t.remove(bb, cp) } -> std ::same_as<void>;

        // Swap a piece from one bitboard to another (general case)
        { t.swap(bb, cp, cp) } -> std::same_as<void>;

        // Swap a piece from one bitboard to another (belonging to the other
        // player)
        { t.swap_oppside(bb, cp, cp) } -> std::same_as<void>;

        // Swap a piece from one bitboard to another (belonging to the same
        // player)
        { t.swap_sameside(bb, colour, piece, piece) } -> std::same_as<void>;

        // Perform other actions whenever a piece is moved from its origin
        // square (e.g. update castling rights)
        //
        // I'll see if I actually use this one
        { t.remove_castling_rights(cp) } -> std::same_as<void>;
        { t.remove_castling_rights(colour) } -> std::same_as<void>;
    };

// When a type receives updates, but it is not favourable to incrementally
// update, ignore updates.
template <typename T>
class IgnoreUpdates {
   public:
    constexpr void add(board::Bitboard loc, board::ColouredPiece cp) {
        (void)loc;
        (void)cp;
    };
    constexpr void remove(board::Bitboard loc, board::ColouredPiece cp) {
        (void)loc;
        (void)cp;
    };
    constexpr void move(board::Bitboard from, board::Bitboard to,
                        board::ColouredPiece cp) {
        (void)from;
        (void)to;
        (void)cp;
    };
    constexpr void swap(board::Bitboard loc, board::ColouredPiece from,
                        board::ColouredPiece to) {
        (void)loc;
        (void)from;
        (void)to;
    };
    constexpr void swap_oppside(board::Bitboard loc, board::ColouredPiece from,
                                board::ColouredPiece to) {
        (void)loc;
        (void)from;
        (void)to;
    };
    constexpr void swap_sameside(board::Bitboard loc, board::Colour side,
                                 board::Piece from, board::Piece to) {
        (void)loc;
        (void)side;
        (void)from;
        (void)to;
    };
    constexpr void remove_castling_rights(board::ColouredPiece cp) const {
        (void)cp;
    };
    constexpr void remove_castling_rights(board::Colour colour) const {
        (void)colour;
    };
};
static_assert(IncrementallyUpdateable<IgnoreUpdates<int>>);

#endif

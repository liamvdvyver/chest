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

        // Toggle (add/remove) a square on a bitboard
        { t.toggle(bb, cp) } -> std ::same_as<void>;

        // Swap a piece from one bitboard to another (general case)
        { t.swap(bb, cp, cp) } -> std::same_as<void>;

        // Swap a piece from one bitboard to another (belonging to the other
        // player)
        { t.swap_oppside(bb, cp, cp) } -> std::same_as<void>;

        // Swap a piece from one bitboard to another (belonging to the same
        // player)
        { t.swap_sameside(bb, colour, piece, piece) } -> std::same_as<void>;

        // Remove a piece from a bitboard and move another to its location
        // { t.capture(bb, cp, cp) } -> std::same_as<void>;

        // Perform other actions whenever a piece is moved from its origin
        // square (e.g. update castling rights)
        //
        // I'll see if I actually use this one
        { t.remove_castling_rights(cp) } -> std::same_as<void>;
        { t.remove_castling_rights(colour) } -> std::same_as<void>;
    };

#endif

#ifndef STATE_H
#define STATE_H

#include "board.h"
#include "move.h"

#include <optional>
#include <vector>

namespace state {

typedef std::string fen_t; // FEN strings

// Standard game setup
static const fen_t new_game_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Container for piece of a certain colour
struct ColouredPiece {
    board::Piece piece;
    board::Colour colour;
};

// Store complete game state.
// Does not track n-fold repetitions.
// Does not pre-compute any redundant information.
//
// Most members are directly accessible, except:
// * Piece bitboard representation,
// * Castling rights,
// since the way these are stored is subject to change.
//

static constexpr int n_castling_sides = 2; // For array sizing

struct State {

  public:
    // Blank state
    constexpr State() : m_pieces{}, m_castling_rights{} {}

    // State from fen string
    constexpr State(const fen_t &fen_string) : State() {

        // Parse FEN string
        std::vector<std::string> parts;

        int len = fen_string.length();
        int curIdx = 0;
        int offset;
        char delim = ' ';

        do {
            offset = fen_string.substr(curIdx, len).find(delim);
            if ((size_t)offset == std::string::npos) {
                parts.push_back(fen_string.substr(curIdx));
            } else {
                parts.push_back(fen_string.substr(curIdx, offset));
            }
            curIdx += (offset + 1);
        } while ((size_t)offset != std::string::npos);

        if (parts.size() != 6)
            throw std::invalid_argument("FEN string must have 6 fields.");

        // Parse placement
        fen_t &placements = parts.at(0);

        int rowIdx = (board::board_size - 1), colIdx = 0;

        for (int charIdx = 0; charIdx < (int)placements.length(); charIdx++) {

            assert(rowIdx <= board::board_size);
            assert(colIdx <= board::board_size);

            if (isdigit(placements[charIdx])) {
                int n_space = stoi(placements.substr(charIdx, 1));
                colIdx += n_space;

            } else if (placements[charIdx] == '/') {
                assert(colIdx == board::board_size);
                colIdx = 0;
                rowIdx--;

            } else {

                assert(isalpha(placements[charIdx]));
                board::Colour colour = isupper(placements[charIdx])
                                           ? board::Colour::WHITE
                                           : board::Colour::BLACK;
                board::Piece piece = board::io::from_char(placements[charIdx]);
                get_bitboard(piece, colour) |=
                    board::Bitboard(board::Square(colIdx, rowIdx));
                colIdx++;
            };
        }

        // Parse the rest

        // Side to move
        std::string &to_move = parts.at(1);
        if (to_move.length() != 1)
            throw std::invalid_argument("Side to move must have length 1.");
        switch (to_move.at(0)) {
        case 'b': {
            this->to_move = board::Colour::BLACK;
            break;
        }
        case 'w': {
            this->to_move = board::Colour::WHITE;
            break;
        }
        default:
            throw std::invalid_argument(
                "Side to move must be one of: 'b', 'w'");
            break;
        }

        // Castling rights
        const std::string &castling_rights_str = parts.at(2);
        if (castling_rights_str.length() > 4) {
            throw std::invalid_argument("Castling rights must have length < 4");
        }
        if (castling_rights_str.length() == 1 && castling_rights_str == "-") {
            // no rights
        } else {

            board::Colour colour;
            board::Piece side;

            for (int i = 0; i < (int)castling_rights_str.length(); i++) {
                colour = isupper(castling_rights_str.at(i))
                             ? board::Colour::WHITE
                             : board::Colour::BLACK;
                side = board::io::from_char(castling_rights_str.at(i));

                if (side != board::Piece::QUEEN && side != board::Piece::KING)
                    throw std::invalid_argument(
                        "Castling rights must specify queen or king only");

                if (get_castling_rights(side, colour))
                    throw std::invalid_argument(
                        "Castling rights may not be redundant");

                set_castling_rights(side, colour, true);
            }
        }

        // EP square
        std::string ep_str = parts.at(3);
        if (ep_str.length() == 1 && ep_str == "-") {
            ep_square = {};
        } else {
            ep_square = board::io::to_square(ep_str);
        }

        // HM clock
        std::string hm_clock_str = parts.at(4);
        halfmove_clock = std::stoi(hm_clock_str);

        // FM clock
        std::string fm_clock_str = parts.at(4);
        fullmove_number = std::stoi(fm_clock_str);
    }

    // Helper: default new game state
    static const State new_game() { return State(new_game_fen); }

    // Last en-passant square, and whether current or not
    std::optional<board::Square> ep_square;

    // Plies since capture/pawn push
    int halfmove_clock;

    // (2-ply) moves since game start
    int fullmove_number;

    // Side to move
    board::Colour to_move;

    // Position accessors

    constexpr board::Bitboard &get_bitboard(board::Piece piece,
                                            board::Colour colour) {
        return m_pieces[(int)colour][(int)piece];
    }

    constexpr board::Bitboard copy_bitboard(board::Piece piece,
                                            board::Colour colour) const {
        return m_pieces[(int)colour][(int)piece];
    }

    // Castling rights accessors

    constexpr bool get_castling_rights(board::Piece side,
                                       board::Colour colour) const {
        return (1) &
               (m_castling_rights >> castling_rights_offset(side, colour));
    }

    // TODO: test, and make it faster
    constexpr void set_castling_rights(board::Piece side, board::Colour colour,
                                       bool rights) {
        int selected_bit = (1 << castling_rights_offset(side, colour));
        int selected_bit_val = (rights << castling_rights_offset(side, colour));
        m_castling_rights =
            (m_castling_rights & ~selected_bit) ^ selected_bit_val;
    }
    // Union of piece bitboards, per side
    constexpr board::Bitboard side_occupancy(board::Colour colour) const {
        board::Bitboard ret = 0;

        for (int i = 0; i < board::n_pieces; i++) {
            ret |= m_pieces[(int)colour][i];
        }

        return ret;
    }

    // Union of piece bitboards, both sides
    constexpr board::Bitboard total_occupancy() const {
        return side_occupancy(board::Colour::BLACK) |
               side_occupancy(board::Colour::WHITE);
    }

    // First piece matching mask
    constexpr std::optional<ColouredPiece> const
    piece_at(board::Bitboard bit) const {
        for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
            for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {

                if (!(m_pieces[colourIdx][pieceIdx] & bit).empty()) {
                    return {{.piece = (board::Piece)pieceIdx,
                             .colour = (board::Colour)colourIdx}};
                }
            }
        }
        return {};
    }

    // First piece matching mask of given colour
    constexpr std::optional<board::Piece> const
    piece_at(board::Bitboard bit, board::Colour colour) const {
        for (int pieceIdx = 0; pieceIdx < board::n_pieces; pieceIdx++) {

            if (!(m_pieces[(int)colour][pieceIdx] & bit).empty()) {
                return (board::Piece)pieceIdx;
            }
        }
        return {};
    }

    // Pretty printing
    constexpr std::string pretty() const {
        std::string ret = "";
        for (int r = board::board_size - 1; r >= 0; r--) {
            for (int c = 0; c < board::board_size; c++) {

                std::optional<ColouredPiece> atLoc =
                    piece_at(board::Bitboard(board::Square(c, r)));

                if (atLoc.has_value()) {
                    char retChar = board::io::to_char(atLoc.value().piece);
                    if (atLoc.value().colour == board::Colour::WHITE) {
                        retChar = toupper(retChar);
                    }
                    ret += retChar;
                } else
                    ret += ".";
            }
            ret += "\n";
        }
        return ret;
    };

  private:
    // Position of all pieces for each player
    board::Bitboard m_pieces[board::n_colours][board::n_pieces];

    // Castling rights for each player, for each side (king/queen)
    uint8_t m_castling_rights;

    // Helper: defines layout of castling rights bitset
    constexpr int castling_rights_offset(board::Piece side,
                                         board::Colour colour) const {
        assert(side == board::Piece::QUEEN || side == board::Piece::KING);
        return (2 * (int)colour) + ((int)(side == board::Piece::KING));
    }
};

constexpr std::ostream &operator<<(std::ostream &os, State s) {
    return (os << s.pretty());
};

// Contains the basic state, plus some precomputed values, (which can be
// incrementally updated), future possible uses include hashes, occupancy,
// attack/defend maps, etc.
//
// In search, movegen, etc, this is the object which should be passed around.
// Then the underlying state representation can be changed, but accessing any
// member of this struct (e.g. occupancy) is guaranteed not to require
// recomputation within the same search node.
struct AugmentedState {

  public:
    AugmentedState(State state)
        : state(state), total_occupancy(state.total_occupancy()),
          m_side_occupancy{state.side_occupancy((board::Colour)0),
                           state.side_occupancy((board::Colour)1)} {};

    State state;
    board::Bitboard total_occupancy;

    // Helper accessor for side_occupancy bitsets
    // TODO: is there a better way to ensure const correctness like this?
    board::Bitboard &side_occupancy(board::Colour colour) {
        return m_side_occupancy[(int)colour];
    }
    board::Bitboard &side_occupancy() { return side_occupancy(state.to_move); }
    board::Bitboard &opponent_occupancy() {
        return side_occupancy(!state.to_move);
    }
    const board::Bitboard &side_occupancy(board::Colour colour) const {
        return m_side_occupancy[(int)colour];
    }
    const board::Bitboard &side_occupancy() const {
        return side_occupancy(state.to_move);
    }
    const board::Bitboard &opponent_occupancy() const {
        return side_occupancy(!state.to_move);
    }

  private:
    board::Bitboard m_side_occupancy[board::n_colours];
};

}; // namespace state

#endif

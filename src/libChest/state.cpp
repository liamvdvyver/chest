#include "state.h"
#include "jumping.h"
#include "move.h"
#include <cstddef>
#include <optional>

using namespace board;

namespace state {

//
// Constructors
//

State::State() : m_pieces{}, m_castling_rights{} {}

State::State(const fen_t &fen_string) : State() {

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

    int rowIdx = (board_size - 1), colIdx = 0;

    for (int charIdx = 0; charIdx < (int)placements.length(); charIdx++) {

        assert(rowIdx <= board_size);
        assert(colIdx <= board_size);

        if (isdigit(placements[charIdx])) {
            int n_space = stoi(placements.substr(charIdx, 1));
            colIdx += n_space;

        } else if (placements[charIdx] == '/') {
            assert(colIdx == board_size);
            colIdx = 0;
            rowIdx--;

        } else {

            assert(isalpha(placements[charIdx]));
            Colour colour =
                isupper(placements[charIdx]) ? Colour::WHITE : Colour::BLACK;
            Piece piece = board::io::from_char(placements[charIdx]);
            get_bitboard(piece, colour) |= Bitboard(Square(colIdx, rowIdx));
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
        this->to_move = Colour::BLACK;
        break;
    }
    case 'w': {
        this->to_move = Colour::WHITE;
        break;
    }
    default:
        throw std::invalid_argument("Side to move must be one of: 'b', 'w'");
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

        Colour colour;
        Piece side;

        for (int i = 0; i < (int)castling_rights_str.length(); i++) {
            colour = isupper(castling_rights_str.at(i)) ? Colour::WHITE
                                                        : Colour::BLACK;
            side = board::io::from_char(castling_rights_str.at(i));

            if (side != Piece::QUEEN && side != Piece::KING)
                throw std::invalid_argument(
                    "Castling rights must specify queen or king only");

            if (can_castle(side, colour))
                throw std::invalid_argument(
                    "Castling rights may not be redundant");

            can_castle(side, colour) = true;
        }
    }

    // EP square
    std::string ep_str = parts.at(3);
    if (ep_str.length() == 1 && ep_str == "-") {
        m_ep = {};
    } else {
        m_ep = board::io::to_square(ep_str);
    }

    // HM clock
    std::string hm_clock_str = parts.at(4);
    m_halfmove_clock = std::stoi(hm_clock_str);

    // FM clock
    std::string fm_clock_str = parts.at(4);
    m_fullmove_number = std::stoi(fm_clock_str);
}

State State::new_game() { return State(new_game_fen); }

//
// Accessors
//

Bitboard &State::get_bitboard(Piece piece, Colour colour) {
    return m_pieces[(int)colour][(int)piece];
}

Bitboard State::copy_bitboard(Piece piece, Colour colour) const {
    return m_pieces[(int)colour][(int)piece];
}

bool &State::can_castle(Piece side, Colour colour) {
    assert(side == Piece::QUEEN || side == Piece::KING);

    return m_castling_rights[(int)colour][(int)side];
}

//
// Others
//

Bitboard State::side_occupancy(Colour colour) const {
    Bitboard ret = 0;

    for (int i = 0; i < n_pieces; i++) {
        ret |= m_pieces[(int)colour][i];
    }

    return ret;
}

Bitboard State::total_occupancy() const {
    return side_occupancy(Colour::BLACK) | side_occupancy(Colour::WHITE);
}

std::optional<ColouredPiece> const State::piece_at(Bitboard bit) const {
    for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
        for (int pieceIdx = 0; pieceIdx < n_pieces; pieceIdx++) {

            if (!(m_pieces[colourIdx][pieceIdx] & bit).empty()) {
                return {
                    {.piece = (Piece)pieceIdx, .colour = (Colour)colourIdx}};
            }
        }
    }
    return {};
}

//
// Move generation
//
void State::get_pseudolegal_moves(std::vector<move::Move> &moves) {

    // Just pawn pushes for now
    // TODO: get all moves

    Bitboard pawns = get_bitboard(Piece::PAWN, to_move);
    Bitboard cur_pawn = 0;
    Square cur_square_from = 0;
    Square cur_square_to = 0;
    Bitboard cur_pawn_pushes = 0;

    while (!pawns.empty()) {

        cur_pawn = pawns.pop_ls1b();

        cur_square_from = cur_pawn.single_bitscan_forward();
        cur_pawn_pushes =
            move::jumping::PawnMoveGenerator::get_push_map(cur_pawn, to_move);

        cur_pawn_pushes = cur_pawn_pushes.setdiff(total_occupancy());

        while (!cur_pawn_pushes.empty()) {

            cur_square_to = cur_pawn_pushes.pop_ls1b().single_bitscan_forward();

            move::Move cur_move = move::Move(cur_square_from, cur_square_to);
            moves.push_back(cur_move);
        }
    }
};

//
// Pretty printing
//

std::string State::pretty() const {
    std::string ret = "";
    for (int r = board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board_size; c++) {

            std::optional<ColouredPiece> atLoc =
                piece_at(Bitboard(Square(c, r)));

            if (atLoc.has_value()) {
                char retChar = board::io::to_char(atLoc.value().piece);
                if (atLoc.value().colour == Colour::WHITE) {
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

std::ostream &operator<<(std::ostream &os, State s) {
    return (os << s.pretty());
};
bool State::en_passant_active() const { return m_ep.has_value(); };

board::Square State::en_passant_square() const { return m_ep.value(); };
} // namespace state

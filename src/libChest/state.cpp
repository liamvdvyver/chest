#include "state.h"

using namespace board;

namespace state {

//
// Constructors
//

State::State() : m_castling_rights{}, m_pieces{} {}

State::State(const fen_t &fen_string) : State() {

    // Parse FEN string
    std::vector<std::string> parts;

    int len = fen_string.length();
    int curIdx = 0;
    int offset;
    char delim = ' ';

    do {
        offset = fen_string.substr(curIdx, len).find(' ');
        if (offset == std::string::npos) {
            parts.push_back(fen_string.substr(curIdx));
        } else {
            parts.push_back(fen_string.substr(curIdx, offset));
        }
        curIdx += (offset + 1);
    } while (offset != std::string::npos);

    if (parts.size() != 6)
        throw std::invalid_argument("FEN string must have 6 fields.");

    // Parse placement
    fen_t &placements = parts.at(0);

    int rowIdx = (board_size - 1), colIdx = 0;

    for (int charIdx = 0; charIdx < placements.length(); charIdx++) {

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
            Piece piece = from_char(placements[charIdx]);
            get_bitboard(piece, colour) |=
                to_bitboard(to_square(colIdx, rowIdx));
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

        for (int i = 0; i < castling_rights_str.length(); i++) {
            colour = isupper(castling_rights_str.at(i)) ? Colour::WHITE
                                                        : Colour::BLACK;
            side = from_char(castling_rights_str.at(i));

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
        m_ep = {.active = false};
    } else {
        m_ep = {.square = to_square(ep_str), .active = true};
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

bitboard_t &State::get_bitboard(Piece piece, Colour colour) {
    return m_pieces[(int)colour][(int)piece];
}

bool &State::can_castle(Piece side, Colour colour) {
    assert(side == Piece::QUEEN || side == Piece::KING);

    return m_castling_rights[(int)colour][(int)side];
}

//
// Others
//

bitboard_t State::side_occupancy(Colour colour) const {
    bitboard_t ret = 0;

    for (int i = 0; i < n_pieces; i++) {
        ret |= m_pieces[(int)colour][i];
    }

    return ret;
}

bitboard_t State::total_occupancy() const {
    return side_occupancy(Colour::BLACK) | side_occupancy(Colour::WHITE);
}

opt_coloured_piece_t const State::piece_at(bitboard_t bit) const {
    for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
        for (int pieceIdx = 0; pieceIdx < n_pieces; pieceIdx++) {

            if (m_pieces[colourIdx][pieceIdx] & bit) {
                return {.piece = {.piece = (Piece)pieceIdx,
                                  .colour = (Colour)colourIdx},
                        .found = true};
            }
        }
    }
    return {.found = false};
}

//
// Pretty printing
//

std::string State::pretty() const {
    std::string ret = "";
    for (int r = board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board_size; c++) {

            bitboard_t b1 = to_bitboard(to_square(c, r));
            bitboard_t b = to_bitboard(to_square(c, r));

            opt_coloured_piece_t atLoc = piece_at(to_bitboard(to_square(c, r)));

            if (atLoc.found) {
                char retChar = to_char(atLoc.piece.piece);
                if (atLoc.piece.colour == Colour::WHITE) {
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
} // namespace state

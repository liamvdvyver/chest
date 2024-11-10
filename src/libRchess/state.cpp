#include "state.h"
#include "board.h"

using namespace board;

Bitboard &State::get_bitboard(Piece piece, Colour colour) {
    return pieces[(int)colour][(int)piece];
}

void State::set_bitboard(Piece piece, Colour colour, Bitboard board) {
    pieces[(int)colour][(int)piece] = board;
}

Bitboard State::total_occupancy() {
    Bitboard ret = side_occupancy(Colour::WHITE);
    ret.add_all(side_occupancy(Colour::BLACK));
    return ret;
}

Bitboard State::side_occupancy(Colour colour) {
    Bitboard ret = 0;

    // TODO: optimise if needed
    for (int i = 0; i < n_pieces; i++) {
        ret.add_all(pieces[(int)colour][i]);
    }

    return ret;
}

// Assumes no double up
std::optional<std::pair<Piece, Colour>> const
State::piece_at(bitboard_t bit) const {
    for (int colourIdx = 0; colourIdx <= 1; colourIdx++) {
        for (int pieceIdx = 0; pieceIdx < n_pieces; pieceIdx++) {

            if ((pieces[colourIdx][pieceIdx].get_board() & bit) != 0) {
                return std::pair((Piece)pieceIdx, (Colour)colourIdx);
            }
        }
    }
    return std::optional<std::pair<Piece, Colour>>();
}

bool State::can_castle(Piece side, Colour colour) const {
    if (side != Piece::QUEEN && side != Piece::KING)
        throw std::invalid_argument("Side must be KING or QUEEN");

    return castling_rights[(int)colour][(int)side];
}

void State::remove_castling_rights(Piece side, Colour colour) {
    if (side != Piece::QUEEN && side != Piece::KING)
        throw std::invalid_argument("Side must be KING or QUEEN");

    castling_rights[(int)colour][(int)side] = false;
}

State::State() {

    // no castling rights
    for (int i = 0; i < n_colours; i++) {
        for (int j = 0; j < n_castling_sides; j++) {
            std::cout << std::to_string(i) << std::to_string(j) << std::endl;
            std::cout << std::addressof(castling_rights[i][j]) << std::endl;
            castling_rights[i][j] = false;
        }
    };

    // init to empty board
    memset(pieces, 0, sizeof(pieces));
}

State::State(const fen_t &fen_string) : State() {

    // Parse FEN string
    std::vector<std::string> parts;

    int len = fen_string.length();
    std::cout << len << std::endl;
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
            get_bitboard(piece, colour).add(Coord(colIdx, rowIdx));
            colIdx++;
        };
    }

    for (const auto &part : parts) {
        // std::cout << "printing" << std::endl;
        std::cout << part << std::endl;
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

            castling_rights[(int)colour][(int)side] = true;
        }
    }

    // EP square
    std::string ep_str = parts.at(3);
    if (ep_str.length() == 1 && ep_str == "-") {
        // No en-passant
    } else {
        // TODO: implement
        en_passant_squares = Coord(ep_str);
    }

    // HM clock
    std::string hm_clock_str = parts.at(4);
    halfmove_clock = std::stoi(hm_clock_str);

    // FM clock
    std::string fm_clock_str = parts.at(4);
    fullmove_number = std::stoi(fm_clock_str);
}

std::ostream &operator<<(std::ostream &os, State s) {
    std::string ret = "";
    for (int r = board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board_size; c++) {

            bitboard_t b1 = Bitboard(Coord(c, r)).get_board();
            Bitboard b = Bitboard(Coord(c, r)).get_board();

            std::optional<std::pair<Piece, Colour>> atLoc =
                s.piece_at(Bitboard(Coord(c, r)).get_board());
            if (atLoc.has_value()) {
                char retChar = to_char(atLoc->first);
                if (atLoc->second == Colour::WHITE) {
                    retChar = toupper(retChar);
                }
                ret += retChar;
            } else
                ret += ".";
        }
        ret += "\n";
    }
    return (os << ret);
};

const std::optional<board::Coord> State::get_en_passant_squares() const {
    return en_passant_squares;
};

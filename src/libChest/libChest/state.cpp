#include "state.h"

#include <string>
#include <vector>

#include "board.h"

//
// Defines IO for game state
// all other code is contexpr in the main module.
//

namespace state {

State::State(const fen_t &fen_string) : State::State() {
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
            get_bitboard({.colour = colour, .piece = piece}) |=
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
            colour = isupper(castling_rights_str.at(i)) ? board::Colour::WHITE
                                                        : board::Colour::BLACK;
            side = board::io::from_char(castling_rights_str.at(i));

            board::ColouredPiece castling_cp = {.colour = colour,
                                                .piece = side};

            if (side != board::Piece::QUEEN && side != board::Piece::KING)
                throw std::invalid_argument(
                    "Castling rights must specify queen or king only");

            if (castling_rights.get_castling_rights(castling_cp))
                throw std::invalid_argument(
                    "Castling rights may not be redundant");

            castling_rights.set_castling_rights(castling_cp, true);
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

std::string State::pretty() const {
    std::string ret = "";
    for (int r = board::board_size - 1; r >= 0; r--) {
        for (int c = 0; c < board::board_size; c++) {
            std::optional<board::ColouredPiece> atLoc =
                piece_at(board::Bitboard(board::Square(c, r)));

            if (atLoc.has_value()) {
                ret += board::io::to_uni(atLoc.value());
            } else {
                ret += ".";
            }
            ret += " ";
        }
        ret += "\n";
    }
    return ret;
};

std::string State::to_fen() const {
    std::string ret = "";

    // To move
    ret += (bool)to_move ? 'w' : 'b';
    ret += ' ';

    // Castling rights
    if (castling_rights.get_castling_rights(
            {board::Colour::WHITE, board::Piece::KING})) {
        ret += 'K';
    }
    if (castling_rights.get_castling_rights(
            {board::Colour::WHITE, board::Piece::QUEEN})) {
        ret += 'Q';
    }
    if (castling_rights.get_castling_rights(
            {board::Colour::BLACK, board::Piece::KING})) {
        ret += 'k';
    }
    if (castling_rights.get_castling_rights(
            {board::Colour::BLACK, board::Piece::QUEEN})) {
        ret += 'q';
    }
    ret += ' ';

    // EP square
    ret +=
        ep_square.has_value() ? board::io::algebraic(ep_square.value()) : "-";
    ret += ' ';

    // Halfmove clock
    ret += std::to_string(halfmove_clock);
    ret += " ";

    // Fullmove number
    ret + std::to_string(fullmove_number);

    return ret;
};

std::ostream &operator<<(std::ostream &os, const State s) {
    return (os << s.pretty());
};

}  // namespace state

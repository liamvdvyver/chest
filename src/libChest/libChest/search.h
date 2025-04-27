#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "eval.h"
#include "makemove.h"
#include "movegen.h"

//
// Basic search
//

namespace search {

struct SearchResult {
    enum class LeafType : uint8_t {
        CUTOFF,
        DRAW,
        STALEMATE,
        CHECKMATE,
    };
    LeafType type;
    eval::centipawn_t eval;
    move::Move best_move{0};
};

struct ABResult {
    SearchResult result;
    eval::centipawn_t alpha;
    eval::centipawn_t beta;
};

template <eval::StaticEvaluator TEval,
          move::movegen::PseudolegalGenerator TMover>
class DLNegaMax {

    constexpr DLNegaMax(const TEval &eval, const TMover &mover,
                        state::AugmentedState &astate, int max_depth)
        : m_eval(eval), m_mover(mover), m_node(mover, astate, max_depth),
          m_max_depth(max_depth) {};

    constexpr SearchResult search() {

        // Cutoff
        if (m_node.bottomed_out()) {
            return {.type = SearchResult::LeafType::CUTOFF,
                    .eval = m_eval.eval(m_node.m_astate),
                    .best_move = 0};
        }

        // Get children
        std::vector<move::Move> &moves = m_node.find_moves();

        // Recurse over children
        std::optional<SearchResult> best_move{};

        // Recurse
        for (move::Move m : moves) {

            m_node.make_move(m);
            SearchResult child_result = -search();

            if (!best_move.has_value() ||
                best_move.value().eval < child_result.eval) {
                best_move = child_result;
            }
        }

        // Stale/checkmate
        if (!best_move) {
            board::Square king_sq =
                m_node.m_astate.state
                    .get_bitboard(board::Piece::KING,
                                  m_node.m_astate.state.to_move)
                    .bitscan_forward();
            bool checked = m_mover.is_attacked(m_node.m_astate, king_sq);
            return {.type = checked ? SearchResult::LeafType::CHECKMATE
                                    : SearchResult::LeafType::STALEMATE,
                    .eval = checked ? -eval::max_eval : 0};
        }

        return best_move.value();
    }

  private:
    const TEval &m_eval;
    const TMover &m_mover;
    state::SearchNode<TMover> m_node;
    const int m_max_depth;
};

} // namespace search

#endif

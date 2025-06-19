#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "eval.h"
#include "libChest/move.h"
#include "libChest/movebuffer.h"
#include "libChest/state.h"
#include "makemove.h"
#include "movegen.h"
#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstdlib>
#include <mutex>

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
        TIMEOUT,
    };
    LeafType type;
    move::FatMove best_move{};
    eval::centipawn_t eval;
    size_t n_nodes; // Count of legal nodes
};

static const SearchResult cutoff_result{.type = SearchResult::LeafType::TIMEOUT,
                                        .best_move = {},
                                        .eval = 0,
                                        .n_nodes = 0};

// Basic searchers
template <typename T>
concept Searcher = requires(T t) {
    { t.search() } -> std::convertible_to<SearchResult>;
};

// Searchers which can be made to return early,
// and auto-return at a point in time
template <typename T>
// TODO: use steady clock?
concept StoppableSearcher = requires(
    T t, std::chrono::time_point<std::chrono::steady_clock> finish_time) {
    { t.stop() } -> std::same_as<void>;
    { t.search(finish_time) } -> std::convertible_to<SearchResult>;
};

// Depth-limited searches:
// * can set depth (which unstops the search)
// * can search
// * can abort search by calling stop() (from other thread), returning the best
// result so far These are usually used with iterative deepening
template <typename T>
concept DLSearcher = requires(T t, int max_depth) {
    { t.set_depth(max_depth) };
} && StoppableSearcher<T>;

//
// Random search
//

class RandomSearcher {
  public:
    RandomSearcher(const move::movegen::AllMoveGenerator<> &mover,
                   state::AugmentedState &astate)
        : m_mover(mover), m_astate(astate), m_node(m_mover, m_astate, 1) {}
    constexpr SearchResult search() {
        m_node.prep_search(1);
        MoveBuffer &moves = m_node.find_moves();
        size_t n_moves = moves.size();

        // TODO: make this less hacky
        // find random legal move (assume one exists)
        // does not evaluate
        bool legal = false;
        size_t n_nodes = 0;
        move::FatMove m;
        while (!legal) {
            int i = std::rand() % n_moves;
            m = moves.at(i);
            legal = m_node.make_move(m);
            m_node.unmake_move();
            n_nodes++;
        }
        return {SearchResult::LeafType::CUTOFF, m, 0, n_nodes};
    }

  private:
    const move::movegen::AllMoveGenerator<> &m_mover;
    state::AugmentedState &m_astate;
    state::SearchNode<1> m_node;
};
static_assert(Searcher<RandomSearcher>);

//
// Depth-limited
//

//
// Depth-limited negamax
//

struct Bounds {

  public:
    Bounds() : alpha(-eval::max_eval), beta(eval::max_eval) {};
    Bounds(eval::centipawn_t alpha, eval::centipawn_t beta)
        : alpha(alpha), beta(beta) {};

    eval::centipawn_t alpha;
    eval::centipawn_t beta;
};

// TODO: actually return node types, if its useful at some point.
// I just return NA for now.
struct ABResult {
    // Use Knuth's numbering
    enum class ABNodeType : uint8_t {
        NA = 0,  // e.g. time cutoff
        PV = 1,  // didn't fail
        CUT = 2, // failed high
        ALL = 3, // failed low
    };
    SearchResult result;
    ABNodeType type;
    operator SearchResult() const { return result; }
};
static_assert(std::convertible_to<ABResult, SearchResult>);

// Time cutoff -> don't worry about result for now
static const ABResult ab_cutoff_result{cutoff_result, ABResult::ABNodeType::NA};

template <eval::StaticEvaluator TEval, size_t MaxDepth> class DLNegaMax {

  public:
    constexpr DLNegaMax(const TEval &eval,
                        const move::movegen::AllMoveGenerator<> &mover,
                        state::AugmentedState &astate, int max_depth)
        : m_eval(eval), m_mover(mover), m_node(mover, astate, max_depth),
          m_max_depth(max_depth), m_stopped(false) {};

    constexpr void set_depth(int max_depth) {
        m_node.prep_search(max_depth);

        m_stopped = false;
    }

    // Not protected from races.
    // Calling code should ensure set_depth/stop
    // do not race.
    constexpr void stop() { m_stopped = true; }
    constexpr ABResult
    search(std::chrono::time_point<std::chrono::steady_clock> finish_time,
           Bounds bounds) {

        // Auto-stop
        if (std::chrono::steady_clock::now() > finish_time) {
            stop();
        }

        // Early return
        if (m_stopped) {
            return ab_cutoff_result;
        }

        // Cutoff -> return value
        if (m_node.bottomed_out()) {
            SearchResult search_ret = {.type = SearchResult::LeafType::CUTOFF,
                                       .best_move = {},
                                       .eval = m_eval.eval(),
                                       .n_nodes = 1};
            return {search_ret, ABResult::ABNodeType::NA};
        }

        // Get children
        MoveBuffer &moves = m_node.find_moves();

        // Recurse over children
        std::optional<SearchResult> best_move;
        size_t n_nodes = 1;

        // Recurse
        for (move::FatMove m : moves) {

            // Early return from recursion
            if (m_stopped) {
                return ab_cutoff_result;
            }

            // Check child
            if (m_node.make_move(m)) {

                SearchResult child_result =
                    search(finish_time, {-bounds.beta, -bounds.alpha});

                // Count nodes
                n_nodes += child_result.n_nodes;
                eval::centipawn_t child_eval = -child_result.eval;

                // Update best move if eval improves
                if (!(best_move.has_value()) || child_eval > best_move->eval) {
                    best_move = {.type = child_result.type,
                                 .best_move = m,
                                 .eval = child_eval,
                                 .n_nodes{}}; // count nodes later

                    // Handle pruning
                    bounds.alpha = std::max(bounds.alpha, child_eval);
                    if (bounds.alpha >= bounds.beta) {
                        // Pruned -> return lower bound
                        m_node.unmake_move();
                        break;
                    }
                }
            }
            m_node.unmake_move();
        }

        // Stale/checkmate
        if (!best_move) {
            board::Square king_sq =
                m_node.m_astate.state
                    .get_bitboard(
                        {m_node.m_astate.state.to_move, board::Piece::KING})
                    .single_bitscan_forward();
            bool checked = m_mover.is_attacked(m_node.m_astate, king_sq,
                                               m_node.m_astate.state.to_move);
            return {{.type = checked ? SearchResult::LeafType::CHECKMATE
                                     : SearchResult::LeafType::STALEMATE,
                     .eval = checked ? -eval::max_eval : 0,
                     .n_nodes = n_nodes},
                    ABResult::ABNodeType::NA};
        }

        best_move->n_nodes = n_nodes;
        return {best_move.value(), ABResult::ABNodeType::NA};
    }

    // Search with default bounds
    constexpr ABResult
    search(std::chrono::time_point<std::chrono::steady_clock> finish_time) {
        return search(finish_time, {});
    }

  private:
    const TEval &m_eval;
    const move::movegen::AllMoveGenerator<> &m_mover;
    state::SearchNode<MaxDepth> m_node;
    const int m_max_depth;
    bool m_stopped;
};
static_assert(DLSearcher<DLNegaMax<eval::MaterialEval, 1>>);

//
// Alpha
//

//
// Iterative deepening
//

// Call backs to report search statistics a the top of each loop
// These will be called in a blocking manner once search has completed
// to a depth, so should not be too slow.
struct StatReporter {

    virtual void report(int depth, eval::centipawn_t eval, size_t nodes,
                        std::chrono::duration<double> time,
                        const MoveBuffer &pv,
                        const state::AugmentedState &astate) const = 0;
};

// Given a depth-limited searcher, implement iterative deepening
// No special move ordering
// Synchronises stop/search w/ mutex.
template <DLSearcher TSearcher, int MaxDepth> class IDSearcher {

  public:
    IDSearcher(state::AugmentedState &astate, TSearcher &searcher,
               const StatReporter &reporter)
        : m_searcher(searcher), m_astate(astate), m_reporter(reporter),
          m_stopped(false) {}

    // Stops the search as soon as possible, will return to the (other) thread
    // which called search().
    constexpr void stop() {
        m_stoplock.lock();
        m_stopped = true;
        m_searcher.stop();
        m_stoplock.unlock();
    };

    // Infer return type from searcher,
    // Always searches at least to depth 1 so a legal move is returned.
    constexpr auto
    search(std::chrono::time_point<std::chrono::steady_clock> finish_time) {

        m_stoplock.lock();
        m_stopped = false;
        m_stoplock.unlock();

        std::optional<SearchResult> search_result = {};

        // Loop over all possible levels
        for (int max_depth = 1; max_depth <= MaxDepth && !m_stopped;
             max_depth++) {

            // Time/node count for reporting
            auto start_time = std::chrono::steady_clock::now();

            m_stoplock.lock();
            // Force the first ply to complete
            if (!m_stopped || max_depth == 1) {
                m_searcher.set_depth(max_depth);
            } else {

                // Will be reached if search was aborted before first ply,
                // after forcing that search to conclude
                break;
            }

            // keep locked if first ply
            if (max_depth > 1) {
                m_stoplock.unlock();
            }

            SearchResult candidate_result = m_searcher.search(finish_time);

            // if first ply, we now have a result
            if (max_depth == 1) {
                m_stoplock.unlock();
            }

            if (candidate_result.type == SearchResult::LeafType::TIMEOUT) {
                break;
            };

            search_result = {candidate_result};

            // Report partial results

            assert(search_result.has_value());

            m_pv.clear();
            m_pv.push_back(search_result->best_move);

            std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - start_time;

            // TODO: report nps for current iteration, not total?
            m_reporter.report(max_depth, search_result->eval,
                              search_result->n_nodes, elapsed, m_pv, m_astate);

            if (search_result->type == SearchResult::LeafType::CHECKMATE) {
                break;
            }
        };

        assert(search_result.has_value());
        return search_result.value();
    };

  private:
    TSearcher &m_searcher;
    state::AugmentedState &m_astate;
    const StatReporter &m_reporter;
    bool m_stopped;
    std::mutex m_stoplock;
    // For now, just report one move from pv
    MoveBuffer m_pv;
};
static_assert(
    StoppableSearcher<IDSearcher<DLNegaMax<eval::MaterialEval, 5>, 5>>);

} // namespace search

#endif

//============================================================================//
// Search algorithms
// Note: in general, searchers should be short-lived.
//============================================================================//

#pragma once

#include <algorithm>
#include <chrono>
#include <mutex>

#include "board.h"
#include "eval.h"
#include "libChest/move.h"
#include "libChest/state.h"
#include "libChest/util.h"
#include "makemove.h"
#include "movegen.h"

namespace search {

//============================================================================//
// Types and concepts
//============================================================================//

// TODO: implement integrated bound and value eval
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
    size_t n_nodes;  // Count of legal nodes
};

// When the search is terminated early, this should be returned
static constexpr SearchResult cutoff_result{
    .type = SearchResult::LeafType::TIMEOUT,
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

// Call backs to report search statistics at the top of each loop
// These will be called in a blocking manner once search has completed
// to a depth, so should not be too slow.
struct StatReporter {
    StatReporter() = default;
    StatReporter(const StatReporter &) = default;
    StatReporter(StatReporter &&) = delete;
    StatReporter &operator=(const StatReporter &) = default;
    StatReporter &operator=(StatReporter &&) = delete;
    virtual ~StatReporter() = default;

    virtual void report(const size_t depth, const eval::centipawn_t eval,
                        const size_t nodes,
                        const std::chrono::duration<double> time,
                        const MoveBuffer &pv,
                        const state::AugmentedState &astate) const = 0;
};

//============================================================================//
// Depth-limited negamax
//============================================================================//

// Passed between levels of search
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
        NA = 0,   // e.g. time cutoff
        PV = 1,   // didn't fail
        CUT = 2,  // failed high
        ALL = 3,  // failed low
    };
    SearchResult result;
    ABNodeType type;
    operator SearchResult() const { return result; }
};
static_assert(std::convertible_to<ABResult, SearchResult>);

// Time cutoff -> don't worry about result for now
static constexpr ABResult ab_cutoff_result{.result = cutoff_result,
                                           .type = ABResult::ABNodeType::NA};

template <eval::IncrementallyUpdateableEvaluator TEval, size_t MaxDepth>
class DLNegaMax {
   public:
    constexpr DLNegaMax(const move::movegen::AllMoveGenerator<> &mover,
                        state::AugmentedState &astate)
        : m_mover(mover), m_astate(astate), m_node(mover, astate, MaxDepth) {};

    constexpr void set_depth(size_t depth) {
        assert(depth <= MaxDepth);
        m_node.prep_search(depth);

        m_stopped = false;
    }

    // Not protected from races.
    // Calling code should ensure set_depth/stop
    // do not race.
    constexpr void stop() { m_stopped = true; }
    constexpr ABResult search(
        const std::chrono::time_point<std::chrono::steady_clock> finish_time,
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
            SearchResult search_ret = {
                .type = SearchResult::LeafType::CUTOFF,
                .best_move = {},
                .eval = m_node.template get<TEval>().eval(),
                .n_nodes = 1};
            return {.result = search_ret, .type = ABResult::ABNodeType::NA};
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
                                 .n_nodes{}};  // count nodes later

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
                m_astate.state
                    .get_bitboard({m_astate.state.to_move, board::Piece::KING})
                    .single_bitscan_forward();
            bool checked =
                m_mover.is_attacked(m_astate, king_sq, m_astate.state.to_move);
            return {
                .result = {.type = checked ? SearchResult::LeafType::CHECKMATE
                                           : SearchResult::LeafType::STALEMATE,
                           .eval = checked ? -eval::max_eval : 0,
                           .n_nodes = n_nodes},
                .type = ABResult::ABNodeType::NA};
        }

        best_move->n_nodes = n_nodes;
        return {.result = best_move.value(), .type = ABResult::ABNodeType::NA};
    }

    // Search with default bounds
    constexpr ABResult search(
        std::chrono::time_point<std::chrono::steady_clock> finish_time) {
        return search(finish_time, {});
    }

   private:
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    // Movegen and state should outlive searcher.
    const move::movegen::AllMoveGenerator<> &m_mover;
    state::AugmentedState &m_astate;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    state::SearchNode<MaxDepth, TEval, Zobrist> m_node;
    bool m_stopped = false;
};
static_assert(DLSearcher<DLNegaMax<eval::StdEval, 1>>);

//============================================================================//
// Iterative deepening
//============================================================================//

// Given a depth-limited searcher, implement iterative deepening
// No special move ordering
// Synchronises stop/search w/ mutex.
template <DLSearcher TSearcher, size_t MaxDepth>
class IDSearcher {
   public:
    constexpr IDSearcher(state::AugmentedState &astate, TSearcher &searcher,
                         const StatReporter &reporter)
        : m_searcher(searcher), m_astate(astate), m_reporter(reporter) {}

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
    constexpr auto search(
        const std::chrono::time_point<std::chrono::steady_clock> finish_time) {
        m_stoplock.lock();
        m_stopped = false;
        m_stoplock.unlock();

        std::optional<SearchResult> search_result = {};

        // Loop over all possible levels
        for (size_t max_depth = 1; max_depth <= MaxDepth && !m_stopped;
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
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    // Searcher should be shorter-lived than other objects.
    TSearcher &m_searcher;
    state::AugmentedState &m_astate;
    const StatReporter &m_reporter;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    bool m_stopped = false;
    std::mutex m_stoplock;

    // For now, just report one move from pv
    // TODO: triangular table
    MoveBuffer m_pv;
};
static_assert(
    StoppableSearcher<IDSearcher<DLNegaMax<eval::DefaultEval, 1>, 1>>);

}  // namespace search

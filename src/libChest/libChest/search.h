//============================================================================//
// Search algorithms
// Note: in general, searchers should be short-lived.
//============================================================================//

#pragma once

#include <mutex>

#include "eval.h"
#include "makemove.h"
#include "move.h"
#include "state.h"
#include "util.h"
#include "wrapper.h"
#include "zobrist.h"

namespace search {

//============================================================================//
// Types and concepts
//============================================================================//

//----------------------------------------------------------------------------//
// Results
//----------------------------------------------------------------------------//

// TODO: implement integrated bound and value eval
struct SearchResult {
    enum class LeafType : uint8_t {
        CUTOFF,
        DRAW,
        STALEMATE,
        CHECKMATE,
        TIMEOUT,
        STANDPAT,
    };
    LeafType type;
    move::FatMove best_move{};
    eval::centipawn_t eval;
    size_t n_nodes;  // Count of legal nodes
};

// Ordering is optimal for IBValue,
// that is, integrated bound and values are equal to
// corresponding enum values mod 2.
enum class ABNodeType : uint8_t {
    PV = 0,   // didn't fail/raised alpha (exact)
    CUT = 1,  // failed high (lower bound)
    NA = 2,   // e.g. incomplete result
    ALL = 3,  // failed low (upper bound)
};

struct ABResult {
    SearchResult result;
    ABNodeType type;
    operator SearchResult() const { return result; }
};
static_assert(std::convertible_to<ABResult, SearchResult>);

//----------------------------------------------------------------------------//
// Searching
//----------------------------------------------------------------------------//

// When the search is terminated early, this should be returned
static constexpr SearchResult timeout_result{
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

//----------------------------------------------------------------------------//
// Reporting
//----------------------------------------------------------------------------//

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
// Move ordering
//============================================================================//

//----------------------------------------------------------------------------//
// Helpers
//----------------------------------------------------------------------------//

// Combine several comparators.
template <auto... Cmp>
struct LexicographicGt;

template <auto H, auto... T>
struct LexicographicGt<H, T...> {
    static constexpr bool operator()(const auto a, const auto b) {
        if (H(a, b)) return true;
        if (H(b, a)) return false;
        return LexicographicGt<T...>::operator()(a, b);
    }
};

// Base case: no comparators left
template <>
struct LexicographicGt<> {
    static constexpr bool operator()(const auto a, const auto b) {
        (void)a;
        (void)b;
        return false;  // exhausted all comparators, treat as equal
    }
};

// Sort a particular move first if presetn, e.g. hash move, killer, etc.
struct IdentityGt {
    IdentityGt(const move::FatMove target) : m_target(target) {}

    constexpr bool operator()(const move::FatMove mv_a,
                              const move::FatMove mv_b) const {
        const bool is_a = mv_a == m_target;
        const bool is_b = mv_b == m_target;
        return is_a && !is_b;
    }

   private:
    const move::FatMove
        m_target;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

//----------------------------------------------------------------------------//
// Capture ordering
//----------------------------------------------------------------------------//

// Captures are sorted (highest to lowest) lexicographically by:
// * highest victim value,
// * then lowest attacker value
//
// Quiet moves are ordered to be always equivalent.
// A promotion is considered quiet, since it is currently not generated in
// pawn's loud line generation.
//
// This is a rough implementation, the following improvements should be
// made, contingent upon changes to pawn move generation:
// TODO: sort queen promotion captures first
// TODO: include queen promotions as last tactical move
struct MvvLva {
    constexpr MvvLva(const state::AugmentedState &astate) : m_astate(astate) {};
    constexpr bool operator()(const move::FatMove mv_a,
                              const move::FatMove mv_b) const {
        const uint8_t victim_a = victim_val(mv_a);
        const uint8_t victim_b = victim_val(mv_b);

        return (victim_a > victim_b) ||
               (victim_a && (victim_a == victim_b) &&
                (attacker_val(mv_a) < attacker_val(mv_b)));
    }

   private:
    // 0 if there is no victim (not a tactical move)
    // enum value + 1 if there is a victim
    constexpr uint8_t victim_val(const move::FatMove mv) const {
        return move::is_capture(mv.get_move().type())
                   ? static_cast<uint8_t>(
                         m_astate.state
                             .piece_at(mv.get_move().to(),
                                       !m_astate.state.to_move)
                             .transform([](board::ColouredPiece cp) {
                                 return cp.piece;
                             })

                             // Only capture with empty destination is EP
                             .value_or(board::Piece::PAWN)) +
                         1
                   : 0;
    }

    // 0 if there is no victim (not a tactical move)
    // enum value + 1 if there is a victim
    // promoted pieces are considered to be taken
    constexpr static uint8_t attacker_val(const move::FatMove mv) {
        assert(move::is_capture(mv.get_move().type()));
        return static_cast<uint8_t>(mv.get_piece());
    }

    const state::AugmentedState
        &m_astate;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

// Concrete type used currently
struct DefaultOrdering {
    DefaultOrdering(const state::AugmentedState &astate,
                    const move::FatMove hash_move)
        : m_mvv_lva(astate), m_hash_move_comparator(hash_move) {};

    constexpr bool operator()(const move::FatMove mv_a,
                              const move::FatMove mv_b) const {
        if (m_hash_move_comparator(mv_a, mv_b)) return true;
        if (m_hash_move_comparator(mv_b, mv_a)) return false;
        return m_mvv_lva(mv_a, mv_b);
    }

   private:
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    const MvvLva m_mvv_lva;
    const IdentityGt m_hash_move_comparator;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};

//============================================================================//
// Transposition tables
//============================================================================//

struct TTable {
   public:
    TTable() = default;
    TTable(size_t n) : m_size(n) {};

    //-- Value/entry types ---------------------------------------------------//
    struct TTValue {
        eval::centipawn_t score{};
        // Stored as depth + 1, so that a value of 0 indicates no value.
        uint8_t depth_remaining{};
        search::ABNodeType type{ABNodeType::NA};
        move::FatMove best_move{};
    };

    using TTEntry = std::pair<Zobrist, TTValue>;

    //-- Accessors -----------------------------------------------------------//

    const TTValue &operator[](const Zobrist idx) const {
        return get(idx).second;
    }

    TTValue &operator[](const Zobrist idx) { return get(idx).second; }

    // Optional accessor
    const std::optional<TTValue> at_opt(const Zobrist idx) const {
        return contains(idx) ? get(idx).second : std::optional<TTValue>{};
    }

    // Extracts the principal variation from a state until miss.
    template <size_t MaxDepth, typename... TComponents>
    void get_pv(MoveBuffer &buf,
                state::SearchNode<MaxDepth, TComponents...> &sn) const {
        sn.prep_search(MaxDepth);
        buf.clear();
        Zobrist hash = sn.template get<Zobrist>();
        while (contains(hash) && !sn.bottomed_out()) {
            move::FatMove best_move = get(hash).second.best_move;
            buf.push_back(best_move);
            sn.make_move(best_move);
            hash = sn.template get<Zobrist>();
        }
        sn.unmake_all();
    }

    // Checks membership
    bool contains(const Zobrist idx) const { return get(idx).first == idx; }

    // Inserts result,
    // if depth of new entry is deeper than an existing entry.
    void insert(const Zobrist idx, const ABResult result,
                const uint8_t depth_remaining) {
        // Do not overwrite results from a deeper search.
        if (contains(idx) &&
            // Entries stored depth as depth + 1.
            get(idx).second.depth_remaining > depth_remaining + 1) {
            return;
        }

        // Store the new result.
        get(idx) = {idx, TTValue{.score = result.result.eval,
                                 .depth_remaining =
                                     static_cast<uint8_t>(depth_remaining + 1),
                                 .type = result.type,
                                 .best_move = result.result.best_move}};
    };

    void clear() {
        m_entries.clear();
        m_entries.resize(m_size);
    }

   private:
    // Access helper.
    TTEntry &get(const Zobrist idx) {
        return m_entries[static_cast<size_t>(idx) % m_size];
    }

    // Access helper.
    const TTEntry &get(const Zobrist idx) const {
        return m_entries[static_cast<size_t>(idx) % m_size];
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const size_t m_size = 1024UL * 1024UL / sizeof(TTEntry);

    std::vector<TTEntry> m_entries = std::vector<TTEntry>(m_size, {{}, {}});
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

// Settings to pass into negamax
struct NegaMaxOptions {
    bool prune = true;
    bool sort = true;
    bool quiesce = true;
    bool quiescence_standpat = true;
};

template <eval::IncrementallyUpdateableEvaluator TEval, size_t MaxDepth,
          NegaMaxOptions Opts = {}>
class DLNegaMax {
   public:
    constexpr DLNegaMax(const move::movegen::AllMoveGenerator &mover,
                        state::AugmentedState &astate, TTable &ttable)
        : m_mover(mover),
          m_astate(astate),
          m_node(mover, astate, MaxDepth),
          m_ttable(ttable) {};

    constexpr void set_depth(size_t depth) {
        assert(depth <= MaxDepth);
        m_node.prep_search(depth);

        m_stopped = false;
    }

    // Not protected from races.
    // Calling code should ensure set_depth/stop do not race.
    constexpr void stop() { m_stopped = true; }
    template <SearchType Type = SearchType::NORMAL>
    constexpr ABResult search(
        const std::optional<std::chrono::time_point<std::chrono::steady_clock>>
            finish_time = std::nullopt,
        Bounds bounds = {}) {
        // Auto-stop
        if (finish_time && std::chrono::steady_clock::now() > finish_time) {
            stop();
        }

        // Early return
        if (m_stopped) {
            return ab_timeout_result;
        }

        // Cutoff -> return value
        if (m_node.template bottomed_out<Type>()) {
            // Normal search -> quiesce
            if constexpr (Type == SearchType::NORMAL && Opts.quiesce) {
                return search<SearchType::QUIESCE>(finish_time, bounds);
            } else {
                return cutoff_result();
            }
        }

        // In quiescence only: check stand-pat score
        if constexpr (Type == SearchType::QUIESCE && Opts.quiescence_standpat) {
            const eval::centipawn_t standpat_score =
                m_node.template get<TEval>().eval();
            bounds.alpha = std::max(bounds.alpha, standpat_score);
            if (standpat_score >= bounds.beta) {
                return {.result = {.type = SearchResult::LeafType::STANDPAT,
                                   .best_move = {},
                                   .eval = standpat_score,
                                   .n_nodes = 1},
                        .type = ABNodeType::NA};
            }
        }

        // Get children (in order)
        MoveBuffer &moves = search_moves<Type>();
        if constexpr (Opts.sort) {
            std::sort(
                moves.begin(), moves.end(),
                [this, hash_move](const move::FatMove a,
                                  const move::FatMove b) {
                    const bool ret = DefaultOrdering(m_astate, hash_move)(a, b);
                    if constexpr (DEBUG()) {
                        assert(!(ret &&
                                 DefaultOrdering(m_astate, hash_move)(b, a)));
                        assert(!(DefaultOrdering(m_astate, hash_move)(a, a)));
                        assert(!(DefaultOrdering(m_astate, hash_move)(b, b)));
                    }
                    return ret;
                });
        }

        // Recurse over children
        std::optional<SearchResult> best_move;
        size_t n_nodes = 1;

        // Recurse
        for (move::FatMove m : moves) {
            // Early return from recursion
            if (m_stopped) {
                return ab_timeout_result;
            }

            // Check child
            if (m_node.make_move(m)) {
                SearchResult child_result =
                    search<Type>(finish_time, {-bounds.beta, -bounds.alpha});

                if constexpr (Type == SearchType::QUIESCE) {
                    assert(move::is_capture(m.get_move().type()));
                }

                // Count nodes
                n_nodes += child_result.n_nodes;
                const eval::centipawn_t child_eval = -child_result.eval;

                // Update best move if eval improves
                if (!(best_move.has_value()) || child_eval > best_move->eval) {
                    best_move = {.type = child_result.type,
                                 .best_move = m,
                                 .eval = child_eval,
                                 .n_nodes{}};  // count nodes later
                }

                if constexpr (Opts.prune) {
                    if (child_eval > bounds.alpha) {
                        bounds.alpha = child_eval;
                    }
                    if (child_eval >= bounds.beta) {
                        // Pruned -> return lower bound
                        m_node.unmake_move();
                        break;
                    }
                }
            }
            m_node.unmake_move();
        }

        if (!best_move) {
            // If no result in quiescence search: search quiet moves.
            if constexpr (Type == SearchType::QUIESCE) {
                if (quiet_moves_exist()) {
                    return cutoff_result();
                }
            }

            // Stale/checkmate
            const board::Square king_sq =
                m_astate.state
                    .get_bitboard({.colour = m_astate.state.to_move,
                                   .piece = board::Piece::KING})
                    .single_bitscan_forward();
            const bool checked =
                m_mover.is_attacked(m_astate, king_sq, m_astate.state.to_move);
            return {
                .result = {.type = checked ? SearchResult::LeafType::CHECKMATE
                                           : SearchResult::LeafType::STALEMATE,
                           .eval = checked ? -eval::max_eval : 0,
                           .n_nodes = n_nodes},
                .type = ABNodeType::NA};
        }

        best_move->n_nodes = n_nodes;
        const ABResult ret = {.result = best_move.value(),
                              .type = ABNodeType::NA};
        m_ttable.insert(
            hash, ret,
            static_cast<uint8_t>(m_node.template depth_remaining<Type>()));
        return ret;
    }

    // Search with default bounds
    constexpr ABResult search(
        std::chrono::time_point<std::chrono::steady_clock> finish_time) {
        return search(finish_time, {});
    }

   private:
    // Time cutoff -> don't worry about result for now
    static constexpr ABResult ab_timeout_result{.result = timeout_result,
                                                .type = ABNodeType::NA};

    // Return value in (soft/hard) cutoff
    constexpr ABResult cutoff_result() const {
        SearchResult search_ret = {.type = SearchResult::LeafType::CUTOFF,
                                   .best_move = {},
                                   .eval = m_node.template get<TEval>().eval(),
                                   .n_nodes = 1};
        return {.result = search_ret, .type = ABNodeType::NA};
    }

    // Gets moves to be searched based on search type.
    template <SearchType Type>
    constexpr MoveBuffer &search_moves();

    template <>
    constexpr MoveBuffer &search_moves<SearchType::NORMAL>() {
        return m_node.template find_moves<true>();
    }

    // Return sorted
    template <>
    constexpr MoveBuffer &search_moves<SearchType::QUIESCE>() {
        return m_node.find_loud_moves();
    }

    // Quiescence helper: if no loud moves were found,
    // check if this is due to checkmate/stalemate,
    // or if there are legal quiet moves.
    constexpr bool quiet_moves_exist() {
        // Get children (in order)
        const MoveBuffer &moves = m_node.find_quiet_moves();

        for (const move::FatMove &m : moves) {
            if (m_node.make_move(m)) {
                m_node.unmake_move();
                return true;
            }
            m_node.unmake_move();
        }

        return false;
    }

    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    // Movegen and state should outlive searcher.
    const move::movegen::AllMoveGenerator &m_mover;
    state::AugmentedState &m_astate;
    TTable &m_ttable;
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

    // Stops the search as soon as possible, will return to the (other)
    // thread which called search().
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

//============================================================================//
// Search algorithms
// Note: in general, searchers should be short-lived.
//============================================================================//

#pragma once

#include <functional>
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

// Ordering is optimal for IBValue,
// that is, integrated bound and values are equal to
// corresponding enum values mod 2.
enum class ABNodeType : uint8_t {
    PV = 0,   // didn't fail/raised alpha (exact)
    CUT = 1,  // failed high (lower bound)
    ALL = 3,  // failed low (upper bound)
};

// Integrated bound and value.
struct IBValue : public Wrapper<eval::centipawn_t, IBValue> {
   private:
    using Wrapper::Wrapper;

   public:
    IBValue() = default;
    // using Wrapper::Wrapper;
    IBValue(const eval::centipawn_t score, const ABNodeType type) {
        value = score * 4;
        value += static_cast<eval::centipawn_t>(type);
        value -= static_cast<eval::centipawn_t>(type == ABNodeType::ALL) * 4;

        assert(eval() == score);
        assert(node_type() == type);
    }

    eval::centipawn_t eval() const { return (value + 1) >> 2; }

    ABNodeType node_type() const {
        return static_cast<ABNodeType>(value & 0b11);
    }

    bool exact() const { return !(0b1 & value); }

   private:
};

struct SearchResult {
    enum class LeafType : uint8_t {
        DEPTH_CUTOFF,
        DRAW,
        STALEMATE,
        CHECKMATE,
        TIMEOUT,
        STANDPAT,
        HASH_CUTOFF,
    };
    IBValue value{};
    LeafType type;
    move::FatMove best_move{};
    size_t n_nodes{};  // Count of legal nodes
};

//----------------------------------------------------------------------------//
// Searching
//----------------------------------------------------------------------------//

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

// Search node type that searchers expect.
template <eval::IncrementallyUpdateableEvaluator TEval, size_t MaxDepth>
using DefaultNode =
    state::SearchNodeWithHistory<MaxDepth, state::default_history_size, TEval,
                                 Zobrist>;

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
                        const MoveBuffer &pv) const = 0;

    virtual void debug_log(const std::string_view &msg) const = 0;
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
    move::FatMove m_target;
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
                         m_astate.get()
                             .state
                             .piece_at(mv.get_move().to(),
                                       !m_astate.get().state.to_move)
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

    std::reference_wrapper<const state::AugmentedState> m_astate;
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
    MvvLva m_mvv_lva;
    IdentityGt m_hash_move_comparator;
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
        IBValue value{};
        // Stored as depth + 1, so that a value of 0 indicates no value.
        uint8_t depth_remaining{};
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
    template <size_t MaxDepth, size_t HistorySz, typename... TComponents>
    void get_pv(MoveBuffer &buf,
                state::SearchNodeWithHistory<MaxDepth, HistorySz,
                                             TComponents...> &sn) const {
        sn.prep_search(MaxDepth);
        buf.clear();
        Zobrist hash = sn.template get<Zobrist>();
        while (contains(hash) && !sn.bottomed_out()) {
            const move::FatMove best_move = get(hash).second.best_move;
            sn.make_move(best_move);
            if (sn.is_non_stalemate_draw()) {
                break;
            }
            buf.push_back(best_move);
            hash = sn.template get<Zobrist>();
        }
        sn.unmake_all();
    }

    // Checks membership
    bool contains(const Zobrist idx) const { return get(idx).first == idx; }

    // Inserts result,
    // if depth of new entry is deeper than an existing entry.
    void insert(const Zobrist idx, const SearchResult result,
                const uint8_t depth_remaining) {
        // Do not overwrite results from a deeper search.
        if (contains(idx) &&
            // Entries stored depth as depth + 1.
            get(idx).second.depth_remaining > depth_remaining + 1) {
            return;
        }

        // Store the new result.
        get(idx) = {idx, TTValue{.value = result.value,
                                 .depth_remaining =
                                     static_cast<uint8_t>(depth_remaining + 1),
                                 .best_move = result.best_move}};
    };

    void clear() {
        m_entries.clear();
        m_entries.resize(m_size);
    }

    void resize(size_t n) {
        m_size = n;
        clear();
        m_entries.shrink_to_fit();
    }

    void resize_mb(size_t n) { return resize(n * kb * kb / sizeof(TTEntry)); }

   private:
    // Access helper.
    TTEntry &get(const Zobrist idx) {
        return m_entries[static_cast<size_t>(idx) % m_size];
    }

    // Access helper.
    const TTEntry &get(const Zobrist idx) const {
        return m_entries[static_cast<size_t>(idx) % m_size];
    }

    // TODO: make resizable
    static constexpr size_t kb = 1024;
    size_t m_size = kb * kb / sizeof(TTEntry);

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
    bool use_hash = false;
    bool hash_pruning = false;
    bool verbose = false;
};

template <eval::IncrementallyUpdateableEvaluator TEval, size_t MaxDepth,
          NegaMaxOptions Opts = {}>
class DLNegaMax {
   public:
    constexpr DLNegaMax(DefaultNode<TEval, MaxDepth> &node, TTable &ttable)

        : m_node(node), m_ttable(ttable) {};

    constexpr void set_depth(size_t depth) {
        assert(depth <= MaxDepth);
        m_node.get().prep_search(depth);

        m_stopped = false;
    }

    constexpr const DefaultNode<TEval, MaxDepth> &get_node() const {
        return m_node;
    }

    // Not protected from races.
    // Calling code should ensure set_depth/stop do not race.
    constexpr void stop() { m_stopped = true; }
    template <SearchType Type = SearchType::NORMAL>
    constexpr SearchResult search(
        const std::optional<std::chrono::time_point<std::chrono::steady_clock>>
            finish_time = std::nullopt,
        Bounds bounds = {}, const StatReporter *reporter = nullptr) {
        // Auto-stop
        if (finish_time && std::chrono::steady_clock::now() > finish_time) {
            stop();
        }

        // Early return
        if (m_stopped) {
            return {.type = SearchResult::LeafType::TIMEOUT};
        }

        // Check for repetition

        // This will waste time researching all moves if we hit
        // stalemate/checkmate at the fiftieth fullmove,
        // but this is unlikely.
        if (m_node.get().depth() > 0 &&
            m_node.get().template is_non_stalemate_draw<1>()) {
            if constexpr (Opts.verbose) {
                if (reporter) {
                    // reporter->debug_log("bruh");
                }
            }

            return {.type = SearchResult::LeafType::DRAW, .n_nodes = 1};
        }

        // Cutoff -> return value
        if (m_node.get().template bottomed_out<Type>()) {
            // Normal search -> quiesce
            if constexpr (Type == SearchType::NORMAL && Opts.quiesce) {
                return search<SearchType::QUIESCE>(finish_time, bounds);
            } else {
                return cutoff_result();
            }
        }

        // In quiescence only: check stand-pat score
        if constexpr (Type == SearchType::QUIESCE && Opts.quiescence_standpat) {
            if (!m_node.get().is_checked()) {
                const eval::centipawn_t standpat_score =
                    m_node.get().template get<TEval>().eval();
                bounds.alpha = std::max(bounds.alpha, standpat_score);
                if (standpat_score >= bounds.beta) {
                    return {
                        .value = IBValue(standpat_score, ABNodeType::CUT),
                        .type = SearchResult::LeafType::DEPTH_CUTOFF,
                        .best_move = {},
                        .n_nodes = 1,
                    };
                }
            }
        }

        // Get hash move
        const Zobrist hash = m_node.get().template get<Zobrist>();
        move::FatMove hash_move;

        if constexpr (Opts.use_hash) {
            const std::optional<TTable::TTValue> tt_value =
                m_ttable.get().at_opt(hash);
            hash_move =
                tt_value.transform([](auto val) { return val.best_move; })
                    .value_or({});

            if constexpr (Opts.hash_pruning) {
                // Has deeper value
                if (tt_value &&
                    tt_value->depth_remaining >=
                        m_node.get().template depth_remaining<Type>() + 1) {
                    // TODO: check for repettions first
                    // TODO: handle null moves from mates
                    if (tt_value->value.exact() ||
                        (tt_value->value.node_type() == ABNodeType::CUT &&
                         tt_value->value.eval() >= bounds.beta) ||
                        (tt_value->value.node_type() == ABNodeType::ALL &&
                         tt_value->value.eval() < bounds.alpha)) {
                        return {
                            .value = tt_value->value,
                            .type = SearchResult::LeafType::HASH_CUTOFF,
                            .best_move = tt_value->best_move,
                            .n_nodes = 1,
                        };
                    }
                }
            }
        }

        // Get children (in order)
        MoveBuffer &moves = search_moves<Type>();
        if constexpr (Opts.sort) {
            std::sort(moves.begin(), moves.end(),
                      [this, hash_move](const move::FatMove a,
                                        const move::FatMove b) {
                          const bool ret = DefaultOrdering(
                              m_node.get().get_astate(), hash_move)(a, b);
                          if constexpr (DEBUG()) {
                              assert(!(ret && DefaultOrdering(
                                                  m_node.get().get_astate(),
                                                  hash_move)(b, a)));
                              assert(!(DefaultOrdering(
                                  m_node.get().get_astate(), hash_move)(a, a)));
                              assert(!(DefaultOrdering(
                                  m_node.get().get_astate(), hash_move)(b, b)));
                          }
                          return ret;
                      });
        }

        // Recurse over children
        std::optional<SearchResult> best_move;
        size_t n_nodes = 1;

        // Recurse
        for (const move::FatMove m : moves) {
            // Early return from recursion
            if (m_stopped) {
                return {.type = SearchResult::LeafType::TIMEOUT};
            }

            // Check child
            if (m_node.get().make_move(m)) {
                const SearchResult child_result =
                    search<Type>(finish_time, {-bounds.beta, -bounds.alpha});

                if constexpr (Type == SearchType::QUIESCE) {
                    assert(move::is_capture(m.get_move().type()));
                }

                // Count nodes
                n_nodes += child_result.n_nodes;
                const IBValue child_value = -child_result.value;

                // Update best move if eval improves
                if (!(best_move.has_value()) ||
                    child_value > best_move->value) {
                    best_move = {.value = child_value,
                                 .type = child_result.type,
                                 .best_move = m};  // count nodes later
                }

                if constexpr (Opts.prune) {
                    if (child_value > IBValue(bounds.alpha, ABNodeType::PV)) {
                        bounds.alpha = child_value.eval();
                    }
                    if (child_value >= IBValue(bounds.beta, ABNodeType::PV)) {
                        // Pruned -> return lower bound
                        m_node.get().unmake_move();
                        best_move->value =
                            IBValue(best_move->value.eval(), ABNodeType::CUT);
                        break;
                    }
                }
            }
            m_node.get().unmake_move();
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
                m_node.get()
                    .get_astate()
                    .state
                    .copy_bitboard(
                        {.colour = m_node.get().get_astate().state.to_move,
                         .piece = board::Piece::KING})
                    .single_bitscan_forward();

            const bool checked = move::movegen::AllMoveGenerator::is_attacked(
                m_node.get().get_astate(), king_sq,
                m_node.get().get_astate().state.to_move);

            const SearchResult endgame_result = {
                .value = IBValue(checked ? -eval::max_eval : 0, ABNodeType::PV),
                .type = checked ? SearchResult::LeafType::CHECKMATE
                                : SearchResult::LeafType::STALEMATE,
                .n_nodes = n_nodes,
            };
            // TODO: insert these
            // But, handle the fact that we have null moves in the cache
            // now. m_ttable.get().insert(
            //     hash, endgame_result,
            //     m_node.get().template depth_remaining<Type>());
            return endgame_result;
        }

        best_move->n_nodes = n_nodes;

        m_ttable.get().insert(
            hash, best_move.value(),
            static_cast<uint8_t>(
                m_node.get().template depth_remaining<Type>()));
        return best_move.value();
    }

    // Extracts principal variation from the transposition table.
    void get_pv(MoveBuffer &buf) {
        return m_ttable.get().get_pv(buf, m_node.get());
    }

   private:
    // Return value in (soft/hard) cutoff
    constexpr SearchResult cutoff_result() const {
        return {.value = IBValue(m_node.get().template get<TEval>().eval(),
                                 ABNodeType::PV),
                .type = SearchResult::LeafType::DEPTH_CUTOFF,
                .n_nodes = 1};
    }

    // Gets moves to be searched based on search type.
    template <SearchType Type>
    constexpr MoveBuffer &search_moves();

    template <>
    constexpr MoveBuffer &search_moves<SearchType::NORMAL>() {
        return m_node.get().template find_moves<true>();
    }

    // Return sorted
    template <>
    constexpr MoveBuffer &search_moves<SearchType::QUIESCE>() {
        return m_node.get().find_loud_moves();
    }

    // Quiescence helper: if no loud moves were found,
    // check if this is due to checkmate/stalemate,
    // or if there are legal quiet moves.
    constexpr bool quiet_moves_exist() {
        // Get children (in order)
        const MoveBuffer &moves = m_node.get().find_quiet_moves();

        for (const move::FatMove &m : moves) {
            if (m_node.get().make_move(m)) {
                m_node.get().unmake_move();
                return true;
            }
            m_node.get().unmake_move();
        }

        return false;
    }

    // Helper for fifty-move rule: check a legal move exists.
    constexpr bool legal_moves_exist() {
        // Get children (in order)
        const MoveBuffer &moves = m_node.find_moves();

        for (const move::FatMove &m : moves) {
            if (m_node.make_move(m)) {
                m_node.unmake_move();
                return true;
            }
            m_node.unmake_move();
        }

        return false;
    }

    std::reference_wrapper<DefaultNode<TEval, MaxDepth>> m_node;
    std::reference_wrapper<TTable> m_ttable;

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
    template <typename... Ts>
    constexpr IDSearcher(Ts &&...args)
        : m_searcher(std::forward<Ts>(args)...) {}

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
        const std::optional<std::chrono::time_point<std::chrono::steady_clock>>
            finish_time,
        Bounds bounds = {}, const StatReporter *reporter = nullptr) {
        m_stoplock.lock();
        m_stopped = false;
        m_stoplock.unlock();

        std::optional<SearchResult> search_result = {};

        // Loop over all possible levels
        for (size_t max_depth = 1; max_depth <= m_depth && !m_stopped;
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

            const std::optional<
                std::chrono::time_point<std::chrono::steady_clock>>
                ply_finish_time =
                    (max_depth > 1) ? std::optional(finish_time) : std::nullopt;
            SearchResult candidate_result =
                m_searcher.search(ply_finish_time, bounds, reporter);

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

            m_searcher.get_pv(m_pv);

            const std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - start_time;

            // TODO: report nps for current iteration, not total?
            if (reporter) {
                reporter->report(max_depth, search_result->value.eval(),
                                 search_result->n_nodes, elapsed, m_pv);
            }

            if (search_result->type == SearchResult::LeafType::CHECKMATE) {
                break;
            }
        };

        assert(search_result.has_value());
        return search_result.value();
    };

    void set_depth(size_t depth) {
        assert(depth <= MaxDepth);
        m_depth = depth;
    };

   private:
    // Searcher should be shorter-lived than other objects.
    TSearcher m_searcher;
    size_t m_depth = MaxDepth;

    bool m_stopped = false;
    std::mutex m_stoplock;

    // For now, just report one move from pv
    // TODO: triangular table
    MoveBuffer m_pv;
};

static_assert(
    StoppableSearcher<IDSearcher<DLNegaMax<eval::DefaultEval, 1>, 1>>);
static_assert(DLSearcher<IDSearcher<DLNegaMax<eval::DefaultEval, 1>, 1>>);

}  // namespace search

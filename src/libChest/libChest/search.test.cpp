//============================================================================//
// Ensure search features which shouldn't change results do not.
//============================================================================//

#include "libChest/search.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <iostream>
#include <vector>

#include "libChest/eval.h"
#include "libChest/move.h"
#include "libChest/state.h"
#include "libChest/util.h"

constexpr size_t max_depth = 64;
#if DEBUG()
constexpr size_t search_depth = 6;
#else
constexpr size_t search_depth = 7;
#endif

using VanillaNegaMax = search::DLNegaMax<eval::DefaultEval, max_depth,
                                         {.prune = false,
                                          .sort = false,
                                          .quiesce = false,
                                          .quiescence_standpat = false,
                                          .hash_first = false}>;

using ABNegaMax = search::DLNegaMax<eval::DefaultEval, max_depth,
                                    {.prune = true,
                                     .sort = false,
                                     .quiesce = false,
                                     .quiescence_standpat = false,
                                     .hash_first = false}>;

using ABSorted = search::DLNegaMax<eval::DefaultEval, max_depth,
                                   {.prune = true,
                                    .sort = true,
                                    .quiesce = false,
                                    .quiescence_standpat = false,
                                    .hash_first = false}>;

using QSearch = search::DLNegaMax<eval::DefaultEval, max_depth,
                                  {.prune = true,
                                   .sort = false,
                                   .quiesce = true,
                                   .quiescence_standpat = false,
                                   .hash_first = false}>;

using QSearchSorted = search::DLNegaMax<eval::DefaultEval, max_depth,
                                        {.prune = true,
                                         .sort = true,
                                         .quiesce = true,
                                         .quiescence_standpat = false,
                                         .hash_first = false}>;

using QSearchStandPat = search::DLNegaMax<eval::DefaultEval, max_depth,
                                          {.prune = true,
                                           .sort = false,
                                           .quiesce = true,
                                           .quiescence_standpat = true,
                                           .hash_first = false}>;

using FullQSearch = search::DLNegaMax<eval::DefaultEval, max_depth,
                                      {.prune = true,
                                       .sort = true,
                                       .quiesce = true,
                                       .quiescence_standpat = true,
                                       .hash_first = false}>;

using FullQSearchWithHashMove = search::DLNegaMax<eval::DefaultEval, max_depth,
                                                  {.prune = true,
                                                   .sort = true,
                                                   .quiesce = true,
                                                   .quiescence_standpat = true,
                                                   .hash_first = true}>;

search::SearchResult do_search(auto &searcher, const size_t d,
                               const std::string_view name,
                               search::TTable &ttable) {
    searcher.set_depth(d);
    search::SearchResult ret = searcher.search();
    std::cerr << name << "\n  DEPTH: " << d << ", eval: " << ret.value.eval()
              << ", best_move: "
              << static_cast<std::string>(move::LongAlgMove(ret.best_move))
              << ", nodes: " << ret.n_nodes << '\n'
              << "  pv: ";
    MoveBuffer pv;
    searcher.get_pv(pv);
    for (auto mv : pv) {
        std::cerr << static_cast<std::string>(move::LongAlgMove(mv)) << ", ";
    }
    std::cerr << '\n';
    ttable.clear();
    return ret;
};

TEST_CASE("Equality of equivalent search results.") {
    std::vector<search::SearchResult> results;
    static search::TTable ttable;
    state::AugmentedState state{state::new_game_fen};

    search::DefaultNode<eval::DefaultEval, max_depth> sn(state, max_depth);

    std::tuple<VanillaNegaMax, ABNegaMax, ABSorted, QSearch, QSearchSorted,
               QSearchStandPat, FullQSearch, FullQSearchWithHashMove>

        searchers{{sn, ttable}, {sn, ttable}, {sn, ttable}, {sn, ttable},
                  {sn, ttable}, {sn, ttable}, {sn, ttable}, {sn, ttable}};

    for (size_t d = 1; d < search_depth; d++) {
        search::SearchResult vanilla_result = do_search(
            std::get<VanillaNegaMax>(searchers), d, "Minimax", ttable);
        search::SearchResult ab_result =
            do_search(std::get<ABNegaMax>(searchers), d, "Alpha-beta", ttable);
        search::SearchResult ab_sorted_result =
            do_search(std::get<ABSorted>(searchers), d,
                      "Alpha-beta (mvv-lva sorted)", ttable);
        search::SearchResult qsearch_result =
            do_search(std::get<QSearch>(searchers), d,
                      "Quiescence search (unsorted)", ttable);
        search::SearchResult qsearch_sorted_result =
            do_search(std::get<QSearchSorted>(searchers), d,
                      "Quiescence search (mvv-lva sorted)", ttable);
        search::SearchResult qsearch_stand_pat_result = do_search(
            std::get<QSearchStandPat>(searchers), d,
            "Quiescence search w/ stand-pat pruning (unsorted)", ttable);

        search::SearchResult qsearch_sorted_stand_pat_result = do_search(
            std::get<FullQSearch>(searchers), d,
            "Full quiescence search (stand-pat pruning, mvv-lva sorted)",
            ttable);
        search::SearchResult hash_move_result =
            do_search(std::get<FullQSearchWithHashMove>(searchers), d,
                      "Full quiescence search (stand-pat pruning, "
                      "hash-move/mvv-lva sorted)",
                      ttable);

        std::cerr << '\n';

        // Run test
        // NOLINTBEGIN(cppcoreguidelines-avoid-do-while)
        REQUIRE(vanilla_result.value.eval() == ab_result.value.eval());
        REQUIRE(ab_result.value.eval() == ab_sorted_result.value.eval());
        REQUIRE(qsearch_result.value.eval() ==
                qsearch_sorted_result.value.eval());
        REQUIRE(qsearch_sorted_stand_pat_result.value.eval() ==
                hash_move_result.value.eval());
        (void)qsearch_stand_pat_result;
        // NOLINTEND(cppcoreguidelines-avoid-do-while)
    }
}

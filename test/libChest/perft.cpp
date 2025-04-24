#include "../../src/libChest/makemove.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <iostream>

//
// Test all depths up to threshold per position
//

// Results as of 22/05/24:
//   InOrder = true -> 26.8Mn/s
//   InOrder = false -> 28.3Mn/s
// move generation logic decide the fastest way to get moves.
constexpr bool in_order = false;
constexpr size_t max_depth_limit = 6;

struct PerftTest {
    std::string name;
    state::fen_t fen;
    std::vector<uint64_t> results;
};

move::movegen::AllMoveGenerator<in_order> mover{};

const std::string indent = "    ";
const uint64_t million = 1000000;

struct AveragePerft {
    uint64_t nodes;
    double seconds;
};

static AveragePerft avg;

void do_perft_test(const PerftTest &perft_case, size_t depth_limit) {

    state::AugmentedState astate(state::State(perft_case.fen));
    std::cout << indent << "Testing position: " << perft_case.name << std::endl;

    for (size_t i = 0; i < perft_case.results.size() && i < depth_limit; i++) {

        std::cout << indent << indent << "perft(" << i << "): " << std::endl;

        state::SearchNode<move::movegen::AllMoveGenerator<in_order>,
                          max_depth_limit>
            sn(mover, astate, i);

        // Run perft
        auto start = std::chrono::steady_clock::now();
        state::SearchNode<move::movegen::AllMoveGenerator<in_order>,
                          max_depth_limit>::PerftResult res = sn.perft();
        auto end = std::chrono::steady_clock::now();

        // Timing info
        std::chrono::duration<double> taken = end - start;
        double mnps = res.nodes / taken.count() / million;
        std::cout << indent << indent << indent << "took: " << taken
                  << std::endl;
        std::cout << indent << indent << indent
                  << "legal nodes searched: " << res.nodes << std::endl;
        std::cout << indent << indent << indent << "rate: " << mnps << "Mn/s"
                  << std::endl;

        // Update average
        avg.nodes += res.nodes;
        avg.seconds += taken.count();

        // Run test
        REQUIRE(res.perft == perft_case.results.at(i));
    }
}

//
// Define the tests
//

PerftTest cases[] = {
    {"startpos/CPW position 1",
     state::new_game_fen,
     {

         1,
         20,
         400,
         8902,
         197281,
         4865609,
         119060324,
         3195901860,
         84998978956,
         2439530234167,
         69352859712417,
         2097651003696806,
         62854969236701747,
         1981066775000396239,
     }},
    {"Kiwipete/CPW position 2",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {

         1,
         48,
         2039,
         97862,
         4085603,
         193690690,
         8031647685,
     }},
    {"CPW position 3",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {

         1,
         14,
         191,
         2812,
         43238,
         674624,
         11030083,
         178633661,
         3009794393,
     }},
    {"CPW position 4",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {

         1,
         6,
         264,
         9467,
         422333,
         15833292,
         706045033,
     }},
    {"CPW position 4 (mirrored)",
     "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     {

         1,
         6,
         264,
         9467,
         422333,
         15833292,
         706045033,
     }},
    {"CPW position 5",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {
         1,
         44,
         1486,
         62379,
         2103487,
         89941194,
     }},
    {"CPW position 6",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     {1, 46, 2079, 89890, 3894594, 164075551, 6923051137, 287188994746,
      11923589843526, 490154852788714}},
    {"Rocechess 'Good testposition'",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {1, 48, 2039, 97862, 4085603, 193690690, 8031647685}},
    {"Rocechess 'Discover promotion bugs'",
     "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
     {1, 24, 496, 9483, 182838, 3605103, 71179139}}};

//
// Run
//

TEST_CASE("Perft tests") {
    for (auto &perft_case : cases) {
        do_perft_test(perft_case, max_depth_limit);
    };

    std::cout << indent << "TOTAL (LEGAL) NODES: " << avg.nodes / million
              << " million" << std::endl;
    std::cout << indent << "AVERAGE RATE: " << avg.nodes / avg.seconds / million
              << "Mn/s" << std::endl;
}

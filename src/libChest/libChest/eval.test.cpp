#include "../../src/libChest/eval.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <vector>

// Material eval

struct EvalCase {
    std::string name;
    state::fen_t fen;
    eval::centipawn_t expected;
};

std::vector<EvalCase> eval_cases{{"startpos", state::new_game_fen, 0},
                                 {"CPW position 3, modified",
                                  "q7/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                                  -900}};

TEST_CASE("Evaluation") {
    for (auto &eval_case : eval_cases) {
        std::cout << "Evaluating position: " << eval_case.name << std::endl;
        state::AugmentedState astate =
            state::AugmentedState{state::State(eval_case.fen)};
        eval::MaterialEval ev(astate);
        REQUIRE(ev.eval() == eval_case.expected);
    }
}

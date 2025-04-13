// #include "libChest/board.h"
// #include "libChest/jumping.h"
#include "libChest/board.h"
#include "libChest/magic.h"
#include "libChest/move.h"
#include "libChest/state.h"
#include <iostream>

using namespace board;
using namespace board::io;
using namespace std;
using namespace state;
using namespace move::magic;
// using namespace move::jumping;

int main(int argc, char **argv) {
    State s = State("p7/PP6/8/8/8/8/2P5/8 b - - 0 1");
    std::cout << s.total_occupancy().pretty()<< std::endl;;

    Magics m = Magics();
}

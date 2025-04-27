#include <iostream>
#include "enginestate.h"
#include <libChest/state.h>

//
// Main executable for the UCI engine.
//

int main() {

    std::cin.clear();

    Globals g;
    Engine eng(g);
    while (true) {
        while (!std::cin.peek()) {
        }
        eng.handle_command(eng.read_command());
    }
    return 0;
}

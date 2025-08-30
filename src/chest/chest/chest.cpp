#include "chest.h"
#include <iostream>
#include <libChest/state.h>

//
// Main executable for the UCI engine.
//

int main() {

    std::cin.clear();

    Globals g;
    Engine eng(g);
    while (true) {
        UciCommand cmd = eng.read_command();
        if (!cmd.input.empty()) {
            eng.handle_command(cmd);
        }
    }
    return 0;
}

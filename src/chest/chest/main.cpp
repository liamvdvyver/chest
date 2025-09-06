//============================================================================//
// Main executable for the UCI engine.
//============================================================================//

#include <iostream>
#include <optional>

#include "uci.h"

int main() {
    std::cin.clear();
    UCIEngine eng{};
    std::optional<int> rc{};
    for (; !rc.has_value(); rc = eng.run()) {
    }
    return (rc.value());
}

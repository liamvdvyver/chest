#include <iostream>
#include <optional>

#include "uci.h"

//
// Main executable for the UCI engine.
//

int main() {
    std::cin.clear();
    UCIEngine eng{};
    std::optional<int> rc{};
    for (; !rc.has_value(); rc = eng.run()) {
    }
    return (rc.value());
}

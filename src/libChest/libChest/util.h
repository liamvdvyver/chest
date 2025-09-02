#pragma once

#include <tuple>

// Calls a function for each element of a tuple.
auto apply_tuple(const auto &f, auto &t) {
    return std::apply([f]<typename... T>(T &...t) { (f(t), ...); }, t);
}

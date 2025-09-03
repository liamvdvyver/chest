#pragma once

#include <tuple>
#include <type_traits>

// Calls a function for each element of a tuple.
auto apply_tuple(const auto &f, auto &t) {
    return std::apply([f]<typename... T>(T &...t) { (f(t), ...); }, t);
}

// Does a tuple contain an element of a type?
// taken from:
// https://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type
template <typename T, typename Tuple>
struct tuple_has;

template <typename T, typename... Us>
struct tuple_has<T, std::tuple<Us...>>
    : std::disjunction<std::is_same<T, Us>...> {};

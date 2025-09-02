#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <vector>

#include "move.h"

// Stack/static/sized vectors.
//
// Push/pop is by value for now, mainly used for storing types smaller than a
// register No error handling, or anything like that, an std::array and a stack
// pointer is all I needed.

// R(andom)A(ccess)Stack
// Allows custom containers to be used as drop-in containers for std::vectors
// in search code which only uses this subset of std::vector's interface.
//
// Basically, a stack (push/pop back) with random access and reset.
template <typename Container, typename T>
concept RAStack =
    requires(Container c, T t, size_t i, std::initializer_list<T> init) {
        { c.back() } -> std::convertible_to<T>;
        // Enforces preference for .at() over [], even when using std::vector
        { c.at(i) } -> std::convertible_to<T>;
        { c.push_back(t) } -> std::same_as<void>;
        { c.pop_back() } -> std::same_as<void>;
        { c.clear() } -> std::same_as<void>;
        { c.begin() } -> std::forward_iterator<>;
        { c.end() } -> std::forward_iterator<>;
        { c.size() } -> std::same_as<size_t>;
        { c.resize(i) } -> std::same_as<void>;
    };
static_assert(RAStack<std::vector<int>, int>);

template <typename T, size_t N>
struct svec {
    constexpr T &back() { return data[sp - 1]; }
    constexpr void push_back(T val) {
        data[sp++] = val;
        check_sz();
    }
    constexpr void pop_back() {
        check_nonempty();
        sp--;
    }
    constexpr void clear() { sp = 0; }
    constexpr T &at(size_t i) { return data.at(i); }
    constexpr T const &at(size_t i) const { return data.at(i); }
    constexpr auto begin() { return data.begin(); }
    constexpr auto begin() const { return data.begin(); }
    constexpr auto end() { return &data.at(sp); }
    constexpr auto end() const { return &data.at(sp); }
    constexpr size_t size() { return sp; }
    constexpr void resize(size_t sz) {
        sp = sz;
        check_sz();
    }

   private:
    size_t sp = 0;  // pointer to element after last
    std::array<T, N> data;
    void check_sz() { assert(sp <= N); };
    void check_nonempty() { assert(sp); }
};
static_assert(RAStack<svec<int, 5>, int>);

// Size of move buffer
// Note: 218 is the limit of legal chess moves
// since we have psuedolegal moves, this could be insufficient,
// I just chose 256, then we can index in with a uint8_t.
constexpr size_t MaxMoves = 256;

// The type of buffer used for storing search results,
// must be compatible with interface of std::vector<move::Move>
using MoveBuffer = svec<move::FatMove, MaxMoves>;
static_assert(RAStack<MoveBuffer, move::FatMove>);

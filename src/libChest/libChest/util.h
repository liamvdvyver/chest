//============================================================================//
// Various utility classes
//============================================================================//

#pragma once

#include <array>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <vector>

#include "move.h"

//============================================================================//
// Debug macro
//============================================================================//

#ifdef NDEBUG
#define DEBUG() false
#else
#define DEBUG() true
#endif

//============================================================================//
// Stack/static/sized vectors
//============================================================================//

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
        { c[i] } -> std::convertible_to<T>;
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
struct SVec {
    constexpr T &back() { return m_data[m_sp - 1]; }
    constexpr void push_back(const T val) {
        m_data[m_sp++] = val;
        check_sz();
    }
    constexpr void pop_back() {
        check_nonempty();
        m_sp--;
    }
    constexpr void clear() { m_sp = 0; }
    constexpr T &operator[](const size_t i) { return m_data[i]; }
    constexpr T const &operator[](const size_t i) const { return m_data[i]; }
    constexpr T &at(const size_t i) { return m_data.at(i); }
    constexpr T const &at(const size_t i) const { return m_data.at(i); }
    constexpr auto begin() { return m_data.begin(); }
    constexpr auto begin() const { return m_data.begin(); }
    constexpr auto end() { return &m_data.at(m_sp); }
    constexpr auto end() const { return &m_data.at(m_sp); }
    constexpr size_t size() { return m_sp; }
    constexpr void resize(const size_t sz) {
        m_sp = sz;
        check_sz();
    }

   private:
    size_t m_sp = 0;  // pointer to element after last
    std::array<T, N> m_data;
    void check_sz() { assert(m_sp <= N); };
    void check_nonempty() { assert(m_sp); }
};

// Size of move buffer
// Note: 218 is the limit of legal chess moves
// since we have psuedolegal moves, this could be insufficient,
// I just chose 256, then we can index in with a uint8_t.
constexpr size_t max_moves = 256;

// The type of buffer used for storing search results,
// must be compatible with interface of std::vector<move::Move>
using MoveBuffer = SVec<move::FatMove, max_moves>;
static_assert(RAStack<MoveBuffer, move::FatMove>);

//============================================================================//
// Compile-time tuple iteration
//============================================================================//

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

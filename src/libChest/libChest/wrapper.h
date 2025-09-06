//============================================================================//
// Operator-forwarding struct wrappers for primitive types
//============================================================================//

#pragma once

// Wrap a base type with a single-member struct, providing:
// * private "value" field of base type
// * (explicit) conversion operator to base type or bool
// * one arg (value)/no-arg (default initialisation of value field) c'tors
// * overload operators of a wrapper based on the operators of a wrapper type
template <typename T, typename WrapperType>
struct Wrapper {
    // Make value available to derived WrapperType
    friend WrapperType;

    // Construct from value, or use default initiliaser
    constexpr Wrapper(const T value) : value{value} {};
    constexpr Wrapper() : value{} {};

    // Allow explicit conversion to get base type
    constexpr explicit operator T() const { return value; };
    constexpr explicit operator bool() const { return value; };

    // Equality
    constexpr friend bool operator==(const WrapperType &a,
                                     const WrapperType &b) {
        return a.value == b.value;
    }
    constexpr friend bool operator!=(const WrapperType &a,
                                     const WrapperType &b) {
        return a.value != b.value;
    }

    // Arithmetic
    constexpr friend WrapperType operator+(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value + b.value;
    }
    constexpr friend WrapperType operator-(const WrapperType &a) {
        return -a.value;
    }
    constexpr friend WrapperType operator-(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value - b.value;
    }
    constexpr friend WrapperType operator*(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value * b.value;
    }
    constexpr friend WrapperType operator/(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value / b.value;
    }
    constexpr friend WrapperType operator+=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value += b.value;
    }
    constexpr friend WrapperType operator-=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value -= b.value;
    }
    constexpr friend WrapperType operator*=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value *= b.value;
    }
    constexpr friend WrapperType operator/=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value /= b.value;
    }

    // Pre/post-increment/decrement
    constexpr friend WrapperType operator++(WrapperType &a) {
        return ++a.value;
    }
    constexpr friend WrapperType operator++(WrapperType &a, int b) {
        (void)b;
        return a.value++;
    }
    constexpr friend WrapperType operator--(WrapperType &a) {
        return --a.value;
    }
    constexpr friend WrapperType operator--(WrapperType &a, int b) {
        (void)b;
        return a.value--;
    }

    // Shift
    constexpr friend WrapperType operator<<(const WrapperType &a, int b) {
        return a.value << b;
    }
    constexpr friend WrapperType operator>>(const WrapperType &a, int b) {
        return a.value >> b;
    }
    constexpr friend WrapperType operator<<=(WrapperType &a, int b) {
        return a.value <<= b;
    }
    constexpr friend WrapperType operator>>=(WrapperType &a, int b) {
        return a.value >>= b;
    }

    // Logical
    constexpr friend WrapperType operator~(const WrapperType &a) {
        return ~a.value;
    }
    constexpr friend WrapperType operator!(const WrapperType &a) {
        return !a.value;
    }
    constexpr friend WrapperType operator&(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value & b.value;
    }
    constexpr friend WrapperType operator|(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value | b.value;
    }
    constexpr friend WrapperType operator^(const WrapperType &a,
                                           const WrapperType &b) {
        return a.value ^ b.value;
    }
    constexpr friend WrapperType operator&=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value &= b.value;
    }
    constexpr friend WrapperType operator|=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value |= b.value;
    }
    constexpr friend WrapperType operator^=(WrapperType &a,
                                            const WrapperType &b) {
        return a.value ^= b.value;
    }

   private:
    T value;
};

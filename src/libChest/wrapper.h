#ifndef WRAPPER_H
#define WRAPPER_H

// Wrap a base type with a single-member struct, providing:
//
// * private "value" field of base type
// * (implicit) conversion operator to base type is allowed
// * one arg (value)/no-arg (default initialisation of value field) c'tors
// * overload operators of a wrapper based on the operators of a wrapper type
//
// By inheriting from the Wrapper type, and adding a (constexpr) constructor for
// the derived WrapperType from the Wrapper type, the derived Wrapper type can
// be used (mostly) interchangeable with T.
template <typename T, typename WrapperType> struct Wrapper {

    // Make value available to derived WrapperType
    friend WrapperType;

    // Construct from value, or use default initiliaser
    constexpr Wrapper(const T board) : value{board} {};
    constexpr Wrapper() : value{} {};

    // Allow explicit conversion to get base type
    constexpr explicit operator T() { return value; };
    constexpr explicit operator bool() { return value; };

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
        return a.value++;
    }
    constexpr friend WrapperType operator--(WrapperType &a) {
        return --a.value;
    }
    constexpr friend WrapperType operator--(WrapperType &a, int b) {
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

#endif

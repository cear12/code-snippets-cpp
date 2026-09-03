// to_underlying: converts an enum (typically enum class) value to its
// underlying integer type -- standardized as std::to_underlying in C++23,
// commonly hand-rolled before that.
#include <iostream>
#include <type_traits>

template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

enum class Color : std::uint8_t { Red, Green, Blue };

int main() {
    Color c = Color::Green;

    // Color c2 = c + 1; // would not compile: enum class has no arithmetic
    auto raw = to_underlying(c);
    std::cout << "Color::Green as underlying type: " << static_cast<int>(raw) << "\n";

    return 0;
}

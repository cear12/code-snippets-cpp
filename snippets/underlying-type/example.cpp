// to_underlying: converts an enum (typically enum class) value to its
// underlying integer type -- standardized as std::to_underlying in C++23,
// commonly hand-rolled before that.
#include <iostream>
#include <type_traits>

template <typename E>
constexpr typename std::underlying_type<E>::type ToUnderlying(E e) noexcept {
  return static_cast<typename std::underlying_type<E>::type>(e);
}

enum class Color : std::uint8_t { kRed, kGreen, kBlue };

int main() {
  Color c = Color::kGreen;

  // Color c2 = c + 1; // would not compile: enum class has no arithmetic
  auto raw = ToUnderlying(c);
  std::cout << "Color::Green as underlying type: " << static_cast<int>(raw)
            << "\n";

  return 0;
}

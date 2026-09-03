// std::forward, reimplemented (as demo::forward, to avoid redefining the
// real std::forward -- see README.md) purely to show the two-overload,
// reference-collapsing mechanism that makes perfect forwarding work.
#include <iostream>
#include <type_traits>
#include <utility>

namespace demo {

template <typename T>
constexpr T &&Forward(typename std::remove_reference<T>::type &arg) noexcept {
  return static_cast<T &&>(arg);
}

template <typename T>
constexpr T &&Forward(typename std::remove_reference<T>::type &&arg) noexcept {
  static_assert(!std::is_lvalue_reference<T>::value,
                "template argument substituting T is an lvalue reference type");
  return static_cast<T &&>(arg);
}

} // namespace demo

// Overloaded on value category; relay() below calls the right one purely
// because demo::forward<T>(arg) hands back the original value category.
void inner(int &) { std::cout << "inner(int&)  -- called with an lvalue\n"; }
void inner(int &&) { std::cout << "inner(int&&) -- called with an rvalue\n"; }

template <typename T> void Relay(T &&arg) { inner(demo::Forward<T>(arg)); }

int main() {
  int x = 5;
  Relay(x);            // x is an lvalue -> inner(int&)
  Relay(10);           // 10 is an rvalue -> inner(int&&)
  Relay(std::move(x)); // explicitly an rvalue -> inner(int&&)
  return 0;
}

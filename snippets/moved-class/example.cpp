// Deriving all special member functions from just default-ctor,
// copy-ctor, and move-assignment idiom: move-ctor delegates to
// default-ctor + move-assign; copy-assign delegates to copy-ctor +
// move-assign.
#include <iostream>
#include <utility>

class T {
public:
  T() { std::cout << "default ctor\n"; }

  T(const T &) { std::cout << "copy ctor\n"; }

  T &operator=(T &&) noexcept {
    std::cout << "move assign\n";
    return *this;
  }

  // Move ctor: default-construct, then move-assign -- derived from the
  // two members above rather than implemented directly.
  T(T &&t) noexcept : T() {
    std::cout << "move ctor (delegates to default ctor + move assign)\n";
    *this = std::move(t);
  }

  // Copy assign: copy-construct a temporary, then move-assign it in --
  // also derived rather than implemented directly.
  T &operator=(const T &t) {
    std::cout << "copy assign (delegates to copy ctor + move assign)\n";
    return *this = T(t);
  }
};

int main() {
  std::cout << "-- T a; --\n";
  T a;

  std::cout << "-- T b(a); (copy ctor) --\n";
  T b(a);

  std::cout << "-- T c(std::move(a)); (move ctor) --\n";
  T c(std::move(a));

  std::cout << "-- b = c; (copy assign) --\n";
  b = c;

  std::cout << "-- b = std::move(c); (move assign) --\n";
  b = std::move(c);

  return 0;
}

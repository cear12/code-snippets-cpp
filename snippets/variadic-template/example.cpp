// Variadic Templates idiom: two independent techniques for a variadic
// print(), namespaced apart since (as the original notes point out) they
// are not meant to overload against each other in the same scope.
#include <iostream>

namespace with_base_case {

// Termination (base) case: zero arguments left.
inline void Print() {}

template <typename Head, typename... Tail>
void Print(const Head &head, const Tail &...tail) {
  std::cout << head << "\n";
  Print(tail...);
}

} // namespace with_base_case

namespace fold_expression {

// No termination overload needed: the fold expression expands the whole
// pack directly.
template <typename... Args> void Print(Args &&...args) {
  ((std::cout << args << "\n"), ...); // C++17 unary right fold
}

} // namespace fold_expression

int main() {
  std::cout << "-- with_base_case::print --\n";
  with_base_case::Print(1, "two", 3.0);

  std::cout << "-- fold_expression::print --\n";
  fold_expression::Print(1, "two", 3.0);

  return 0;
}

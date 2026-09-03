// Fast C++ Stream I/O idiom: disable sync with C stdio and untie cin from
// cout, both one-time setup steps that meaningfully speed up large
// amounts of console I/O (common in competitive programming).
#include <iostream>

static const auto kFastIo = []() {
  std::ios_base::sync_with_stdio(false); // ios_base: applies process-wide
  std::cin.tie(nullptr);                 // ios: applies to this stream only
  return 0;
}();

int main() {
  std::cout << "fast I/O configured before main() started\n";

  for (int i = 0; i < 5; ++i) {
    std::cout << "line " << i << "\n";
  }

  return 0;
}

// BOOST_SP_USE_QUICK_ALLOCATOR idiom: must be defined before including any
// Boost smart pointer header. Requires Boost (not compiled locally in
// this portfolio -- see README.md; verified by CI instead).
#define BOOST_SP_USE_QUICK_ALLOCATOR

#include <boost/shared_ptr.hpp>
#include <chrono>
#include <iostream>

int main() {
  boost::shared_ptr<int> p;

  constexpr int kIterations = 1'000'000;
  auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < kIterations; ++i) {
    p.reset(new int{i});
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  std::cout << kIterations << " shared_ptr resets took " << elapsed.count()
            << " ms"
            << " (using the quick allocator for control blocks)\n";

  return 0;
}

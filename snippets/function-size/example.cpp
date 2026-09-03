// Estimating a function's code size via pointer subtraction: a
// non-portable hack that happens to often work with optimizations
// disabled on some toolchains. See README.md -- do not rely on this.
#include <cstdint>
#include <iostream>

namespace function_size_hack {

int TestProc() { return 1; }

void SizeMarkerProc() {}

std::size_t EstimatedTestProcSize() {
  return static_cast<std::size_t>(
      reinterpret_cast<std::uintptr_t>(
          reinterpret_cast<void *>(SizeMarkerProc)) -
      reinterpret_cast<std::uintptr_t>(reinterpret_cast<void *>(TestProc)));
}

} // namespace function_size_hack

int main() {
  std::cout << "estimated function size (bytes, unreliable): "
            << function_size_hack::EstimatedTestProcSize() << "\n";
  return 0;
}

// Estimating a function's code size via pointer subtraction: a
// non-portable hack that happens to often work with optimizations
// disabled on some toolchains. See README.md -- do not rely on this.
#include <cstdint>
#include <iostream>

namespace function_size_hack {

int test_proc() {
    return 1;
}

void size_marker_proc() {}

std::size_t estimated_test_proc_size() {
    return static_cast<std::size_t>(
        reinterpret_cast<std::uintptr_t>(reinterpret_cast<void*>(size_marker_proc)) -
        reinterpret_cast<std::uintptr_t>(reinterpret_cast<void*>(test_proc)));
}

} // namespace function_size_hack

int main() {
    std::cout << "estimated function size (bytes, unreliable): "
              << function_size_hack::estimated_test_proc_size() << "\n";
    return 0;
}

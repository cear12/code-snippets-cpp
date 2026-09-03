// IIFE (Immediately-Invoked Function Expression) idiom: a lambda that's
// both defined and called in the same expression, used to initialize a
// const variable from logic that needs more than one expression, without
// leaking helper variables into the enclosing scope.
#include <iostream>

int main() {
    constexpr int kA = 12;
    constexpr int kB = 24;

    // Without an IIFE, computing this would need a non-const variable
    // (assigned across branches) or a separate named helper function used
    // exactly once.
    const int kResult = [] {
        if (kA > kB) {
            return 36 * (kA - kB);
        }
        return (kB > 345) ? (kA + 3 + kB * 35) : 77;
    }();

    std::cout << "result = " << kResult << "\n";
    return 0;
}

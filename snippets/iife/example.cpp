// IIFE (Immediately-Invoked Function Expression) idiom: a lambda that's
// both defined and called in the same expression, used to initialize a
// const variable from logic that needs more than one expression, without
// leaking helper variables into the enclosing scope.
#include <iostream>

int main() {
    constexpr int a = 12;
    constexpr int b = 24;

    // Without an IIFE, computing this would need a non-const variable
    // (assigned across branches) or a separate named helper function used
    // exactly once.
    const int result = [] {
        if (a > b) {
            return 36 * (a - b);
        }
        return (b > 345) ? (a + 3 + b * 35) : 77;
    }();

    std::cout << "result = " << result << "\n";
    return 0;
}

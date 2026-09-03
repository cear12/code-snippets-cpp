// Variadic to_string: folds operator<< over an arbitrary argument pack
// into a single std::ostringstream, then extracts the result.
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

template <typename... Ts>
std::string ToString(Ts&&... ts) {
    std::ostringstream oss;
    (oss << ... << std::forward<Ts>(ts)); // C++17 fold expression
    return oss.str();
}

int main() {
    std::string s = ToString("x=", 42, ", y=", 3.14, ", ok=", true);
    std::cout << s << "\n";
    return 0;
}

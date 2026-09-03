// SFINAE idiom: three independent techniques for constraining
// printContainer to only accept types with begin()/end(), each kept in
// its own namespace.
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================
// 1. Hand-written EnableIf (the pre-<type_traits> shape of std::enable_if)
// ============================================================
namespace enable_if_style {

template <bool Condition, typename T = void>
struct EnableIf {};

template <typename T>
struct EnableIf<true, T> {
    using Type = T;
};

template <typename T, typename U>
struct IsSame {
    static constexpr bool kValue = false;
};
template <typename T>
struct IsSame<T, T> {
    static constexpr bool kValue = true;
};

template <typename T>
typename EnableIf<!IsSame<T, int>::kValue, void>::Type PrintContainer(const T& container) {
    std::cout << "Values: { ";
    for (const auto& value : container) std::cout << value << " ";
    std::cout << "}\n";
}

} // namespace enable_if_style

// ============================================================
// 2. Expression-validity SFINAE via trailing decltype
// ============================================================
namespace expression_validity_style {

using std::begin;
using std::end;

template <typename T>
auto PrintContainer(const T& container)
    -> decltype(begin(container), end(container), void()) {
    std::cout << "Values: { ";
    for (const auto& value : container) std::cout << value << " ";
    std::cout << "}\n";
}

} // namespace expression_validity_style

// ============================================================
// 3. if constexpr (C++17): one function, branching internally
// ============================================================
namespace if_constexpr_style {

template <typename T>
void PrintContainer(const T& value) {
    if constexpr (std::is_scalar_v<T>) {
        std::cout << "Scalar: " << value << "\n";
    } else {
        std::cout << "Values: { ";
        for (const auto& v : value) std::cout << v << " ";
        std::cout << "}\n";
    }
}

} // namespace if_constexpr_style

int main() {
    std::vector<int> v{1, 2, 3};

    enable_if_style::PrintContainer(v);
    expression_validity_style::PrintContainer(v);

    if_constexpr_style::PrintContainer(v);
    if_constexpr_style::PrintContainer(42); // the scalar branch, same function template

    return 0;
}

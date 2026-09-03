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

template <bool Condition, typename Type = void>
struct EnableIf {};

template <typename Type>
struct EnableIf<true, Type> {
    using type = Type;
};

template <typename T, typename U>
struct IsSame {
    static constexpr bool value = false;
};
template <typename T>
struct IsSame<T, T> {
    static constexpr bool value = true;
};

template <typename T>
typename EnableIf<!IsSame<T, int>::value, void>::type printContainer(const T& container) {
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
auto printContainer(const T& container)
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
void printContainer(const T& value) {
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

    enable_if_style::printContainer(v);
    expression_validity_style::printContainer(v);

    if_constexpr_style::printContainer(v);
    if_constexpr_style::printContainer(42); // the scalar branch, same function template

    return 0;
}

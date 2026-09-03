// A Generic Logging Decorator for Callables idiom: DebugDecorator wraps
// any callable, logging before the call and the result after, working
// identically for a free function, a functor, or a lambda.
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>

template <typename Callable>
class DebugDecorator {
public:
    DebugDecorator(const Callable& c, const char* label) : c_(c), label_(label) {}

    template <typename... Args>
    auto operator()(Args&&... args) const {
        std::cout << "Calling " << label_ << "\n";
        auto result = c_(std::forward<Args>(args)...);
        std::cout << "Result: " << result << "\n";
        return result;
    }

private:
    const Callable& c_;
    std::string label_;
};

template <typename Callable>
auto decorate_debug(const Callable& c, const char* label) {
    return DebugDecorator<Callable>(c, label);
}

int subtract(int a, int b) {
    return a - b;
}

struct RandomRatio {
    double operator()() const {
        return double(std::rand() + 1) / double(std::rand() + 1);
    }
};

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    auto decoratedSubtract = decorate_debug(subtract, "subtract()");
    decoratedSubtract(5, 2);

    RandomRatio randomRatio;
    auto decoratedRatio = decorate_debug(randomRatio, "rand/rand");
    decoratedRatio();
    decoratedRatio();

    auto decoratedLambda = decorate_debug([](int t, int j) { return t + j; }, "t+j");
    decoratedLambda(5, 3);

    return 0;
}

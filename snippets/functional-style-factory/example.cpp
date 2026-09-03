// Functional-Style Factory idiom: Factory is a std::function type, not a
// class hierarchy -- Client depends only on that call signature, and any
// callable (here, a lambda) can serve as a factory.
#include <functional>
#include <iostream>
#include <memory>

struct IProduct {
    virtual void foo() = 0;
    virtual ~IProduct() = default;
};

struct ConcreteProduct : IProduct {
    void foo() override {
        std::cout << "ConcreteProduct created and used\n";
    }
};

using Factory = std::function<std::unique_ptr<IProduct>()>;

void Client(const Factory& makeProduct) {
    auto product = makeProduct();
    product->foo();
}

int main() {
    Client([] { return std::make_unique<ConcreteProduct>(); });
    return 0;
}

// Overload Resolution via Multiple Conversion Operators idiom: Func
// converts to std::string, unsigned, or Foo depending on the context each
// use site requires.
#include <iostream>
#include <string>

struct Foo {
    int a = 10;
    int b = 20;
};

struct Func {
    operator std::string() const { return "10"; }
    operator unsigned() const { return 2; }
    operator Foo() const { return {100, 200}; }
};

int main() {
    std::cout << static_cast<std::string>(Func()) << "\n"; // context: std::string
    std::cout << static_cast<unsigned>(Func()) << "\n";     // context: unsigned

    auto val = Foo(Func()).a; // context: Foo
    std::cout << val << "\n";

    return 0;
}

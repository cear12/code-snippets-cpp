// Builder idiom: Foo::Builder accumulates values via chained setters and
// constructs the actual (here, immutable-after-construction) Foo only in
// build().
#include <iostream>
#include <vector>

class Foo {
public:
    class Builder;

    Foo(int prop1, bool prop2, bool prop3, std::vector<int> prop4)
        : prop1(prop1), prop2(prop2), prop3(prop3), prop4(std::move(prop4)) {}

    int prop1;
    bool prop2;
    bool prop3;
    std::vector<int> prop4;
};

class Foo::Builder {
public:
    Builder& set_prop1(int value) {
        prop1_ = value;
        return *this;
    }
    Builder& set_prop2(bool value) {
        prop2_ = value;
        return *this;
    }
    Builder& set_prop3(bool value) {
        prop3_ = value;
        return *this;
    }
    Builder& set_prop4(std::vector<int> value) {
        prop4_ = std::move(value);
        return *this;
    }

    Foo build() const { return Foo{prop1_, prop2_, prop3_, prop4_}; }

private:
    int prop1_ = 0;
    bool prop2_ = false;
    bool prop3_ = false;
    std::vector<int> prop4_ = {};
};

int main() {
    Foo f = Foo::Builder{}
                 .set_prop1(5)
                 .set_prop3(true)
                 .set_prop4({1, 2, 3})
                 .build();

    std::cout << "prop1=" << f.prop1 << " prop2=" << std::boolalpha << f.prop2
              << " prop3=" << f.prop3 << " prop4.size()=" << f.prop4.size() << "\n";

    return 0;
}

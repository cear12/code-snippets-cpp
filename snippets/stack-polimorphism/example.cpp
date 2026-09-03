// "Voldemort Types" idiom: VoldeFoo and Voldemort are local classes with
// no name any caller can spell, returned from lambdas via `auto` and used
// through `auto`/`decltype` without either side naming the type.
#include <iostream>

struct IFoo { // an ordinary, nameable interface
    virtual int getValue() = 0;
    virtual ~IFoo() = default;
};

int main() {
    // A local type with no interface, returned via a lambda's auto
    // return type.
    auto createVoldemortType = [] {
        struct Voldemort { // locally defined, unnameable outside this lambda
            int getValue() { return 21; }
        };
        return Voldemort{};
    };

    auto unnameable = createVoldemortType();       // must use auto -- Voldemort has no spellable name
    decltype(unnameable) unnameable2;               // but decltype can still name *this* type

    std::cout << "unnameable.getValue() + unnameable2.getValue() = "
              << (unnameable.getValue() + unnameable2.getValue()) << "\n"; // 21 + 21 = 42

    // A local type CAN derive from a normal, nameable interface, so
    // callers that only need IFoo's API don't need the concrete type's
    // name at all.
    auto fooFactory = [] {
        struct VoldeFoo : IFoo {
            int getValue() override { return 42; }
        };
        return VoldeFoo{};
    };

    auto foo = fooFactory();
    std::cout << "foo.getValue() = " << foo.getValue() << "\n";

    return 0;
}

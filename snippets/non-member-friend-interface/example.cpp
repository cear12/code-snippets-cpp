// Non-Member Interface Function idiom: non_member(obj) is part of foo's
// public interface even though it isn't a member, and ADL finds it
// without needing to qualify it with the namespace name.
#include <iostream>

namespace interface_ns {

class Foo {
public:
    void Member() const {
        std::cout << "Foo::member() (an ordinary member function)\n";
    }
};

// Not a member, but still part of Foo's interface: it only uses Foo's
// public API, and lives in the same namespace so ADL finds it.
void NonMember(const Foo& obj) {
    std::cout << "non_member(Foo) -- found via ADL, not a member function\n";
    obj.Member();
}

} // namespace interface_ns

int main() {
    interface_ns::Foo obj;
    NonMember(obj); // no "interface_ns::" needed: ADL finds it from obj's type
    return 0;
}

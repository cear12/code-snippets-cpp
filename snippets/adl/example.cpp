// Argument-Dependent Lookup (ADL / Koenig lookup) idiom: func(foo) finds
// ns::func without qualification, because ADL also searches the
// namespace(s) associated with foo's type.
#include <iostream>

namespace adl_demo {

struct Foo {};

void Func(Foo) {
    std::cout << "adl_demo::func(Foo) called via ADL\n";
}

} // namespace adl_demo

int main() {
    adl_demo::Foo foo;
    Func(foo); // no "adl_demo::" needed: ADL finds it from foo's type
    return 0;
}

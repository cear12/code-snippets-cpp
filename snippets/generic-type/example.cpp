// Type-as-Value Tag Dispatch idiom: GenericType<TypeA> and
// GenericType<TypeB> are distinct (empty) types, so print() is selected
// by overload resolution at compile time rather than by a runtime branch.
#include <iostream>

enum Type { TypeA, TypeB };

template <int>
struct GenericType final {};

static GenericType<TypeA> genericTypeA;
static GenericType<TypeB> genericTypeB;

void print(GenericType<TypeA>) {
    std::cout << "TypeA\n";
}

void print(GenericType<TypeB>) {
    std::cout << "TypeB\n";
}

int main() {
    // style a: construct a temporary of the tag type
    print(GenericType<TypeA>{});
    print(GenericType<TypeB>{});

    // style b: reuse a pre-existing tag instance
    print(genericTypeA);
    print(genericTypeB);

    return 0;
}

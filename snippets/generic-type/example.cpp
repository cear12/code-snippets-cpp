// Type-as-Value Tag Dispatch idiom: GenericType<TypeA> and
// GenericType<TypeB> are distinct (empty) types, so print() is selected
// by overload resolution at compile time rather than by a runtime branch.
#include <iostream>

enum Type { kTypeA, kTypeB };

template <int> struct GenericType final {};

static GenericType<kTypeA> g_generic_type_a;
static GenericType<kTypeB> g_generic_type_b;

void Print(GenericType<kTypeA>) { std::cout << "TypeA\n"; }

void Print(GenericType<kTypeB>) { std::cout << "TypeB\n"; }

int main() {
  // style a: construct a temporary of the tag type
  Print(GenericType<kTypeA>{});
  Print(GenericType<kTypeB>{});

  // style b: reuse a pre-existing tag instance
  Print(g_generic_type_a);
  Print(g_generic_type_b);

  return 0;
}

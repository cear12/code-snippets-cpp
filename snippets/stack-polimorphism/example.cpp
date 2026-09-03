// "Voldemort Types" idiom: VoldeFoo and Voldemort are local classes with
// no name any caller can spell, returned from lambdas via `auto` and used
// through `auto`/`decltype` without either side naming the type.
#include <iostream>

struct IFoo { // an ordinary, nameable interface
  virtual int GetValue() = 0;
  virtual ~IFoo() = default;
};

int main() {
  // A local type with no interface, returned via a lambda's auto
  // return type.
  auto create_voldemort_type = [] {
    struct Voldemort { // locally defined, unnameable outside this lambda
      int GetValue() { return 21; }
    };
    return Voldemort{};
  };

  auto unnameable = create_voldemort_type(); // must use auto -- Voldemort has
                                             // no spellable name
  decltype(unnameable) unnameable2; // but decltype can still name *this* type

  std::cout << "unnameable.getValue() + unnameable2.getValue() = "
            << (unnameable.GetValue() + unnameable2.GetValue())
            << "\n"; // 21 + 21 = 42

  // A local type CAN derive from a normal, nameable interface, so
  // callers that only need IFoo's API don't need the concrete type's
  // name at all.
  auto foo_factory = [] {
    struct VoldeFoo : IFoo {
      int GetValue() override { return 42; }
    };
    return VoldeFoo{};
  };

  auto foo = foo_factory();
  std::cout << "foo.getValue() = " << foo.GetValue() << "\n";

  return 0;
}

// Manual RTTI via per-type accessors ("asX()") idiom: Item::asItemA()
// defaults to nullptr; only the real ItemA overrides it to return `this`.
#include <iostream>

struct ItemB; // only ever used as an opaque pointer target here
struct ItemC;

struct Item {
  virtual ~Item() = default;
  virtual struct ItemA *AsItemA() { return nullptr; }
  virtual ItemB *AsItemB() { return nullptr; }
  virtual ItemC *AsItemC() { return nullptr; }
};

struct ItemA : Item {
  ItemA *AsItemA() override { return this; }
};

struct SomeOtherItem : Item {
  // Doesn't override any asX(): every accessor correctly returns nullptr.
};

int main() {
  Item *item = new ItemA();
  if (item->AsItemA()) {
    std::cout << "real ItemA\n";
  } else {
    std::cout << "not real ItemA\n";
  }
  delete item;

  Item *other = new SomeOtherItem();
  std::cout << (other->AsItemA() ? "real ItemA\n" : "not real ItemA\n");
  delete other;

  return 0;
}

// Manual RTTI via per-type accessors ("asX()") idiom: Item::asItemA()
// defaults to nullptr; only the real ItemA overrides it to return `this`.
#include <iostream>

struct ItemB; // only ever used as an opaque pointer target here
struct ItemC;

struct Item {
    virtual ~Item() = default;
    virtual struct ItemA* asItemA() { return nullptr; }
    virtual ItemB* asItemB() { return nullptr; }
    virtual ItemC* asItemC() { return nullptr; }
};

struct ItemA : Item {
    ItemA* asItemA() override { return this; }
};

struct SomeOtherItem : Item {
    // Doesn't override any asX(): every accessor correctly returns nullptr.
};

int main() {
    Item* item = new ItemA();
    if (item->asItemA()) {
        std::cout << "real ItemA\n";
    } else {
        std::cout << "not real ItemA\n";
    }
    delete item;

    Item* other = new SomeOtherItem();
    std::cout << (other->asItemA() ? "real ItemA\n" : "not real ItemA\n");
    delete other;

    return 0;
}

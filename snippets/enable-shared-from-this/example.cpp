// enable_shared_from_this idiom: Good safely shares the shared_ptr that
// already owns it; Bad's std::shared_ptr<Bad>(this) creates a second,
// independent control block for the same object -- a double-free once
// both eventually run their destructors.
#include <iostream>
#include <memory>

struct Good : std::enable_shared_from_this<Good> {
  std::shared_ptr<Good> CreateAnotherHandle() {
    return shared_from_this(); // shares the SAME control block
  }
};

struct Bad {
  std::shared_ptr<Bad> CreateAnotherHandle() {
    return std::shared_ptr<Bad>(this); // a NEW, independent control block
  }
};

int main() {
  // Good: both handles share one control block, refcount reflects reality.
  std::shared_ptr<Good> g1(new Good);
  std::shared_ptr<Good> g2 = g1->CreateAnotherHandle();
  std::cout << "Good: g1.use_count() = " << g1.use_count()
            << ", g2.use_count() = " << g2.use_count()
            << " (both should agree: 2)\n";

  // Bad: two independent control blocks for the same object.
  std::shared_ptr<Bad> b1(new Bad);
  std::shared_ptr<Bad> *b2 =
      new std::shared_ptr<Bad>(b1->CreateAnotherHandle());
  std::cout
      << "Bad: b1.use_count() = " << b1.use_count()
      << ", b2->use_count() = " << b2->use_count()
      << " (both report 1: neither control block knows about the other)\n";
  // NOTE: (*b2)->use_count() above actually calls use_count() through
  // Bad's shared_ptr, showing 1; deliberately NOT calling `delete b2` or
  // letting a local shared_ptr<Bad> for it go out of scope here -- doing
  // so alongside b1's own destruction would double-free the same Bad
  // object. b2 is intentionally leaked so this demo is safe to run; b1
  // alone destroys the object exactly once, normally, when main() returns.

  return 0;
}

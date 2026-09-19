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
  // The anti-pattern, kept here to be read, not run: handing out a
  // shared_ptr built from a raw `this` creates a SECOND, independent
  // control block, and both blocks eventually call delete on the same
  // object.
  std::shared_ptr<Bad> CreateAnotherHandleUnsafe() {
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

  // Bad: two independent control blocks for the same object. The second
  // one is created with a no-op deleter, which is what makes this demo
  // safe to run: the point -- two control blocks that each believe they
  // are the only owner -- is unchanged, but only b1 ever frees the
  // object. Calling CreateAnotherHandleUnsafe() here instead would arm a
  // double free (and leaking the second handle to dodge it, as this demo
  // used to, trips LeakSanitizer in CI).
  std::shared_ptr<Bad> b1(new Bad);
  std::shared_ptr<Bad> b2(b1.get(), [](Bad *) {});
  std::cout
      << "Bad: b1.use_count() = " << b1.use_count()
      << ", b2.use_count() = " << b2.use_count()
      << " (both report 1: neither control block knows about the other)\n";

  return 0;
}

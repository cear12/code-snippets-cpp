// Aliasing shared_ptr: py shares px's ownership/refcount but points at
// px's subobject y, not at the X object itself.
#include <iostream>
#include <memory>

struct Y {
  int value_ = 7;
};

struct X {
  Y y_;
  explicit X(int v) : y_{v} {}
};

int main() {
  std::shared_ptr<X> px(std::make_shared<X>(42));
  std::shared_ptr<Y> py(px, &px->y_); // aliasing constructor

  std::cout << "py->value = " << py->value_ << "\n";
  std::cout << "px.use_count() = " << px.use_count()
            << "\n"; // 2: px and py share ownership
  std::cout << "py.use_count() = " << py.use_count()
            << "\n"; // 2, same control block

  px.reset(); // the X object is NOT destroyed yet: py still owns a share of it
  std::cout << "after px.reset(): py->value = " << py->value_
            << ", py.use_count() = " << py.use_count() << "\n";

  return 0;
}

// Builder idiom: Foo::Builder accumulates values via chained setters and
// constructs the actual (here, immutable-after-construction) Foo only in
// build().
#include <iostream>
#include <vector>

class Foo {
public:
  class Builder;

  Foo(int prop1, bool prop2, bool prop3, std::vector<int> prop4)
      : prop1_(prop1), prop2_(prop2), prop3_(prop3), prop4_(std::move(prop4)) {}

  int prop1_;
  bool prop2_;
  bool prop3_;
  std::vector<int> prop4_;
};

class Foo::Builder {
public:
  Builder &SetProp1(int value) {
    prop1_ = value;
    return *this;
  }
  Builder &SetProp2(bool value) {
    prop2_ = value;
    return *this;
  }
  Builder &SetProp3(bool value) {
    prop3_ = value;
    return *this;
  }
  Builder &SetProp4(std::vector<int> value) {
    prop4_ = std::move(value);
    return *this;
  }

  Foo Build() const { return Foo{prop1_, prop2_, prop3_, prop4_}; }

private:
  int prop1_ = 0;
  bool prop2_ = false;
  bool prop3_ = false;
  std::vector<int> prop4_ = {};
};

int main() {
  Foo f = Foo::Builder{}.SetProp1(5).SetProp3(true).SetProp4({1, 2, 3}).Build();

  std::cout << "prop1=" << f.prop1_ << " prop2=" << std::boolalpha << f.prop2_
            << " prop3=" << f.prop3_ << " prop4.size()=" << f.prop4_.size()
            << "\n";

  return 0;
}

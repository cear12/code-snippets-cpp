// Universal Pointer idiom (always-owning variant): every constructor
// results in std::shared_ptr<T> owning *something* -- a copy (from a
// reference), the object itself (from a raw pointer), or a transferred
// unique_ptr. Contrast with idioms-cpp's universal-ptr, which adds a
// genuinely non-owning/observing mode via a no-op deleter.
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

template <typename T> struct UniversalPtr {
  std::shared_ptr<T> ptr_;

  constexpr UniversalPtr() noexcept = default;
  constexpr UniversalPtr(std::nullptr_t) noexcept {}

  // From a reference: copies U into a new, owned object.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  UniversalPtr(const U &r) : ptr_(std::make_shared<U>(r)) {}

  // From a raw pointer: adopts it -- ptr will delete it later.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  UniversalPtr(U *raw_ptr) : ptr_(raw_ptr) {}

  // From a unique_ptr: ownership transfers to ptr.
  template <typename U, typename D,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  UniversalPtr(std::unique_ptr<U, D> &&uptr) : ptr_(std::move(uptr)) {}

  T *Get() const noexcept { return ptr_.get(); }
  T &operator*() const { return *ptr_; }
  T *operator->() const { return ptr_.get(); }
};

struct Base {
  virtual void Foo() const { std::cout << "Base::foo\n"; }
  virtual ~Base() = default;
};

struct Derived : Base {
  void Foo() const override { std::cout << "Derived::foo\n"; }
};

int main() {
  Derived d;
  UniversalPtr<Base> p_from_ref(d); // copies d into a new, owned Derived
  UniversalPtr<Base> p_from_raw(
      new Derived());                 // adopts and will delete this object
  UniversalPtr<Base> p_null(nullptr); // owns nothing
  auto uptr = std::make_unique<Derived>();
  UniversalPtr<Base> p_from_unique(std::move(uptr)); // ownership transferred in

  p_from_ref->Foo();
  p_from_raw->Foo();
  std::cout << "pNull.get() == nullptr: " << std::boolalpha
            << (p_null.Get() == nullptr) << "\n";
  p_from_unique->Foo();

  return 0;
}

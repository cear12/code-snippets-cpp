// Universal Pointer idiom (always-owning variant): every constructor
// results in std::shared_ptr<T> owning *something* -- a copy (from a
// reference), the object itself (from a raw pointer), or a transferred
// unique_ptr. Contrast with idioms-cpp's universal-ptr, which adds a
// genuinely non-owning/observing mode via a no-op deleter.
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

template <typename T>
struct UniversalPtr {
    std::shared_ptr<T> ptr;

    constexpr UniversalPtr() noexcept = default;
    constexpr UniversalPtr(std::nullptr_t) noexcept {}

    // From a reference: copies U into a new, owned object.
    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    UniversalPtr(const U& r) : ptr(std::make_shared<U>(r)) {}

    // From a raw pointer: adopts it -- ptr will delete it later.
    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    UniversalPtr(U* rawPtr) : ptr(rawPtr) {}

    // From a unique_ptr: ownership transfers to ptr.
    template <typename U, typename D, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    UniversalPtr(std::unique_ptr<U, D>&& uptr) : ptr(std::move(uptr)) {}

    T* get() const noexcept { return ptr.get(); }
    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr.get(); }
};

struct Base {
    virtual void foo() const { std::cout << "Base::foo\n"; }
    virtual ~Base() = default;
};

struct Derived : Base {
    void foo() const override { std::cout << "Derived::foo\n"; }
};

int main() {
    Derived d;
    UniversalPtr<Base> pFromRef(d);               // copies d into a new, owned Derived
    UniversalPtr<Base> pFromRaw(new Derived());     // adopts and will delete this object
    UniversalPtr<Base> pNull(nullptr);              // owns nothing
    auto uptr = std::make_unique<Derived>();
    UniversalPtr<Base> pFromUnique(std::move(uptr)); // ownership transferred in

    pFromRef->foo();
    pFromRaw->foo();
    std::cout << "pNull.get() == nullptr: " << std::boolalpha << (pNull.get() == nullptr) << "\n";
    pFromUnique->foo();

    return 0;
}

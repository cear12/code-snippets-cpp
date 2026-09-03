// A Minimal Custom Allocator idiom: the member shape std::list (or any
// other standard container) expects from its allocator template argument.
#include <iostream>
#include <list>
#include <new>
#include <utility>

template <typename T>
class MyAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    MyAllocator() noexcept = default;

    template <typename U>
    MyAllocator(const MyAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        std::cout << "MyAllocator::allocate(" << n << ")\n";
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type n) noexcept {
        std::cout << "MyAllocator::deallocate(" << n << ")\n";
        ::operator delete(p);
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) {
        p->~U();
    }

    template <typename U>
    bool operator==(const MyAllocator<U>&) const noexcept { return true; }
    template <typename U>
    bool operator!=(const MyAllocator<U>&) const noexcept { return false; }
};

int main() {
    // BUG FIX: originally referenced an undefined `helloworld<int>`
    // allocator type -- a copy-paste leftover. Uses MyAllocator, as the
    // rest of the file defines.
    std::list<int, MyAllocator<int>> v;
    v.push_back(42);
    v.push_back(43);

    std::cout << "list contents: ";
    for (int x : v) std::cout << x << " ";
    std::cout << "\n";

    return 0;
}

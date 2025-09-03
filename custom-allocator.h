template<typename T>
class MyAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    MyAllocator() noexcept {}
    ~MyAllocator() noexcept {}

    template<typename U>
    MyAllocator(const MyAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type) noexcept {
        ::operator delete(p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new ((void*)p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
};

void test() {
 std::list<int, helloworld<int>> v;
 v.push_back(42);
}
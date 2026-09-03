// unique_ptr, reimplemented (as demo::unique_ptr) to show the RAII +
// move-only mechanics std::unique_ptr is built from.
#include <iostream>
#include <utility>

namespace demo {

template <typename T>
class unique_ptr {
public:
    explicit unique_ptr(T* ptr = nullptr) : ptr_(ptr) {}
    ~unique_ptr() { delete ptr_; }

    unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

private:
    T* ptr_;
};

} // namespace demo

struct Widget {
    int id;
    explicit Widget(int i) : id(i) { std::cout << "Widget(" << id << ") constructed\n"; }
    ~Widget() { std::cout << "Widget(" << id << ") destroyed\n"; }
};

int main() {
    demo::unique_ptr<Widget> a(new Widget(1));
    std::cout << "a->id = " << a->id << "\n";

    demo::unique_ptr<Widget> b(std::move(a)); // ownership transferred to b
    std::cout << "a.get() == nullptr: " << std::boolalpha << (a.get() == nullptr) << "\n";
    std::cout << "b->id = " << b->id << "\n";

    return 0; // Widget(1) destroyed exactly once, via b's destructor
}

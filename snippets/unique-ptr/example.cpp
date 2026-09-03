// unique_ptr, reimplemented (as demo::unique_ptr) to show the RAII +
// move-only mechanics std::unique_ptr is built from.
#include <iostream>
#include <utility>

namespace demo {

template <typename T> class UniquePtr {
public:
  explicit UniquePtr(T *ptr = nullptr) : ptr_(ptr) {}
  ~UniquePtr() { delete ptr_; }

  UniquePtr(UniquePtr &&other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  UniquePtr &operator=(UniquePtr &&other) noexcept {
    if (this != &other) {
      delete ptr_;
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  UniquePtr(const UniquePtr &) = delete;
  UniquePtr &operator=(const UniquePtr &) = delete;

  T &operator*() const { return *ptr_; }
  T *operator->() const { return ptr_; }
  T *Get() const { return ptr_; }

private:
  T *ptr_;
};

} // namespace demo

struct Widget {
  int id_;
  explicit Widget(int i) : id_(i) {
    std::cout << "Widget(" << id_ << ") constructed\n";
  }
  ~Widget() { std::cout << "Widget(" << id_ << ") destroyed\n"; }
};

int main() {
  demo::UniquePtr<Widget> a(new Widget(1));
  std::cout << "a->id = " << a->id_ << "\n";

  demo::UniquePtr<Widget> b(std::move(a)); // ownership transferred to b
  std::cout << "a.get() == nullptr: " << std::boolalpha << (a.Get() == nullptr)
            << "\n";
  std::cout << "b->id = " << b->id_ << "\n";

  return 0; // Widget(1) destroyed exactly once, via b's destructor
}

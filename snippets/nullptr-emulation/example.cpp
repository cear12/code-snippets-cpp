// nullptr Emulation idiom: nullptr_t_demo implicitly converts to any
// pointer type but to nothing else -- the pre-C++11 library stand-in for
// the `nullptr` keyword. See idioms-cpp/idioms/nullptr-emulation for the
// fuller writeup; kept here as a self-contained duplicate since this
// repository is a separate, standalone snippet collection.
#include <iostream>

struct NullptrTDemo {
  template <typename T> operator T *() const noexcept { return 0; }

  template <typename C, typename T> operator T C::*() const noexcept {
    return 0;
  }

private:
  void operator&() const = delete; // can't take its address
};

constexpr NullptrTDemo kMyNullptr = {};

struct S {
  int m_;
};

int main() {
  int *p_int = kMyNullptr;
  double *p_double = kMyNullptr;
  int S::*ptr_to_member = kMyNullptr;

  if (p_int == nullptr && p_double == nullptr && ptr_to_member == nullptr) {
    std::cout << "All pointers are null\n";
  }

  auto takes_int_ptr = [](int *) { std::cout << "called takesIntPtr(int*)\n"; };
  auto takes_void_ptr = [](void *) {
    std::cout << "called takesVoidPtr(void*)\n";
  };
  auto takes_member_ptr = [](int S::*) {
    std::cout << "called takesMemberPtr(int S::*)\n";
  };

  takes_int_ptr(kMyNullptr);
  takes_void_ptr(kMyNullptr);
  takes_member_ptr(kMyNullptr);

  return 0;
}

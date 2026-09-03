// nullptr Emulation idiom: nullptr_t_demo implicitly converts to any
// pointer type but to nothing else -- the pre-C++11 library stand-in for
// the `nullptr` keyword. See idioms-cpp/idioms/nullptr-emulation for the
// fuller writeup; kept here as a self-contained duplicate since this
// repository is a separate, standalone snippet collection.
#include <iostream>

struct nullptr_t_demo {
    template <typename T>
    operator T*() const noexcept { return 0; }

    template <typename C, typename T>
    operator T C::*() const noexcept { return 0; }

private:
    void operator&() const = delete; // can't take its address
};

constexpr nullptr_t_demo my_nullptr = {};

struct S { int m; };

int main() {
    int* pInt = my_nullptr;
    double* pDouble = my_nullptr;
    int S::* ptrToMember = my_nullptr;

    if (pInt == nullptr && pDouble == nullptr && ptrToMember == nullptr) {
        std::cout << "All pointers are null\n";
    }

    auto takesIntPtr = [](int*) { std::cout << "called takesIntPtr(int*)\n"; };
    auto takesVoidPtr = [](void*) { std::cout << "called takesVoidPtr(void*)\n"; };
    auto takesMemberPtr = [](int S::*) { std::cout << "called takesMemberPtr(int S::*)\n"; };

    takesIntPtr(my_nullptr);
    takesVoidPtr(my_nullptr);
    takesMemberPtr(my_nullptr);

    return 0;
}

// Empty Base Optimization (EBO) idiom: an empty class has some nonzero
// size on its own, but adds nothing to a derived class's size when used
// as a base (unlike as an ordinary data member, which would).
#include <iostream>

struct Empty {};

struct EmptyAsMember {
    Empty e;
    int i;
};

struct EmptyAsBase : Empty {
    int i;
};

int main() {
    std::cout << "sizeof(Empty)         = " << sizeof(Empty) << " (never 0)\n";
    std::cout << "sizeof(int)            = " << sizeof(int) << "\n";
    std::cout << "sizeof(EmptyAsMember)  = " << sizeof(EmptyAsMember)
              << " (Empty as a member costs real space)\n";
    std::cout << "sizeof(EmptyAsBase)    = " << sizeof(EmptyAsBase)
              << " (Empty as a base is typically free)\n";
    return 0;
}

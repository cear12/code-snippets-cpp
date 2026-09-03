// std::invoke idiom: one uniform call syntax for a plain function, a
// function object, and a pointer to member function.
#include <functional>
#include <iostream>

void FreeFunction(int a) {
  std::cout << "Free function called with argument: " << a << "\n";
}

struct Functor {
  void operator()(int a) const {
    std::cout << "Functor called with argument: " << a << "\n";
  }
};

struct Widget {
  void Method(int a) {
    std::cout << "Widget::method called with argument: " << a << "\n";
  }
};

int main() {
  std::invoke(FreeFunction, 42);

  Functor functor;
  std::invoke(functor, 42);

  Widget widget;
  std::invoke(&Widget::Method, widget, 42);  // pointer to member function
  std::invoke(&Widget::Method, &widget, 42); // also works through a pointer

  return 0;
}

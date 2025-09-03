void function(int a) {
    std::cout << "Function called with argument: " << a << std::endl;
}

struct Functor {
    void operator()(int a) const {
        std::cout << "Functor called with argument: " << a << std::endl;
    }
};

struct Class {
    void method(int a) {
        std::cout << "Method called with argument: " << a << std::endl;
    }
};

int main() {
    // function
    std::invoke(function, 42);

    // functor
    Functor functor;
    std::invoke(functor, 42);
}
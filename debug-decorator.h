#include <iostream>
#include <string>
#include <utility>    // для std::forward
#include <cstdlib>    // для rand()
#include <ctime>      // для time()

using namespace std;

// Шаблон декоратора, который принимает вызываемый объект и строку-идентификатор для вывода дебаг-сообщений
template <typename Callable>
class DebugDecorator {
public:
    // Конструктор принимает ссылку на вызываемый объект и строку, которая будет выводиться при вызове
    DebugDecorator(const Callable& c, const char* s) : c_(c), s_(s) {}

    // Перегрузка оператора вызова, который принимает произвольное число аргументов
    template <typename... Args>
    auto operator()(Args&&... args) const {
        cout << "Вызывается " << s_ << endl;
        auto res = c_(std::forward<Args>(args)...);
        cout << "Результат: " << res << endl;
        return res;
    }

private:
    const Callable& c_;     // ссылка на оригинальный вызываемый объект (функция, лямбда, объект с operator())
    const std::string s_;     // строка-идентификатор для вывода
};

// Функция для создания декоратора (позволяет не указывать явно тип шаблона)
template <typename Callable>
auto decorate_debug(const Callable& c, const char* s) {
    return DebugDecorator<Callable>(c, s);
}

// Пример 1: декорирование обычной функции
int g(int t, int j) { 
    return t - j; 
}

// Пример 2: декорирование вызываемого объекта (объект с перегруженным operator())
struct S {
    double operator()() const {
        return double(rand() + 1) / double(rand() + 1);
    }
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    // Декорирование функции g
    auto gl = decorate_debug(g, "g()");
    // Вызов gl(5, 2) должен вывести:
    // "Вызывается g()"
    // "Результат: 3"
    gl(5, 2);

    // Декорирование вызываемого объекта s
    S s;
    auto s1 = decorate_debug(s, "rand/rand");
    s1(); // вызов декорированной функции
    s1(); // повторный вызов декорированной функции

    // Декорирование лямбда-выражения (вызываемого объекта)
    auto f2 = decorate_debug([](int t, int j) { return t + j; }, "t+j");
    // Вызов f2(5, 3) выведет:
    // "Вызывается t+j"
    // "Результат: 8"
    f2(5, 3);

    return 0;
}
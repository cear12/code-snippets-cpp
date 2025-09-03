#include <iostream>
#include <vector>
#include <functional>
#include <exception>

/*
    Aвтоматическое выполненияе отката только при возникновении исключения
*/

// Класс ScopeFail: откат только при выбросе исключения
class ScopeFail {
    int exception_count;
    bool active = true;
    std::function<void()> func;
public:
    explicit ScopeFail(std::function<void()> f)
        : exception_count(std::uncaught_exceptions()), func(std::move(f)) {}
    ~ScopeFail() noexcept {
        // Вызов отката только если в процессе работы возникло новое исключение
        if (active && std::uncaught_exceptions() > exception_count) {
            func();
        }
    }
    void dismiss() { active = false; }
};

// 1. Обычный пример использования ScopeFail
void example(bool should_throw) {
    std::vector<int> data;

    // Критическое действие
    data.push_back(42);

    // Guard для rollback — вызовется только если возникнет исключение
    ScopeFail rollback_guard{[&](){
        // Будет вызван ТОЛЬКО при исключении!
        std::cout << "Rolling back changes!\n";
        data.pop_back();
    }};

    if (should_throw) {
        throw std::runtime_error("Something went wrong!");
    }

    // Всё прошло успешно, откат не нужен
    rollback_guard.dismiss();

    std::cout << "Operation succeeded!\n";
}

// 2. Пример использования ScopeFail через Boost
void example(bool should_throw) {
    std::vector<int> data;
    data.push_back(42);
    boost::scope::scope_fail rollback_guard{[&]{
        std::cout << "Rollback via boost!\n";
        data.pop_back();
    }};
    if (should_throw) throw std::runtime_error("Oops!");
    rollback_guard.set_active(false);
    std::cout << "Success via boost!\n";
}

int main() {
    try {
        example(true); // Имитируем ошибку: случится откат
    } catch (...) {}

    example(false); // Всё успешно, отката нет
    return 0;
}



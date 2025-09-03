/*
    Реализация собственный "nullptr" до стандарта C++11.
    Описывается тип nullptr_t и объект nullptr, который можно неявно
    преобразовать в любой указатель или указатель на член.
*/

#include <cstddef>   // для std::size_t

// 1. Определяем свой тип nullptr_t
struct nullptr_t {
    // Преобразование в любой тип указателя
    template<typename T>
    operator T*() const noexcept { 
        return 0; 
    }

    // Преобразование в любой тип указателя на член
    template<typename C, typename T>
    operator T C::*() const noexcept { 
        return 0; 
    }

private:
    // Запрещаем создание дополнительных экземпляров
    void operator&() const = delete;
};

// 2. Создаем единственный объект nullptr
constexpr nullptr_t nullptr = {};

// 3. Демонстрация использования
#include <iostream>

int main() {
    int* pInt = nullptr;                // конвертация nullptr_t -> int*
    double* pDouble = nullptr;          // конвертация nullptr_t -> double*
    
    struct S { int m; };
    int S::* ptrToMember = nullptr;     // конвертация nullptr_t -> указатель на член

    if (pInt == nullptr && pDouble == nullptr && ptrToMember == nullptr) {
        std::cout << "All pointers are null\n";
    }

    // Передача nullptr в функцию перегрузку
    auto func = [](int*)       { std::cout << "Called func(int*)\n"; };
    auto func2 = [](void*)     { std::cout << "Called func(void*)\n"; };
    auto func3 = [](int S::*)  { std::cout << "Called func(int S::*)\n"; };

    func(nullptr);
    func2(nullptr);
    func3(nullptr);

    return 0;
}

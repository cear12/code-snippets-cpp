#ifndef FACTORY_H
#define FACTORY_H

код С++, содружество проектов, в проекте core содержится класс фабрика, а также базовый класс A, в проекте wincrypt содержится потомок A класс B, в проекте cryptoki содержится класс C, потомок A. Core создает объекты этих классов потомков, поэтому содержит зависимости от wincrypt / cryptoki. Как исправить эту ситуацию, чтобы crypto не содержало зависимости
ChatGPT сказал:
Чтобы решить проблему зависимости проекта core от wincrypt и cryptoki, можно применить паттерн проектирования "Абстрактная фабрика" или плагины через регистрацию типов. Эти подходы позволяют создать расширяемую архитектуру, в которой core взаимодействует только с абстракциями, не завися от конкретных реализаций.

Проблема
core содержит базовый класс A и фабрику для создания объектов.
Конкретные реализации A (B в wincrypt и C в cryptoki) требуют зависимости на проекты wincrypt и cryptoki в core.
Это нарушает принцип Dependency Inversion Principle (DIP) из SOLID: модуль верхнего уровня (core) не должен зависеть от модулей нижнего уровня (wincrypt, cryptoki).

Решение
Мы сделаем фабрику в core независимой от конкретных классов, используя динамическую регистрацию или внешние зависимости.

1. Используем регистрацию через идентификаторы
Интерфейс фабрики в core:

В core создаём фабрику, которая регистрирует типы потомков через идентификаторы (std::string, enum, и т. д.).

cpp
Копировать код
// core/A.h
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

class A {
public:
    virtual ~A() = default;
    virtual void doWork() = 0;
};

class Factory {
public:
    using Creator = std::function<std::unique_ptr<A>()>;

    static Factory& instance() {
        static Factory factory;
        return factory;
    }

    void registerType(const std::string& id, Creator creator) {
        creators_[id] = std::move(creator);
    }

    std::unique_ptr<A> create(const std::string& id) {
        auto it = creators_.find(id);
        if (it != creators_.end()) {
            return it->second();
        }
        throw std::runtime_error("Unknown type id: " + id);
    }

private:
    std::unordered_map<std::string, Creator> creators_;
};
Конкретные реализации в других проектах:

Реализации классов B и C регистрируются в фабрике через статический метод.

cpp
Копировать код
// wincrypt/B.h
#pragma once
#include "core/A.h"
#include <iostream>

class B : public A {
public:
    void doWork() override {
        std::cout << "B is working\n";
    }

    static void registerType() {
        Factory::instance().registerType("B", []() { return std::make_unique<B>(); });
    }
};

// В каком-то cpp файле wincrypt
B::registerType();
cpp
Копировать код
// cryptoki/C.h
#pragma once
#include "core/A.h"
#include <iostream>

class C : public A {
public:
    void doWork() override {
        std::cout << "C is working\n";
    }

    static void registerType() {
        Factory::instance().registerType("C", []() { return std::make_unique<C>(); });
    }
};

// В каком-то cpp файле cryptoki
C::registerType();
Использование в core:

Теперь core знает только о базовом классе A и абстрактной фабрике.

cpp
Копировать код
// main.cpp (core)
#include "core/A.h"

int main() {
    try {
        auto objB = Factory::instance().create("B");
        objB->doWork();

        auto objC = Factory::instance().create("C");
        objC->doWork();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}

#endif // FACTORY_H


#define REGISTER_CLASS(DerivedClass, BaseClass) \
    namespace { \
    struct DerivedClass##Registrar { \
        DerivedClass##Registrar() { \
            Factory::instance().registerType(#DerivedClass, []() { \
                return std::make_unique<DerivedClass>(); \
            }); \
        } \
    } DerivedClass##_registrar; \
    } \
    static_assert(std::is_base_of_v<BaseClass, DerivedClass>, \
                 #DerivedClass " must inherit from " #BaseClass)

Теперь в производных классах достаточно использовать макрос REGISTER_CLASS:

cpp
// wincrypt/B.h
#pragma once
#include "core/A.h"
#include <iostream>

class B : public A {
public:
    void doWork() override {
        std::cout << "B is working\n";
    }
};

// Автоматическая регистрация при загрузке модуля
REGISTER_CLASS(B, A);

#include "core/A.h"
#include <iostream>

class B : public A {
public:
    void doWork() override {
        std::cout << "B is working\n";
    }

private:
    // Статическая регистрация при загрузке модуля
    static inline bool registered_ = Factory::registerType<B>("B");
};

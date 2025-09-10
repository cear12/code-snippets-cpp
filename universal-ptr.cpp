#include <memory>
#include <type_traits>
#include <utility>

// Универсальный умный указатель
template<typename T>
struct UniversalPtr {
    // Храним внутри shared_ptr, может владеть или НЕ владеть ресурсом.
    std::shared_ptr<T> m_ptr;

    // Конструктор по умолчанию — ptr не владеет ничем (nullptr)
    constexpr UniversalPtr() noexcept = default;

    // Конструктор от nullptr — тоже не владеет ресурсом
    constexpr UniversalPtr(std::nullptr_t) noexcept {}

    // Если передана ссылка: создаём копию и владеем ею
    template<typename U,
             typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    UniversalPtr(const U& r)
        : m_ptr(std::make_shared<U>(r)) // копируем источник, shared_ptr владеет ресурсом
    {}

    // Если передан указатель: shared_ptr будет владеть ресурсом и удалять его
    template<typename U,
             typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    UniversalPtr(U* ptr)
        : m_ptr(ptr) // shared_ptr будет удалять по умолчанию
    {}

    // Если передан unique_ptr: shared_ptr получает владение
    template<typename U, typename D,
             typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    UniversalPtr(std::unique_ptr<U, D>&& uptr)
        : m_ptr(std::move(uptr)) // shared_ptr теперь владеет ресурсом
    {}

    // Статическая функция для "не-владения", если нужно
private:
    static constexpr void NoDelete(T*) noexcept {} // placeholder, не используется
public:
    // Доступ к ресурсам
    T* get() const noexcept { return m_ptr.get(); }
    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr.get(); }
};

// Пример использования
#include <iostream>

struct Base {
    virtual void foo() { std::cout << "Base::foo\n"; }
    virtual ~Base() {}
};

struct Derived : Base {
    void foo() override { std::cout << "Derived::foo\n"; }
};

int main() {
    Derived d;
    UniversalPtr<Base> p_ref(d); // Копирует d, владеет копией
    UniversalPtr<Base> p_ptr(new Derived()); // Владеет объектом и удалит его
    UniversalPtr<Base> p_null(nullptr); // Не владеет ничем
    auto uptr = std::make_unique<Derived>();
    UniversalPtr<Base> p_unique(std::move(uptr)); // Владеет ресурсом

    p_ref->foo();    // Копия d: Derived::foo
    p_ptr->foo();    // Новый объект: Derived::foo
    std::cout << (p_null.get() == nullptr) << std::endl; // true
    p_unique->foo(); // Переданный unique_ptr: Derived::foo
}

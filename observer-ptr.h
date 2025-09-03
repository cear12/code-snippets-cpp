/*
    Уолтер Браун (Walter Brown): «Это самый тупой из умных 
    указателей», – и это всего лишь класс-обертка вокруг простого, невладеющего 
    указателя
*/
template<typename T>
class observer_ptr {
    T *m_ptr = nullptr;
public:
    constexpr observer_ptr() noexcept = default;
    constexpr observer_ptr(T *p) noexcept : m_ptr(p) {}

    T *get() const noexcept { return m_ptr; }

    operator bool() const noexcept { return bool(get()); }

    T& operator*() const noexcept { return *get(); }
    T* operator->() const noexcept { return get(); }
};
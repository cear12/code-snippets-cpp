// smart_ptr.hpp - Демонстрация RAII и управления памятью
template<typename T>
class unique_ptr {
private:
    T* ptr_;
public:
    explicit unique_ptr(T* ptr = nullptr) : ptr_(ptr) {}
    ~unique_ptr() { delete ptr_; }
    
    // Move semantics
    unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }
    
    // Запрещаем копирование
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
};

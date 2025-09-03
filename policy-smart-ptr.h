template <typename T, typename DeletionPolicy, typename ReleasePolicy>
class SmartPtr : private DeletionPolicy {
public:
    // Конструктор с инициализацией указателя
    SmartPtr(T* ptr = nullptr) : p_(ptr) {}

    // Деструктор: вызываем политику удаления
    ~SmartPtr() {
        DeletionPolicy::operator()(p_);
    }
    
    // Метод для получения внутреннего указателя
    T* get() const {
        return p_;
    }
    
    // Метод освобождения владения указателем (просто обнуляем его)
    void release() {
        p_ = nullptr;
    }
    
private:
    T* p_;
};


// Политика отладки с выводом сообщений при конструировании и удалении
struct Debug {
    template <typename T>
    static void constructed(const T* p) {
        std::cout << "Сконструирован SmartPtr для объекта " 
                  << static_cast<const void*>(p) << std::endl;
    }
    
    template <typename T>
    static void deleted(const T* p) {
        std::cout << "Уничтожен SmartPtr для объекта " 
                  << static_cast<const void*>(p) << std::endl;
    }
};

// Пустая политика отладки (ничего не выводит)
struct NoDebug {
    template <typename T>
    static void constructed(const T* p) {
        // Ничего не делаем
    }
    
    template <typename T>
    static void deleted(const T* p) {
        // Ничего не делаем
    }
};

template <typename T, typename DeletionPolicy, typename DebugPolicy = NoDebug>
class SmartPtr : private DeletionPolicy {
public:
    // Явный конструктор с указателем и политикой удаления по умолчанию
    explicit SmartPtr(T* p = nullptr, 
                      DeletionPolicy&& deletion_policy = DeletionPolicy())
        : DeletionPolicy(std::move(deletion_policy)), p_(p)
    {
        DebugPolicy::constructed(p_);
        // Здесь можно добавить дополнительный код, если необходимо
    }
    
    // Деструктор: сначала вывод сообщения об удалении, затем вызывается политика удаления
    ~SmartPtr() {
        DebugPolicy::deleted(p_);
        DeletionPolicy::operator()(p_);
    }
    
    // Дополнительные методы (например, get, operator->, operator*, и т.д.) можно добавить здесь

private:
    T* p_;
};
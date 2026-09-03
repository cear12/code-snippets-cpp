// Policy-Based Smart Pointer idiom: deletion strategy and debug logging
// are both template parameters, composed at compile time with no virtual
// dispatch involved.
#include <iostream>
#include <utility>

// --- Deletion policies.
struct DeleteSingle {
    template <typename T>
    void operator()(T* p) const {
        delete p;
    }
};

struct DeleteArray {
    template <typename T>
    void operator()(T* p) const {
        delete[] p;
    }
};

// --- Debug policies.
struct Debug {
    template <typename T>
    static void constructed(const T* p) {
        std::cout << "Constructed SmartPtr for object " << static_cast<const void*>(p) << "\n";
    }
    template <typename T>
    static void deleted(const T* p) {
        std::cout << "Destroyed SmartPtr for object " << static_cast<const void*>(p) << "\n";
    }
};

struct NoDebug {
    template <typename T>
    static void constructed(const T*) {}
    template <typename T>
    static void deleted(const T*) {}
};

template <typename T, typename DeletionPolicy, typename DebugPolicy = NoDebug>
class SmartPtr : private DeletionPolicy {
public:
    explicit SmartPtr(T* p = nullptr, DeletionPolicy deletion_policy = DeletionPolicy())
        : DeletionPolicy(std::move(deletion_policy)), p_(p) {
        DebugPolicy::constructed(p_);
    }

    ~SmartPtr() {
        DebugPolicy::deleted(p_);
        DeletionPolicy::operator()(p_);
    }

    SmartPtr(const SmartPtr&) = delete;
    SmartPtr& operator=(const SmartPtr&) = delete;

    T* get() const { return p_; }
    T& operator*() const { return *p_; }
    T* operator->() const { return p_; }

private:
    T* p_;
};

int main() {
    std::cout << "-- single object, with debug logging --\n";
    {
        SmartPtr<int, DeleteSingle, Debug> p(new int(42));
        std::cout << "*p = " << *p << "\n";
    }

    std::cout << "\n-- array, without debug logging --\n";
    {
        SmartPtr<int, DeleteArray> arr(new int[5]{1, 2, 3, 4, 5});
        std::cout << "arr.get()[2] = " << arr.get()[2] << "\n";
    }

    return 0;
}

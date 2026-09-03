// observer_ptr idiom: a pointer wrapper that documents "does not own
// this" as part of the type, with none of unique_ptr/shared_ptr's
// ownership machinery.
#include <iostream>

template <typename T>
class observer_ptr {
public:
    constexpr observer_ptr() noexcept = default;
    constexpr observer_ptr(T* p) noexcept : m_ptr(p) {}

    T* get() const noexcept { return m_ptr; }

    explicit operator bool() const noexcept { return m_ptr != nullptr; }

    T& operator*() const noexcept { return *get(); }
    T* operator->() const noexcept { return get(); }

private:
    T* m_ptr = nullptr;
};

struct Widget {
    int id;
};

void inspect(observer_ptr<Widget> w) {
    // The parameter type alone documents that inspect() does not take
    // ownership of w -- unlike, say, a unique_ptr<Widget> parameter would.
    if (w) {
        std::cout << "Widget id=" << w->id << "\n";
    } else {
        std::cout << "no widget\n";
    }
}

int main() {
    Widget widget{99};
    observer_ptr<Widget> obs(&widget); // widget's storage is still owned by main()'s stack frame

    inspect(obs);
    inspect(nullptr);

    return 0;
}

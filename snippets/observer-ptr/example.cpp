// observer_ptr idiom: a pointer wrapper that documents "does not own
// this" as part of the type, with none of unique_ptr/shared_ptr's
// ownership machinery.
#include <iostream>

template <typename T>
class ObserverPtr {
public:
    constexpr ObserverPtr() noexcept = default;
    constexpr ObserverPtr(T* p) noexcept : m_ptr_(p) {}

    T* Get() const noexcept { return m_ptr_; }

    explicit operator bool() const noexcept { return m_ptr_ != nullptr; }

    T& operator*() const noexcept { return *Get(); }
    T* operator->() const noexcept { return Get(); }

private:
    T* m_ptr_ = nullptr;
};

struct Widget {
    int id_;
};

void Inspect(ObserverPtr<Widget> w) {
    // The parameter type alone documents that inspect() does not take
    // ownership of w -- unlike, say, a unique_ptr<Widget> parameter would.
    if (w) {
        std::cout << "Widget id=" << w->id_ << "\n";
    } else {
        std::cout << "no widget\n";
    }
}

int main() {
    Widget widget{99};
    ObserverPtr<Widget> obs(&widget); // widget's storage is still owned by main()'s stack frame

    Inspect(obs);
    Inspect(nullptr);

    return 0;
}

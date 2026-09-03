// A Minimal Custom Iterator idiom: only operator*, operator++, and
// operator!= are implemented -- exactly what a range-based for loop
// needs, and nothing more.
#include <iostream>

template <typename T>
class SimpleContainer {
public:
    SimpleContainer() = default;

    void addItem(const T& item) {
        if (size_ < kMaxSize) {
            data_[size_++] = item;
        } else {
            std::cerr << "Container is full. Cannot add more items.\n";
        }
    }

    class Iterator {
    public:
        Iterator(const SimpleContainer& container, int index) : container_(container), index_(index) {}

        bool operator!=(const Iterator& other) const { return index_ != other.index_; }
        T operator*() const { return container_.data_[index_]; }
        Iterator& operator++() {
            ++index_;
            return *this;
        }

    private:
        const SimpleContainer& container_;
        int index_;
    };

    Iterator begin() const { return Iterator(*this, 0); }
    Iterator end() const { return Iterator(*this, size_); }

private:
    static constexpr int kMaxSize = 10;
    T data_[kMaxSize]{};
    int size_ = 0;
};

int main() {
    SimpleContainer<int> container;
    container.addItem(1);
    container.addItem(2);
    container.addItem(3);

    for (int value : container) { // works because of operator*/++/!= alone
        std::cout << value << " ";
    }
    std::cout << "\n";

    return 0;
}

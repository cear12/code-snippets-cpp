#include <iostream>

template <typename T>
class SimpleContainer {
private:
    static const int MAX_SIZE = 10;
    T data[MAX_SIZE];
    int size;

public:
    SimpleContainer() : size(0) {}

    void addItem(const T& item) {
        if (size < MAX_SIZE) {
            data[size++] = item;
        } else {
            std::cerr << "Container is full. Cannot add more items." << std::endl;
        }
    }

    class Iterator {
    private:
        const SimpleContainer& container;
        int index;

    public:
        Iterator(const SimpleContainer& cont, int idx) : container(cont), index(idx) {}

        bool operator!=(const Iterator& other) const {
            return index != other.index;
        }

        T operator*() const {
            return container.data[index];
        }

        Iterator& operator++() {
            ++index;
            return *this;
        }
    };

    Iterator begin() const {
        return Iterator(*this, 0);
    }

    Iterator end() const {
        return Iterator(*this, size);
    }
};

int main() {
    SimpleContainer<int> container;
    container.addItem(1);
    container.addItem(2);
    container.addItem(3);

    for (SimpleContainer<int>::Iterator it = container.begin(); it != container.end(); ++it) {
        std::cout << *it << " ";
    }

    return 0;
}

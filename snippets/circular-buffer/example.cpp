// Circular (ring) buffer idiom: fixed-capacity FIFO where put() past
// capacity overwrites the oldest unread element. head_/tail_ chase each
// other modulo max_size_; full_ disambiguates empty vs. full, which both
// have head_ == tail_. See README.md for the Boost variant (not compiled
// here -- Boost unavailable locally, see README).
#include <iostream>
#include <memory>
#include <mutex>

namespace circular_buffer {

template <class T>
class CircularBuffer {
public:
    explicit CircularBuffer(size_t size) : buf_(std::make_unique<T[]>(size)), max_size_(size) {}

    void put(T item) {
        std::lock_guard<std::mutex> lock(mutex_);

        buf_[head_] = item;

        if (full_) {
            tail_ = (tail_ + 1) % max_size_;
        }

        head_ = (head_ + 1) % max_size_;
        full_ = head_ == tail_;
    }

    T get() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (empty()) {
            return T();
        }

        auto val = buf_[tail_];
        full_ = false;
        tail_ = (tail_ + 1) % max_size_;

        return val;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = tail_;
        full_ = false;
    }

    bool empty() const { return !full_ && (head_ == tail_); }
    bool full() const { return full_; }
    size_t capacity() const { return max_size_; }

    size_t size() const {
        if (full_) return max_size_;
        return head_ >= tail_ ? head_ - tail_ : max_size_ + head_ - tail_;
    }

private:
    std::mutex mutex_;
    std::unique_ptr<T[]> buf_;
    size_t head_ = 0;
    size_t tail_ = 0;
    const size_t max_size_;
    bool full_ = false;
};

} // namespace circular_buffer

int main() {
    using circular_buffer::CircularBuffer;

    CircularBuffer<std::uint32_t> circle(10);
    std::cout << "Size: " << circle.size() << " Capacity: " << circle.capacity() << "\n";

    circle.put(1);
    std::cout << "Popped: " << circle.get() << "\n";
    std::cout << "Empty: " << std::boolalpha << circle.empty() << "\n";

    std::cout << "Filling to capacity...\n";
    for (std::uint32_t i = 0; i < circle.capacity(); ++i) circle.put(i);
    std::cout << "Full: " << circle.full() << "\n";

    std::cout << "Overfilling by 5 (oldest entries get overwritten)...\n";
    for (std::uint32_t i = 0; i < 5; ++i) circle.put(100 + i);

    std::cout << "Reading back: ";
    while (!circle.empty()) std::cout << circle.get() << " ";
    std::cout << "\n";

    std::cout << "Empty: " << circle.empty() << " Full: " << circle.full() << "\n";

    return 0;
}

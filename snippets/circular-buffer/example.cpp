// Circular (ring) buffer idiom: fixed-capacity FIFO where put() past
// capacity overwrites the oldest unread element. head_/tail_ chase each
// other modulo max_size_; full_ disambiguates empty vs. full, which both
// have head_ == tail_. See README.md for the Boost variant (not compiled
// here -- Boost unavailable locally, see README).
#include <iostream>
#include <memory>
#include <mutex>

namespace circular_buffer {

template <class T> class CircularBuffer {
public:
  explicit CircularBuffer(size_t size)
      : buf_(std::make_unique<T[]>(size)), kMaxSize(size) {}

  void Put(T item) {
    std::lock_guard<std::mutex> lock(mutex_);

    buf_[head_] = item;

    if (full_) {
      tail_ = (tail_ + 1) % kMaxSize;
    }

    head_ = (head_ + 1) % kMaxSize;
    full_ = head_ == tail_;
  }

  T Get() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (Empty()) {
      return T();
    }

    auto val = buf_[tail_];
    full_ = false;
    tail_ = (tail_ + 1) % kMaxSize;

    return val;
  }

  void Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = tail_;
    full_ = false;
  }

  bool Empty() const { return !full_ && (head_ == tail_); }
  bool Full() const { return full_; }
  size_t Capacity() const { return kMaxSize; }

  size_t Size() const {
    if (full_)
      return kMaxSize;
    return head_ >= tail_ ? head_ - tail_ : kMaxSize + head_ - tail_;
  }

private:
  std::mutex mutex_;
  std::unique_ptr<T[]> buf_;
  size_t head_ = 0;
  size_t tail_ = 0;
  const size_t kMaxSize;
  bool full_ = false;
};

} // namespace circular_buffer

int main() {
  using circular_buffer::CircularBuffer;

  CircularBuffer<std::uint32_t> circle(10);
  std::cout << "Size: " << circle.Size() << " Capacity: " << circle.Capacity()
            << "\n";

  circle.Put(1);
  std::cout << "Popped: " << circle.Get() << "\n";
  std::cout << "Empty: " << std::boolalpha << circle.Empty() << "\n";

  std::cout << "Filling to capacity...\n";
  for (std::uint32_t i = 0; i < circle.Capacity(); ++i)
    circle.Put(i);
  std::cout << "Full: " << circle.Full() << "\n";

  std::cout << "Overfilling by 5 (oldest entries get overwritten)...\n";
  for (std::uint32_t i = 0; i < 5; ++i)
    circle.Put(100 + i);

  std::cout << "Reading back: ";
  while (!circle.Empty())
    std::cout << circle.Get() << " ";
  std::cout << "\n";

  std::cout << "Empty: " << circle.Empty() << " Full: " << circle.Full()
            << "\n";

  return 0;
}

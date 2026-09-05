// Lock-free circular (ring) buffers for the producer-consumer problem:
// SPSCRingBuffer (single producer, single consumer -- the fast path, no
// CAS needed) and MPMCRingBuffer (Dmitry Vyukov's bounded MPMC queue --
// any number of producers/consumers, using compare_exchange to arbitrate).
// ProducerConsumerSystem wraps SPSCRingBuffer with producer/consumer thread
// loops and basic throughput statistics. See README.md for the bugs fixed.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

template <typename T, size_t Capacity> class SPSCRingBuffer {
private:
  static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of 2");
  static constexpr size_t kMask = Capacity - 1;
  static constexpr size_t kCacheLineSize = 64;

  struct alignas(kCacheLineSize) Slot {
    std::atomic<size_t> sequence_{0};
    T data_;
  };

  alignas(kCacheLineSize) std::array<Slot, Capacity> buffer_;
  alignas(kCacheLineSize) std::atomic<size_t> producer_pos_{0};
  alignas(kCacheLineSize) std::atomic<size_t> consumer_pos_{0};

public:
  SPSCRingBuffer() {
    for (size_t i = 0; i < Capacity; ++i) {
      buffer_[i].sequence_.store(i, std::memory_order_relaxed);
    }
  }

  // Single producer, non-blocking.
  template <typename U> bool TryPush(U &&item) {
    size_t pos = producer_pos_.load(std::memory_order_relaxed);
    Slot &slot = buffer_[pos & kMask];
    size_t seq = slot.sequence_.load(std::memory_order_acquire);

    if (seq == pos) {
      slot.data_ = std::forward<U>(item);
      slot.sequence_.store(pos + 1, std::memory_order_release);
      producer_pos_.store(pos + 1, std::memory_order_relaxed);
      return true;
    }

    return false; // Buffer is full.
  }

  // Single producer, blocking (spin-wait).
  template <typename U> void Push(U &&item) {
    while (!TryPush(std::forward<U>(item))) {
      std::this_thread::yield();
    }
  }

  // Single consumer, non-blocking.
  bool TryPop(T &item) {
    size_t pos = consumer_pos_.load(std::memory_order_relaxed);
    Slot &slot = buffer_[pos & kMask];
    size_t seq = slot.sequence_.load(std::memory_order_acquire);

    if (seq == pos + 1) {
      item = std::move(slot.data_);
      slot.sequence_.store(pos + Capacity, std::memory_order_release);
      consumer_pos_.store(pos + 1, std::memory_order_relaxed);
      return true;
    }

    return false; // Buffer is empty.
  }

  // Single consumer, blocking (spin-wait).
  T Pop() {
    T item;
    while (!TryPop(item)) {
      std::this_thread::yield();
    }
    return item;
  }

  // Approximate: read without synchronizing producer_pos_/consumer_pos_ as
  // a pair, so a concurrent push/pop can make these momentarily stale.
  // Fine for monitoring/statistics, not for correctness decisions.
  bool Empty() const {
    return producer_pos_.load(std::memory_order_relaxed) ==
           consumer_pos_.load(std::memory_order_relaxed);
  }

  bool Full() const {
    return (producer_pos_.load(std::memory_order_relaxed) -
            consumer_pos_.load(std::memory_order_relaxed)) >= Capacity;
  }

  size_t Size() const {
    return producer_pos_.load(std::memory_order_relaxed) -
           consumer_pos_.load(std::memory_order_relaxed);
  }

  size_t GetCapacity() const { return Capacity; }
};

// Generalization of the above to multiple producers and consumers: instead
// of a single producer owning producer_pos_ outright, every producer races
// to claim a slot via compare_exchange_weak (and likewise for consumers).
// See SPSCRingBuffer above for the simpler single-writer/single-reader
// fast path this trades away.
template <typename T, size_t Capacity> class MPMCRingBuffer {
private:
  static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of 2");
  static constexpr size_t kMask = Capacity - 1;
  static constexpr size_t kCacheLineSize = 64;

  struct alignas(kCacheLineSize) Slot {
    std::atomic<size_t> sequence_{0};
    T data_;
  };

  alignas(kCacheLineSize) std::array<Slot, Capacity> buffer_;
  alignas(kCacheLineSize) std::atomic<size_t> producer_pos_{0};
  alignas(kCacheLineSize) std::atomic<size_t> consumer_pos_{0};

public:
  MPMCRingBuffer() {
    for (size_t i = 0; i < Capacity; ++i) {
      buffer_[i].sequence_.store(i, std::memory_order_relaxed);
    }
  }

  template <typename U> bool TryPush(U &&item) {
    size_t pos = producer_pos_.load(std::memory_order_relaxed);
    while (true) {
      Slot &slot = buffer_[pos & kMask];
      size_t seq = slot.sequence_.load(std::memory_order_acquire);

      if (seq == pos) {
        if (producer_pos_.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          slot.data_ = std::forward<U>(item);
          slot.sequence_.store(pos + 1, std::memory_order_release);
          return true;
        }
        // Lost the race for this slot; pos was refreshed by the failed
        // CAS, retry with the new value.
      } else if (seq < pos) {
        return false; // Buffer is full.
      } else {
        pos = producer_pos_.load(std::memory_order_relaxed);
      }
    }
  }

  template <typename U> void Push(U &&item) {
    while (!TryPush(std::forward<U>(item))) {
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  }

  bool TryPop(T &item) {
    size_t pos = consumer_pos_.load(std::memory_order_relaxed);
    while (true) {
      Slot &slot = buffer_[pos & kMask];
      size_t seq = slot.sequence_.load(std::memory_order_acquire);

      if (seq == pos + 1) {
        if (consumer_pos_.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          item = std::move(slot.data_);
          slot.sequence_.store(pos + Capacity, std::memory_order_release);
          return true;
        }
      } else if (seq < pos + 1) {
        return false; // Buffer is empty.
      } else {
        pos = consumer_pos_.load(std::memory_order_relaxed);
      }
    }
  }

  T Pop() {
    T item;
    while (!TryPop(item)) {
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    return item;
  }

  size_t GetCapacity() const { return Capacity; }
};

// Wraps an SPSCRingBuffer with producer/consumer thread-loop helpers and
// throughput statistics -- meant to be driven by exactly one producer
// thread and one consumer thread, matching the buffer's own contract.
template <typename T> class ProducerConsumerSystem {
private:
  SPSCRingBuffer<T, 1024> buffer_;
  std::atomic<bool> running_{true};
  std::atomic<bool> producer_done_{false};
  std::atomic<size_t> items_produced_{0};
  std::atomic<size_t> items_consumed_{0};
  std::atomic<size_t> producer_blocks_{0};
  std::atomic<size_t> consumer_blocks_{0};

public:
  template <typename ProducerFn> void RunProducer(ProducerFn &&produce) {
    while (running_.load(std::memory_order_relaxed)) {
      auto item = produce();
      // Retry pushing *this* item until it fits. The original bug called
      // produce() again on every failed attempt instead of retrying the
      // item that just failed to fit -- silently dropping it.
      while (!buffer_.TryPush(std::move(item))) {
        producer_blocks_.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
      items_produced_.fetch_add(1, std::memory_order_relaxed);
    }
    // Release-paired with RunConsumer's acquire load: every push above is
    // guaranteed visible to a consumer that observes producer_done_ true.
    producer_done_.store(true, std::memory_order_release);
  }

  template <typename ConsumerFn> void RunConsumer(ConsumerFn &&consume) {
    T item;
    // Keyed off producer_done_, not running_ directly: RunProducer only
    // sets producer_done_ after its loop has fully exited, i.e. after
    // every successful TryPush it will ever make. Exiting here as soon as
    // running_ flips (the original design) races the producer's last
    // in-flight push -- this thread could observe running_ == false and
    // finish draining before that push happens, permanently losing it.
    while (!producer_done_.load(std::memory_order_acquire)) {
      if (buffer_.TryPop(item)) {
        consume(std::move(item));
        items_consumed_.fetch_add(1, std::memory_order_relaxed);
      } else {
        consumer_blocks_.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    }
    // producer_done_ == true happens-after every push RunProducer made
    // (release/acquire pair below), so this drain is guaranteed to see
    // all of them.
    while (buffer_.TryPop(item)) {
      consume(std::move(item));
      items_consumed_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  // Tells the producer to wind down. Safe to call from any thread at any
  // time -- unlike a naive "just flip running_" stop, RunConsumer no
  // longer keys its own shutdown off this flag directly (see above), so
  // there is no window where the consumer can finish before the
  // producer's last push actually lands.
  void Stop() { running_.store(false, std::memory_order_relaxed); }

  struct Statistics {
    size_t items_produced_;
    size_t items_consumed_;
    size_t producer_blocks_;
    size_t consumer_blocks_;
    size_t buffer_size_;
  };

  Statistics GetStatistics() const {
    return {items_produced_.load(std::memory_order_relaxed),
            items_consumed_.load(std::memory_order_relaxed),
            producer_blocks_.load(std::memory_order_relaxed),
            consumer_blocks_.load(std::memory_order_relaxed), buffer_.Size()};
  }
};

int main() {
  // 1. SPSCRingBuffer, single-threaded: basic FIFO/full/empty behavior.
  {
    SPSCRingBuffer<int, 4> buffer;
    bool ok = buffer.Empty() && !buffer.Full();
    for (int i = 0; i < 4; ++i) ok = ok && buffer.TryPush(i);
    ok = ok && buffer.Full() && !buffer.TryPush(99); // 5th push must fail.

    for (int i = 0; i < 4; ++i) {
      int item = -1;
      ok = ok && buffer.TryPop(item) && item == i; // FIFO order preserved.
    }
    ok = ok && buffer.Empty();
    int unused;
    ok = ok && !buffer.TryPop(unused); // Popping an empty buffer must fail.

    std::cout << "[SPSCRingBuffer] single-threaded FIFO/full/empty: "
              << (ok ? "PASS" : "FAIL") << "\n";
  }

  // 2. SPSCRingBuffer, real concurrency: one producer thread, one consumer
  // thread, verify every item arrives exactly once and in order.
  {
    constexpr int kItems = 200000;
    SPSCRingBuffer<int, 1024> buffer;
    std::vector<int> received;
    received.reserve(kItems);

    std::thread producer([&buffer] {
      for (int i = 0; i < kItems; ++i) buffer.Push(i);
    });
    std::thread consumer([&buffer, &received] {
      for (int i = 0; i < kItems; ++i) received.push_back(buffer.Pop());
    });
    producer.join();
    consumer.join();

    bool ok = received.size() == static_cast<size_t>(kItems);
    for (int i = 0; ok && i < kItems; ++i) ok = received[static_cast<size_t>(i)] == i;

    std::cout << "[SPSCRingBuffer] concurrent producer/consumer, " << kItems
              << " items in order: " << (ok ? "PASS" : "FAIL") << "\n";
  }

  // 3. MPMCRingBuffer, real concurrency: several producers and consumers.
  // Order isn't guaranteed across producers, but every value produced must
  // be consumed exactly once -- checked by sorting and comparing.
  {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kItemsPerProducer = 50000;
    constexpr int kTotalItems = kProducers * kItemsPerProducer;

    MPMCRingBuffer<int, 1024> buffer;
    std::vector<int> consumed;
    consumed.reserve(kTotalItems);
    std::mutex consumed_mutex; // Guards the test's own result vector only --
                               // not part of the lock-free buffer itself.
    std::atomic<int> consumed_count{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
      producers.emplace_back([&buffer, p] {
        int base = p * kItemsPerProducer;
        for (int i = 0; i < kItemsPerProducer; ++i) buffer.Push(base + i);
      });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
      consumers.emplace_back([&] {
        std::vector<int> local;
        while (consumed_count.load(std::memory_order_relaxed) < kTotalItems) {
          int item;
          if (buffer.TryPop(item)) {
            local.push_back(item);
            consumed_count.fetch_add(1, std::memory_order_relaxed);
          } else {
            std::this_thread::yield();
          }
        }
        std::lock_guard<std::mutex> lock(consumed_mutex);
        consumed.insert(consumed.end(), local.begin(), local.end());
      });
    }

    for (auto &t : producers) t.join();
    for (auto &t : consumers) t.join();

    bool ok = consumed.size() == static_cast<size_t>(kTotalItems);
    std::sort(consumed.begin(), consumed.end());
    for (int i = 0; ok && i < kTotalItems; ++i) ok = consumed[static_cast<size_t>(i)] == i;

    std::cout << "[MPMCRingBuffer] " << kProducers << " producers x "
              << kConsumers << " consumers, " << kTotalItems
              << " items, each exactly once: " << (ok ? "PASS" : "FAIL")
              << "\n";
  }

  // 4. ProducerConsumerSystem: the producer's own callback calls Stop()
  // exactly when it hands out the last item, so RunProducer's item count
  // is exact (it never gets called again once running_ flips) and
  // RunConsumer's producer_done_ handshake guarantees that last item is
  // never lost to the shutdown race described above Stop().
  {
    constexpr int kItems = 100000;
    ProducerConsumerSystem<int> system;
    std::atomic<int> next{0};
    std::atomic<long long> sum{0};

    std::thread producer([&system, &next] {
      system.RunProducer([&system, &next]() -> int {
        int value = next.fetch_add(1, std::memory_order_relaxed);
        if (value == kItems - 1) system.Stop();
        return value;
      });
    });
    std::thread consumer([&system, &sum] {
      system.RunConsumer(
          [&sum](int item) { sum.fetch_add(item, std::memory_order_relaxed); });
    });
    producer.join();
    consumer.join();

    auto stats = system.GetStatistics();
    long long expected_sum = static_cast<long long>(kItems - 1) * kItems / 2;
    bool ok = stats.items_produced_ == static_cast<size_t>(kItems) &&
              stats.items_consumed_ == static_cast<size_t>(kItems) &&
              sum.load() == expected_sum;

    std::cout << "[ProducerConsumerSystem] produced=" << stats.items_produced_
              << " consumed=" << stats.items_consumed_
              << " sum=" << sum.load() << " (expected " << expected_sum
              << "): " << (ok ? "PASS" : "FAIL") << "\n";
  }

  return 0;
}

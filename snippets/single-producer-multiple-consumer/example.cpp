// Single-producer, multiple-consumer lock-free ring buffer (LMAX
// Disruptor style: a per-slot sequence number instead of separate
// head/tail indices). See README.md for the bugs fixed (missing
// includes, a dead local, an unused parameter) -- the core algorithm was
// already correct.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <thread>
#include <vector>

template <typename T, size_t Capacity>
class SPMCQueue {
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t kMask = Capacity - 1;
    static constexpr size_t kCacheLineSize = 64;

    struct alignas(kCacheLineSize) Slot {
        std::atomic<size_t> sequence_{0};
        T data_;
    };

    alignas(kCacheLineSize) std::atomic<size_t> producer_pos_{0};
    alignas(kCacheLineSize) std::atomic<size_t> consumer_cursor_{0};
    alignas(kCacheLineSize) std::array<Slot, Capacity> buffer_;

    alignas(kCacheLineSize) mutable std::atomic<size_t> total_enqueued_{0};
    mutable std::atomic<size_t> total_dequeued_{0};
    mutable std::atomic<size_t> batch_operations_{0};
    mutable std::atomic<size_t> failed_dequeues_{0};

public:
    SPMCQueue() {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        }
    }

    // Single producer, non-blocking.
    template <typename U>
    bool TryEnqueue(U&& item) {
        size_t pos = producer_pos_.load(std::memory_order_relaxed);
        Slot& slot = buffer_[pos & kMask];
        size_t seq = slot.sequence_.load(std::memory_order_acquire);

        if (seq == pos) {
            slot.data_ = std::forward<U>(item);
            slot.sequence_.store(pos + 1, std::memory_order_release);
            producer_pos_.store(pos + 1, std::memory_order_relaxed);

            total_enqueued_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        return false; // Queue is full.
    }

    // Single producer, blocking (spin-wait).
    template <typename U>
    void Enqueue(U&& item) {
        while (!TryEnqueue(std::forward<U>(item))) {
            std::this_thread::yield();
        }
    }

    // Single producer, batch.
    template <typename Iterator>
    size_t TryEnqueueBatch(Iterator begin, Iterator end) {
        size_t count = static_cast<size_t>(std::distance(begin, end));
        if (count == 0) return 0;

        size_t pos = producer_pos_.load(std::memory_order_relaxed);
        size_t enqueued = 0;

        for (auto it = begin; it != end && enqueued < count; ++it, ++enqueued) {
            Slot& slot = buffer_[(pos + enqueued) & kMask];
            size_t seq = slot.sequence_.load(std::memory_order_acquire);

            if (seq != pos + enqueued) {
                break; // Queue is full.
            }

            slot.data_ = *it;
            slot.sequence_.store(pos + enqueued + 1, std::memory_order_release);
        }

        if (enqueued > 0) {
            producer_pos_.store(pos + enqueued, std::memory_order_relaxed);
            total_enqueued_.fetch_add(enqueued, std::memory_order_relaxed);
            batch_operations_.fetch_add(1, std::memory_order_relaxed);
        }

        return enqueued;
    }

    // Multiple consumers, non-blocking.
    bool TryDequeue(T& item) {
        while (true) {
            size_t pos = consumer_cursor_.load(std::memory_order_relaxed);
            Slot& slot = buffer_[pos & kMask];
            size_t seq = slot.sequence_.load(std::memory_order_acquire);

            if (seq == pos + 1) {
                if (consumer_cursor_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    item = std::move(slot.data_);
                    slot.sequence_.store(pos + Capacity, std::memory_order_release);

                    total_dequeued_.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                // Another consumer won the race; retry with the updated pos.
            } else if (seq < pos + 1) {
                failed_dequeues_.fetch_add(1, std::memory_order_relaxed);
                return false; // Queue is empty.
            } else {
                std::this_thread::yield(); // Producer hasn't published yet.
            }
        }
    }

    // Multiple consumers, blocking.
    T Dequeue() {
        T item;
        while (!TryDequeue(item)) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        return item;
    }

    // Multiple consumers, batch.
    template <typename OutputIterator>
    size_t TryDequeueBatch(OutputIterator out, size_t max_count) {
        if (max_count == 0) return 0;

        size_t dequeued = 0;

        while (dequeued < max_count) {
            size_t pos = consumer_cursor_.load(std::memory_order_relaxed);
            Slot& slot = buffer_[pos & kMask];
            size_t seq = slot.sequence_.load(std::memory_order_acquire);

            if (seq == pos + 1) {
                if (consumer_cursor_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    *out++ = std::move(slot.data_);
                    slot.sequence_.store(pos + Capacity, std::memory_order_release);
                    ++dequeued;
                }
            } else {
                break; // No more elements available right now.
            }
        }

        if (dequeued > 0) {
            total_dequeued_.fetch_add(dequeued, std::memory_order_relaxed);
            batch_operations_.fetch_add(1, std::memory_order_relaxed);
        } else {
            failed_dequeues_.fetch_add(1, std::memory_order_relaxed);
        }

        return dequeued;
    }

    bool Empty() const {
        return producer_pos_.load(std::memory_order_relaxed) == consumer_cursor_.load(std::memory_order_relaxed);
    }

    bool Full() const {
        return (producer_pos_.load(std::memory_order_relaxed) - consumer_cursor_.load(std::memory_order_relaxed)) >=
               Capacity;
    }

    size_t Size() const {
        return producer_pos_.load(std::memory_order_relaxed) - consumer_cursor_.load(std::memory_order_relaxed);
    }

    size_t GetCapacity() const { return Capacity; }

    struct Statistics {
        size_t total_enqueued_;
        size_t total_dequeued_;
        size_t batch_operations_;
        size_t failed_dequeues_;
        size_t current_size_;
        double utilization_ratio_;
    };

    Statistics GetStatistics() const {
        size_t enqueued = total_enqueued_.load(std::memory_order_relaxed);
        size_t dequeued = total_dequeued_.load(std::memory_order_relaxed);
        size_t current_size = Size();

        return {enqueued,
                dequeued,
                batch_operations_.load(std::memory_order_relaxed),
                failed_dequeues_.load(std::memory_order_relaxed),
                current_size,
                static_cast<double>(current_size) / static_cast<double>(Capacity)};
    }

    void ResetStatistics() {
        total_enqueued_.store(0, std::memory_order_relaxed);
        total_dequeued_.store(0, std::memory_order_relaxed);
        batch_operations_.store(0, std::memory_order_relaxed);
        failed_dequeues_.store(0, std::memory_order_relaxed);
    }
};

// High-level interface: a queue plus a pool of consumer threads that
// drain it via a processor callback.
template <typename T, size_t Capacity = 1024>
class ManagedSPMCSystem {
private:
    SPMCQueue<T, Capacity> queue_;
    std::vector<std::thread> consumers_;
    std::atomic<bool> running_{true};

    std::function<void(T)> item_processor_;
    std::function<void(std::vector<T>)> batch_processor_;

    void ConsumerLoop(size_t /* consumer_id */, bool use_batching) {
        if (use_batching && batch_processor_) {
            std::vector<T> batch;
            batch.reserve(64);

            while (running_.load(std::memory_order_relaxed)) {
                size_t dequeued = queue_.TryDequeueBatch(std::back_inserter(batch), 64);

                if (dequeued > 0) {
                    batch_processor_(std::move(batch));
                    batch.clear();
                    batch.reserve(64);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }

            T item;
            while (queue_.TryDequeue(item)) {
                batch.push_back(std::move(item));
            }
            if (!batch.empty()) {
                batch_processor_(std::move(batch));
            }

        } else if (item_processor_) {
            T item;
            while (running_.load(std::memory_order_relaxed)) {
                if (queue_.TryDequeue(item)) {
                    item_processor_(std::move(item));
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }

            while (queue_.TryDequeue(item)) {
                item_processor_(std::move(item));
            }
        }
    }

public:
    template <typename ItemProcessor>
    explicit ManagedSPMCSystem(ItemProcessor&& processor,
                                size_t num_consumers = std::thread::hardware_concurrency())
        : item_processor_(std::forward<ItemProcessor>(processor)) {
        StartConsumers(num_consumers, false);
    }

    template <typename BatchProcessor>
    ManagedSPMCSystem(BatchProcessor&& batch_processor, size_t num_consumers, bool /* batch_tag */)
        : batch_processor_(std::forward<BatchProcessor>(batch_processor)) {
        StartConsumers(num_consumers, true);
    }

    ~ManagedSPMCSystem() { Stop(); }

    ManagedSPMCSystem(const ManagedSPMCSystem&) = delete;
    ManagedSPMCSystem& operator=(const ManagedSPMCSystem&) = delete;

    template <typename U>
    bool TryProduce(U&& item) {
        return queue_.TryEnqueue(std::forward<U>(item));
    }

    template <typename U>
    void Produce(U&& item) {
        queue_.Enqueue(std::forward<U>(item));
    }

    template <typename Iterator>
    size_t TryProduceBatch(Iterator begin, Iterator end) {
        return queue_.TryEnqueueBatch(begin, end);
    }

    void Stop() {
        running_.store(false, std::memory_order_relaxed);
        for (auto& consumer : consumers_) {
            if (consumer.joinable()) consumer.join();
        }
        consumers_.clear();
    }

    auto GetStatistics() const { return queue_.GetStatistics(); }

private:
    void StartConsumers(size_t num_consumers, bool use_batching) {
        consumers_.reserve(num_consumers);
        for (size_t i = 0; i < num_consumers; ++i) {
            consumers_.emplace_back([this, i, use_batching] { ConsumerLoop(i, use_batching); });
        }
    }
};

int main() {
    // Exercise the queue directly: one producer, several consumers racing
    // for slots via try_dequeue.
    {
        SPMCQueue<int, 1024> queue;
        constexpr int kItems = 5000;
        constexpr int kConsumers = 4;

        std::thread producer([&queue] {
            for (int i = 0; i < kItems; ++i) queue.Enqueue(i);
        });

        std::atomic<int> consumed{0};
        std::vector<std::thread> consumers;
        for (int c = 0; c < kConsumers; ++c) {
            consumers.emplace_back([&queue, &consumed] {
                int item;
                int local_empty_polls = 0;
                while (consumed.load() < kItems && local_empty_polls < 1000) {
                    if (queue.TryDequeue(item)) {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                        local_empty_polls = 0;
                    } else {
                        ++local_empty_polls;
                        std::this_thread::yield();
                    }
                }
            });
        }

        producer.join();
        for (auto& c : consumers) c.join();

        std::cout << "[SPMCQueue] consumed " << consumed.load() << "/" << kItems << ": "
                  << (consumed.load() == kItems ? "PASS" : "FAIL") << "\n";
    }

    // Exercise ManagedSPMCSystem: a producer pushes items, a pool of
    // consumer threads drains them via a processor callback.
    {
        std::atomic<int> processed{0};
        ManagedSPMCSystem<int, 1024> system([&processed](int) { processed.fetch_add(1, std::memory_order_relaxed); },
                                             4);

        constexpr int kItems = 5000;
        for (int i = 0; i < kItems; ++i) {
            system.Produce(i);
        }

        while (processed.load() < kItems) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        system.Stop();

        std::cout << "[ManagedSPMCSystem] processed " << processed.load() << "/" << kItems << ": "
                  << (processed.load() == kItems ? "PASS" : "FAIL") << "\n";
    }

    return 0;
}

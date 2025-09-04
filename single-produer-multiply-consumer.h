#include <atomic>
#include <memory>
#include <vector>
#include <array>
#include <algorithm>

template<typename T, size_t Capacity>
class SPMCQueue {
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static constexpr size_t MASK = Capacity - 1;
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    struct alignas(CACHE_LINE_SIZE) Slot {
        std::atomic<size_t> sequence{0};
        T data;
    };
    
    // Producer данные (выровнены по кеш-линии)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> producer_pos_{0};
    
    // Consumer данные (выровнены по кеш-линии) 
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> consumer_cursor_{0};
    
    // Буфер слотов
    alignas(CACHE_LINE_SIZE) std::array<Slot, Capacity> buffer_;
    
    // Статистика производительности
    alignas(CACHE_LINE_SIZE) mutable std::atomic<size_t> total_enqueued_{0};
    mutable std::atomic<size_t> total_dequeued_{0};
    mutable std::atomic<size_t> batch_operations_{0};
    mutable std::atomic<size_t> failed_dequeues_{0};
    
public:
    SPMCQueue() {
        // Инициализируем последовательности
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    // Single Producer - неблокирующий enqueue
    template<typename U>
    bool try_enqueue(U&& item) {
        size_t pos = producer_pos_.load(std::memory_order_relaxed);
        Slot& slot = buffer_[pos & MASK];
        size_t seq = slot.sequence.load(std::memory_order_acquire);
        
        if (seq == pos) {
            // Слот готов для записи
            slot.data = std::forward<U>(item);
            slot.sequence.store(pos + 1, std::memory_order_release);
            producer_pos_.store(pos + 1, std::memory_order_relaxed);
            
            total_enqueued_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        return false; // Очередь полна
    }
    
    // Single Producer - блокирующий enqueue с spin-wait
    template<typename U>
    void enqueue(U&& item) {
        while (!try_enqueue(std::forward<U>(item))) {
            std::this_thread::yield();
        }
    }
    
    // Single Producer - batch enqueue
    template<typename Iterator>
    size_t try_enqueue_batch(Iterator begin, Iterator end) {
        size_t count = std::distance(begin, end);
        if (count == 0) return 0;
        
        size_t pos = producer_pos_.load(std::memory_order_relaxed);
        size_t enqueued = 0;
        
        for (auto it = begin; it != end && enqueued < count; ++it, ++enqueued) {
            Slot& slot = buffer_[(pos + enqueued) & MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            
            if (seq != pos + enqueued) {
                break; // Очередь заполнена
            }
            
            slot.data = *it;
            slot.sequence.store(pos + enqueued + 1, std::memory_order_release);
        }
        
        if (enqueued > 0) {
            producer_pos_.store(pos + enqueued, std::memory_order_relaxed);
            total_enqueued_.fetch_add(enqueued, std::memory_order_relaxed);
            batch_operations_.fetch_add(1, std::memory_order_relaxed);
        }
        
        return enqueued;
    }
    
    // Multiple Consumer - неблокирующий dequeue
    bool try_dequeue(T& item) {
        while (true) {
            size_t pos = consumer_cursor_.load(std::memory_order_relaxed);
            Slot& slot = buffer_[pos & MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            
            if (seq == pos + 1) {
                // Данные готовы для чтения
                if (consumer_cursor_.compare_exchange_weak(pos, pos + 1, 
                                                         std::memory_order_relaxed)) {
                    item = std::move(slot.data);
                    slot.sequence.store(pos + Capacity, std::memory_order_release);
                    
                    total_dequeued_.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                // Другой consumer опередил нас, повторяем
            } else if (seq < pos + 1) {
                // Очередь пуста
                failed_dequeues_.fetch_add(1, std::memory_order_relaxed);
                return false;
            } else {
                // Данные еще не готовы, повторяем
                std::this_thread::yield();
            }
        }
    }
    
    // Multiple Consumer - блокирующий dequeue
    T dequeue() {
        T item;
        while (!try_dequeue(item)) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        return item;
    }
    
    // Multiple Consumer - batch dequeue
    template<typename OutputIterator>
    size_t try_dequeue_batch(OutputIterator out, size_t max_count) {
        if (max_count == 0) return 0;
        
        size_t dequeued = 0;
        T item;
        
        // Пытаемся получить эксклюзивный доступ к блоку элементов
        while (dequeued < max_count) {
            size_t pos = consumer_cursor_.load(std::memory_order_relaxed);
            Slot& slot = buffer_[pos & MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            
            if (seq == pos + 1) {
                if (consumer_cursor_.compare_exchange_weak(pos, pos + 1,
                                                         std::memory_order_relaxed)) {
                    *out++ = std::move(slot.data);
                    slot.sequence.store(pos + Capacity, std::memory_order_release);
                    ++dequeued;
                }
            } else {
                break; // Нет доступных элементов
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
    
    // Утилитарные методы
    bool empty() const {
        size_t producer = producer_pos_.load(std::memory_order_relaxed);
        size_t consumer = consumer_cursor_.load(std::memory_order_relaxed);
        return producer == consumer;
    }
    
    bool full() const {
        size_t producer = producer_pos_.load(std::memory_order_relaxed);
        size_t consumer = consumer_cursor_.load(std::memory_order_relaxed);
        return (producer - consumer) >= Capacity;
    }
    
    size_t size() const {
        size_t producer = producer_pos_.load(std::memory_order_relaxed);
        size_t consumer = consumer_cursor_.load(std::memory_order_relaxed);
        return producer - consumer;
    }
    
    size_t capacity() const {
        return Capacity;
    }
    
    // Статистика производительности
    struct Statistics {
        size_t total_enqueued;
        size_t total_dequeued;
        size_t batch_operations;
        size_t failed_dequeues;
        size_t current_size;
        double utilization_ratio;
    };
    
    Statistics get_statistics() const {
        size_t enqueued = total_enqueued_.load(std::memory_order_relaxed);
        size_t dequeued = total_dequeued_.load(std::memory_order_relaxed);
        size_t current_size = size();
        
        return {
            enqueued,
            dequeued,
            batch_operations_.load(std::memory_order_relaxed),
            failed_dequeues_.load(std::memory_order_relaxed),
            current_size,
            static_cast<double>(current_size) / Capacity
        };
    }
    
    void reset_statistics() {
        total_enqueued_.store(0, std::memory_order_relaxed);
        total_dequeued_.store(0, std::memory_order_relaxed);
        batch_operations_.store(0, std::memory_order_relaxed);
        failed_dequeues_.store(0, std::memory_order_relaxed);
    }
};

// Специализация для move-only типов
template<typename T, size_t Capacity>
class SPMCMoveOnlyQueue {
private:
    SPMCQueue<std::unique_ptr<T>, Capacity> queue_;
    
public:
    template<typename... Args>
    bool try_emplace(Args&&... args) {
        auto item = std::make_unique<T>(std::forward<Args>(args)...);
        return queue_.try_enqueue(std::move(item));
    }
    
    template<typename... Args>
    void emplace(Args&&... args) {
        auto item = std::make_unique<T>(std::forward<Args>(args)...);
        queue_.enqueue(std::move(item));
    }
    
    bool try_dequeue(std::unique_ptr<T>& item) {
        return queue_.try_dequeue(item);
    }
    
    std::unique_ptr<T> dequeue() {
        return queue_.dequeue();
    }
    
    bool empty() const { return queue_.empty(); }
    bool full() const { return queue_.full(); }
    size_t size() const { return queue_.size(); }
    
    auto get_statistics() const { return queue_.get_statistics(); }
    void reset_statistics() { queue_.reset_statistics(); }
};

// Высокоуровневый интерфейс с автоматическим управлением потоками
template<typename T, size_t Capacity = 1024>
class ManagedSPMCSystem {
private:
    SPMCQueue<T, Capacity> queue_;
    std::vector<std::thread> consumers_;
    std::atomic<bool> running_{true};
    
    // Функциональные объекты для обработки
    std::function<void(T)> item_processor_;
    std::function<void(std::vector<T>)> batch_processor_;
    
    void consumer_loop(size_t consumer_id, bool use_batching) {
        if (use_batching && batch_processor_) {
            std::vector<T> batch;
            batch.reserve(64);
            
            while (running_.load(std::memory_order_relaxed)) {
                size_t dequeued = queue_.try_dequeue_batch(
                    std::back_inserter(batch), 64);
                
                if (dequeued > 0) {
                    batch_processor_(std::move(batch));
                    batch.clear();
                    batch.reserve(64);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            
            // Обрабатываем оставшиеся элементы
            T item;
            while (queue_.try_dequeue(item)) {
                batch.push_back(std::move(item));
            }
            if (!batch.empty()) {
                batch_processor_(std::move(batch));
            }
            
        } else if (item_processor_) {
            T item;
            while (running_.load(std::memory_order_relaxed)) {
                if (queue_.try_dequeue(item)) {
                    item_processor_(std::move(item));
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            
            // Обрабатываем оставшиеся элементы
            while (queue_.try_dequeue(item)) {
                item_processor_(std::move(item));
            }
        }
    }
    
public:
    template<typename ItemProcessor>
    explicit ManagedSPMCSystem(ItemProcessor&& processor, 
                              size_t num_consumers = std::thread::hardware_concurrency())
        : item_processor_(std::forward<ItemProcessor>(processor)) {
        
        start_consumers(num_consumers, false);
    }
    
    template<typename BatchProcessor>
    ManagedSPMCSystem(BatchProcessor&& batch_processor, 
                     size_t num_consumers,
                     bool /* batch_tag */)
        : batch_processor_(std::forward<BatchProcessor>(batch_processor)) {
        
        start_consumers(num_consumers, true);
    }
    
    ~ManagedSPMCSystem() {
        stop();
    }
    
    template<typename U>
    bool try_produce(U&& item) {
        return queue_.try_enqueue(std::forward<U>(item));
    }
    
    template<typename U>
    void produce(U&& item) {
        queue_.enqueue(std::forward<U>(item));
    }
    
    template<typename Iterator>
    size_t try_produce_batch(Iterator begin, Iterator end) {
        return queue_.try_enqueue_batch(begin, end);
    }
    
    void stop() {
        running_.store(false, std::memory_order_relaxed);
        
        for (auto& consumer : consumers_) {
            if (consumer.joinable()) {
                consumer.join();
            }
        }
        consumers_.clear();
    }
    
    auto get_statistics() const {
        return queue_.get_statistics();
    }
    
private:
    void start_consumers(size_t num_consumers, bool use_batching) {
        consumers_.reserve(num_consumers);
        for (size_t i = 0; i < num_consumers; ++i) {
            consumers_.emplace_back([this, i, use_batching]() {
                consumer_loop(i, use_batching);
            });
        }
    }
};

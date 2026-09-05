#include <atomic>
#include <memory>
#include <array>
#include <type_traits>

template<typename T, size_t Size>
class SPSCRingBuffer {
private:
    static_assert(Size > 0 && (Size & (Size - 1)) == 0, "Size must be power of 2");
    
    struct alignas(64) Slot {
        std::atomic<size_t> sequence{0};
        T data;
    };
    
    alignas(64) std::array<Slot, Size> buffer;
    alignas(64) std::atomic<size_t> write_pos{0};
    alignas(64) std::atomic<size_t> read_pos{0};
    
    static constexpr size_t INDEX_MASK = Size - 1;
    
public:
    SPSCRingBuffer() {
        // Инициализируем последовательности
        for (size_t i = 0; i < Size; ++i) {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    // Неблокирующая запись (только для одного producer'а)
    template<typename U>
    bool try_push(U&& item) {
        size_t pos = write_pos.load(std::memory_order_relaxed);
        Slot& slot = buffer[pos & INDEX_MASK];
        size_t seq = slot.sequence.load(std::memory_order_acquire);
        
        // Проверяем, можем ли записать в этот слот
        if (seq == pos) {
            // Записываем данные
            if constexpr (std::is_move_constructible_v<T> && !std::is_copy_constructible_v<T>) {
                slot.data = std::forward<U>(item);
            } else {
                slot.data = item;
            }
            
            // Обновляем последовательность и позицию записи
            slot.sequence.store(pos + 1, std::memory_order_release);
            write_pos.store(pos + 1, std::memory_order_relaxed);
            
            return true;
        }
        
        return false; // Буфер полон
    }
    
    // Блокирующая запись с spin-wait
    template<typename U>
    void push(U&& item) {
        while (!try_push(std::forward<U>(item))) {
            std::this_thread::yield();
        }
    }
    
    // Неблокирующее чтение (только для одного consumer'а)
    bool try_pop(T& item) {
        size_t pos = read_pos.load(std::memory_order_relaxed);
        Slot& slot = buffer[pos & INDEX_MASK];
        size_t seq = slot.sequence.load(std::memory_order_acquire);
        
        // Проверяем, можем ли прочитать из этого слота
        if (seq == pos + 1) {
            // Читаем данные
            if constexpr (std::is_move_constructible_v<T>) {
                item = std::move(slot.data);
            } else {
                item = slot.data;
            }
            
            // Обновляем последовательность и позицию чтения
            slot.sequence.store(pos + Size, std::memory_order_release);
            read_pos.store(pos + 1, std::memory_order_relaxed);
            
            return true;
        }
        
        return false; // Буфер пуст
    }
    
    // Блокирующее чтение с spin-wait
    T pop() {
        T item;
        while (!try_pop(item)) {
            std::this_thread::yield();
        }
        return item;
    }
    
    // Проверки состояния (приблизительные из-за отсутствия синхронизации)
    bool empty() const {
        size_t write = write_pos.load(std::memory_order_relaxed);
        size_t read = read_pos.load(std::memory_order_relaxed);
        return write == read;
    }
    
    bool full() const {
        size_t write = write_pos.load(std::memory_order_relaxed);
        size_t read = read_pos.load(std::memory_order_relaxed);
        return (write - read) >= Size;
    }
    
    size_t size() const {
        size_t write = write_pos.load(std::memory_order_relaxed);
        size_t read = read_pos.load(std::memory_order_relaxed);
        return write - read;
    }
};

// Расширенная версия с multiple producers/consumers
template<typename T, size_t Size>
class MPMCRingBuffer {
private:
    static_assert(Size > 0 && (Size & (Size - 1)) == 0, "Size must be power of 2");
    
    struct alignas(64) Slot {
        std::atomic<size_t> sequence{0};
        T data;
    };
    
    alignas(64) std::array<Slot, Size> buffer;
    alignas(64) std::atomic<size_t> write_pos{0};
    alignas(64) std::atomic<size_t> read_pos{0};
    
    static constexpr size_t INDEX_MASK = Size - 1;
    
public:
    MPMCRingBuffer() {
        for (size_t i = 0; i < Size; ++i) {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
    
    template<typename U>
    bool try_push(U&& item) {
        while (true) {
            size_t pos = write_pos.load(std::memory_order_relaxed);
            Slot& slot = buffer[pos & INDEX_MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            
            if (seq == pos) {
                // Пытаемся захватить позицию
                if (write_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    // Записываем данные
                    slot.data = std::forward<U>(item);
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (seq < pos) {
                // Буфер полон
                return false;
            }
            
            // Другой поток опередил нас, повторяем попытку
            std::this_thread::yield();
        }
    }
    
    bool try_pop(T& item) {
        while (true) {
            size_t pos = read_pos.load(std::memory_order_relaxed);
            Slot& slot = buffer[pos & INDEX_MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            
            if (seq == pos + 1) {
                // Пытаемся захватить позицию
                if (read_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    // Читаем данные
                    if constexpr (std::is_move_constructible_v<T>) {
                        item = std::move(slot.data);
                    } else {
                        item = slot.data;
                    }
                    slot.sequence.store(pos + Size, std::memory_order_release);
                    return true;
                }
            } else if (seq < pos + 1) {
                // Буфер пуст
                return false;
            }
            
            // Другой поток опередил нас, повторяем попытку
            std::this_thread::yield();
        }
    }
    
    template<typename U>
    void push(U&& item) {
        while (!try_push(std::forward<U>(item))) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    
    T pop() {
        T item;
        while (!try_pop(item)) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        return item;
    }
};

// Производитель-потребитель с метриками производительности
template<typename T>
class ProducerConsumerSystem {
private:
    SPSCRingBuffer<T, 1024> buffer;
    std::atomic<bool> running{true};
    std::atomic<size_t> items_produced{0};
    std::atomic<size_t> items_consumed{0};
    std::atomic<size_t> producer_blocks{0};
    std::atomic<size_t> consumer_blocks{0};
    
public:
    template<typename Producer>
    void run_producer(Producer&& producer) {
        while (running.load(std::memory_order_relaxed)) {
            auto item = producer();
            
            if (!buffer.try_push(std::move(item))) {
                producer_blocks.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            } else {
                items_produced.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    
    template<typename Consumer>
    void run_consumer(Consumer&& consumer) {
        T item;
        while (running.load(std::memory_order_relaxed)) {
            if (buffer.try_pop(item)) {
                consumer(std::move(item));
                items_consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                consumer_blocks.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        
        // Обрабатываем оставшиеся элементы
        while (buffer.try_pop(item)) {
            consumer(std::move(item));
            items_consumed.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    void stop() {
        running.store(false, std::memory_order_relaxed);
    }
    
    struct Statistics {
        size_t items_produced;
        size_t items_consumed;
        size_t producer_blocks;
        size_t consumer_blocks;
        size_t buffer_size;
    };
    
    Statistics get_statistics() const {
        return {
            items_produced.load(std::memory_order_relaxed),
            items_consumed.load(std::memory_order_relaxed),
            producer_blocks.load(std::memory_order_relaxed),
            consumer_blocks.load(std::memory_order_relaxed),
            buffer.size()
        };
    }
};

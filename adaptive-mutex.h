#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>

class AdaptiveMutex {
private:
    enum class State : uint32_t {
        UNLOCKED = 0,
        LOCKED = 1,
        LOCKED_WITH_WAITERS = 2
    };
    
    std::atomic<uint32_t> state_{static_cast<uint32_t>(State::UNLOCKED)};
    
    // Статистика для адаптации
    mutable std::atomic<uint64_t> total_acquisitions_{0};
    mutable std::atomic<uint64_t> spin_acquisitions_{0};
    mutable std::atomic<uint64_t> sleep_acquisitions_{0};
    mutable std::atomic<uint64_t> total_contention_time_{0}; // в наносекундах
    
    // Адаптивные параметры
    mutable std::atomic<uint32_t> spin_limit_{1000};
    mutable std::atomic<uint32_t> base_sleep_duration_{1}; // в микросекундах
    
    // Для обновления параметров
    mutable std::atomic<uint64_t> last_adaptation_time_{0};
    static constexpr uint64_t ADAPTATION_INTERVAL_NS = 1000000000; // 1 секунда
    
    // Thread-local статистика
    thread_local static uint64_t local_spin_count_;
    thread_local static std::mt19937 rng_;
    
    void adapt_parameters() const {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        uint64_t last_time = last_adaptation_time_.load(std::memory_order_relaxed);
        
        if (now - last_time < ADAPTATION_INTERVAL_NS) {
            return;
        }
        
        if (!last_adaptation_time_.compare_exchange_weak(last_time, now, 
                                                        std::memory_order_relaxed)) {
            return; // Другой поток уже адаптирует
        }
        
        uint64_t total = total_acquisitions_.load(std::memory_order_relaxed);
        uint64_t spin_success = spin_acquisitions_.load(std::memory_order_relaxed);
        uint64_t sleep_success = sleep_acquisitions_.load(std::memory_order_relaxed);
        uint64_t avg_contention = total > 0 ? 
            total_contention_time_.load(std::memory_order_relaxed) / total : 0;
        
        if (total < 100) {
            return; // Недостаточно данных для адаптации
        }
        
        // Адаптируем spin_limit
        double spin_success_rate = static_cast<double>(spin_success) / total;
        
        if (spin_success_rate > 0.8) {
            // Высокий успех при спине - можем увеличить лимит
            uint32_t current_limit = spin_limit_.load(std::memory_order_relaxed);
            uint32_t new_limit = std::min(current_limit * 2, 10000u);
            spin_limit_.store(new_limit, std::memory_order_relaxed);
        } else if (spin_success_rate < 0.3) {
            // Низкий успех при спине - уменьшаем лимит
            uint32_t current_limit = spin_limit_.load(std::memory_order_relaxed);
            uint32_t new_limit = std::max(current_limit / 2, 100u);
            spin_limit_.store(new_limit, std::memory_order_relaxed);
        }
        
        // Адаптируем время сна
        if (avg_contention > 100000) { // > 100 микросекунд
            uint32_t current_sleep = base_sleep_duration_.load(std::memory_order_relaxed);
            uint32_t new_sleep = std::min(current_sleep * 2, 1000u);
            base_sleep_duration_.store(new_sleep, std::memory_order_relaxed);
        } else if (avg_contention < 10000) { // < 10 микросекунд
            uint32_t current_sleep = base_sleep_duration_.load(std::memory_order_relaxed);
            uint32_t new_sleep = std::max(current_sleep / 2, 1u);
            base_sleep_duration_.store(new_sleep, std::memory_order_relaxed);
        }
        
        // Сбрасываем счетчики для следующего периода
        total_acquisitions_.store(0, std::memory_order_relaxed);
        spin_acquisitions_.store(0, std::memory_order_relaxed);
        sleep_acquisitions_.store(0, std::memory_order_relaxed);
        total_contention_time_.store(0, std::memory_order_relaxed);
    }
    
    bool try_spin_lock() const {
        uint32_t expected = static_cast<uint32_t>(State::UNLOCKED);
        return state_.compare_exchange_weak(expected, static_cast<uint32_t>(State::LOCKED),
                                          std::memory_order_acquire, 
                                          std::memory_order_relaxed);
    }
    
    void exponential_backoff(uint32_t& backoff_count) const {
        // Экспоненциальный backoff с рандомизацией
        uint32_t delay = std::min(1u << std::min(backoff_count, 10u), 1024u);
        uint32_t jitter = rng_() % (delay + 1);
        
        for (uint32_t i = 0; i < delay + jitter; ++i) {
            std::this_thread::yield();
        }
        
        ++backoff_count;
    }
    
public:
    AdaptiveMutex() = default;
    ~AdaptiveMutex() = default;
    
    AdaptiveMutex(const AdaptiveMutex&) = delete;
    AdaptiveMutex& operator=(const AdaptiveMutex&) = delete;
    
    void lock() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Фаза 1: Оптимистичная попытка без спина
        if (try_spin_lock()) {
            total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        
        // Фаза 2: Адаптивный спин
        uint32_t spin_count = 0;
        uint32_t spin_limit = spin_limit_.load(std::memory_order_relaxed);
        uint32_t backoff_count = 0;
        
        while (spin_count < spin_limit) {
            // Проверяем состояние перед спином
            uint32_t current_state = state_.load(std::memory_order_relaxed);
            if (current_state == static_cast<uint32_t>(State::UNLOCKED)) {
                if (try_spin_lock()) {
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end_time - start_time).count();
                    
                    total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
                    spin_acquisitions_.fetch_add(1, std::memory_order_relaxed);
                    total_contention_time_.fetch_add(duration, std::memory_order_relaxed);
                    
                    // Периодически адаптируем параметры
                    if ((total_acquisitions_.load() & 0xFF) == 0) {
                        adapt_parameters();
                    }
                    
                    return;
                }
            }
            
            exponential_backoff(backoff_count);
            ++spin_count;
            ++local_spin_count_;
        }
        
        // Фаза 3: Переходим к блокирующему ожиданию
        uint32_t sleep_duration = base_sleep_duration_.load(std::memory_order_relaxed);
        uint32_t sleep_backoff = 0;
        
        while (true) {
            // Устанавливаем состояние "заблокирован с ожидающими"
            uint32_t expected = static_cast<uint32_t>(State::UNLOCKED);
            if (state_.compare_exchange_weak(expected, static_cast<uint32_t>(State::LOCKED),
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed)) {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time - start_time).count();
                
                total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
                sleep_acquisitions_.fetch_add(1, std::memory_order_relaxed);
                total_contention_time_.fetch_add(duration, std::memory_order_relaxed);
                return;
            }
            
            // Помечаем, что есть ожидающие потоки
            if (expected == static_cast<uint32_t>(State::LOCKED)) {
                state_.compare_exchange_weak(expected, 
                    static_cast<uint32_t>(State::LOCKED_WITH_WAITERS),
                    std::memory_order_relaxed, std::memory_order_relaxed);
            }
            
            // Спим с экспоненциальным увеличением времени
            uint32_t actual_sleep = sleep_duration * (1 << std::min(sleep_backoff, 6u));
            std::this_thread::sleep_for(std::chrono::microseconds(actual_sleep));
            ++sleep_backoff;
        }
    }
    
    bool try_lock() {
        bool success = try_spin_lock();
        if (success) {
            total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
        }
        return success;
    }
    
    template<typename Rep, typename Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        auto start_time = std::chrono::steady_clock::now();
        auto timeout_point = start_time + timeout_duration;
        
        // Сначала пытаемся как обычный lock, но с ограничением по времени
        if (try_spin_lock()) {
            total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        uint32_t backoff_count = 0;
        
        while (std::chrono::steady_clock::now() < timeout_point) {
            if (try_spin_lock()) {
                total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            
            exponential_backoff(backoff_count);
        }
        
        return false;
    }
    
    void unlock() {
        uint32_t expected = static_cast<uint32_t>(State::LOCKED);
        
        // Простое освобождение если нет ожидающих
        if (state_.compare_exchange_strong(expected, static_cast<uint32_t>(State::UNLOCKED),
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
            return;
        }
        
        // Есть ожидающие потоки
        expected = static_cast<uint32_t>(State::LOCKED_WITH_WAITERS);
        if (state_.compare_exchange_strong(expected, static_cast<uint32_t>(State::UNLOCKED),
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
            // Можно уведомить ожидающие потоки, но в этой реализации
            // они сами переопросят состояние
            return;
        }
        
        // Состояние изменилось во время unlock - это не должно происходить
        // В отладочной версии можно добавить assert
    }
    
    // Статистика производительности
    struct MutexStatistics {
        uint64_t total_acquisitions;
        uint64_t spin_acquisitions;
        uint64_t sleep_acquisitions;
        uint64_t average_contention_time_ns;
        uint32_t current_spin_limit;
        uint32_t current_sleep_duration;
        uint64_t thread_local_spins;
    };
    
    MutexStatistics get_statistics() const {
        uint64_t total = total_acquisitions_.load(std::memory_order_relaxed);
        uint64_t contention_time = total_contention_time_.load(std::memory_order_relaxed);
        
        return {
            total,
            spin_acquisitions_.load(std::memory_order_relaxed),
            sleep_acquisitions_.load(std::memory_order_relaxed),
            total > 0 ? contention_time / total : 0,
            spin_limit_.load(std::memory_order_relaxed),
            base_sleep_duration_.load(std::memory_order_relaxed),
            local_spin_count_
        };
    }
    
    void reset_statistics() {
        total_acquisitions_.store(0, std::memory_order_relaxed);
        spin_acquisitions_.store(0, std::memory_order_relaxed);
        sleep_acquisitions_.store(0, std::memory_order_relaxed);
        total_contention_time_.store(0, std::memory_order_relaxed);
        local_spin_count_ = 0;
    }
};

// Статические thread_local переменные
thread_local uint64_t AdaptiveMutex::local_spin_count_ = 0;
thread_local std::mt19937 AdaptiveMutex::rng_{std::random_device{}()};

// RAII lock guard для AdaptiveMutex
class AdaptiveLockGuard {
private:
    AdaptiveMutex& mutex_;
    
public:
    explicit AdaptiveLockGuard(AdaptiveMutex& mutex) : mutex_(mutex) {
        mutex_.lock();
    }
    
    ~AdaptiveLockGuard() {
        mutex_.unlock();
    }
    
    AdaptiveLockGuard(const AdaptiveLockGuard&) = delete;
    AdaptiveLockGuard& operator=(const AdaptiveLockGuard&) = delete;
};

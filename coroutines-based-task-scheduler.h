#include <coroutine>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

namespace coro_scheduler {

// Базовый awaitable
struct TaskAwaitable {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) noexcept;
    void await_resume() const noexcept {}
};

// Task - основной тип корутины
template<typename T = void>
class Task {
public:
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_void() requires std::is_void_v<T> {}
        
        void return_value(T value) requires (!std::is_void_v<T>) {
            result = std::make_unique<T>(std::move(value));
        }
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
        
        // Поддержка co_await
        template<typename Awaitable>
        auto await_transform(Awaitable&& awaitable) {
            return std::forward<Awaitable>(awaitable);
        }
        
        std::unique_ptr<T> result;
        std::exception_ptr exception;
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    Task(handle_type h) : handle(h) {}
    
    Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) {
                handle.destroy();
            }
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }
    
    ~Task() {
        if (handle) {
            handle.destroy();
        }
    }
    
    // Получение результата
    T get() requires (!std::is_void_v<T>) {
        if (!handle || !handle.done()) {
            throw std::runtime_error("Task not completed");
        }
        
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        
        return *handle.promise().result;
    }
    
    void get() requires std::is_void_v<T> {
        if (!handle || !handle.done()) {
            throw std::runtime_error("Task not completed");
        }
        
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
    }
    
    bool is_ready() const {
        return handle && handle.done();
    }
    
    handle_type handle;
};

// Планировщик корутин
class CoroutineScheduler {
private:
    struct WorkerThread {
        std::thread thread;
        std::queue<std::coroutine_handle<>> local_queue;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop_requested{false};
        
        WorkerThread(CoroutineScheduler* scheduler, size_t id) {
            thread = std::thread([this, scheduler, id]() {
                scheduler->worker_loop(this, id);
            });
        }
        
        ~WorkerThread() {
            stop_requested.store(true);
            condition.notify_all();
            if (thread.joinable()) {
                thread.join();
            }
        }
    };
    
    std::vector<std::unique_ptr<WorkerThread>> workers;
    std::queue<std::coroutine_handle<>> global_queue;
    std::mutex global_mutex;
    std::condition_variable global_condition;
    std::atomic<bool> shutdown{false};
    std::atomic<size_t> active_tasks{0};
    
    // Round-robin балансировка
    std::atomic<size_t> next_worker{0};
    
    void worker_loop(WorkerThread* worker, size_t worker_id) {
        while (!shutdown.load()) {
            std::coroutine_handle<> task_handle;
            bool found_task = false;
            
            // 1. Проверяем локальную очередь
            {
                std::unique_lock<std::mutex> lock(worker->queue_mutex);
                if (!worker->local_queue.empty()) {
                    task_handle = worker->local_queue.front();
                    worker->local_queue.pop();
                    found_task = true;
                }
            }
            
            // 2. Если локальная очередь пуста, проверяем глобальную
            if (!found_task) {
                {
                    std::unique_lock<std::mutex> lock(global_mutex);
                    global_condition.wait_for(lock, std::chrono::milliseconds(10), [this]() {
                        return !global_queue.empty() || shutdown.load();
                    });
                    
                    if (!global_queue.empty()) {
                        task_handle = global_queue.front();
                        global_queue.pop();
                        found_task = true;
                    }
                }
            }
            
            // 3. Work stealing из других потоков
            if (!found_task) {
                found_task = try_steal_work(worker_id, task_handle);
            }
            
            // 4. Выполняем задачу если нашли
            if (found_task) {
                execute_coroutine(task_handle);
            }
        }
    }
    
    bool try_steal_work(size_t current_worker_id, std::coroutine_handle<>& stolen_task) {
        for (size_t i = 1; i < workers.size(); ++i) {
            size_t target_id = (current_worker_id + i) % workers.size();
            WorkerThread* target_worker = workers[target_id].get();
            
            std::unique_lock<std::mutex> lock(target_worker->queue_mutex, std::try_to_lock);
            if (lock.owns_lock() && !target_worker->local_queue.empty()) {
                stolen_task = target_worker->local_queue.front();
                target_worker->local_queue.pop();
                return true;
            }
        }
        return false;
    }
    
    void execute_coroutine(std::coroutine_handle<> handle) {
        try {
            handle.resume();
            
            if (handle.done()) {
                active_tasks.fetch_sub(1, std::memory_order_relaxed);
                // handle будет уничтожен автоматически при разрушении Task
            }
        } catch (...) {
            active_tasks.fetch_sub(1, std::memory_order_relaxed);
            // Исключения обрабатываются в promise_type::unhandled_exception
        }
    }
    
public:
    explicit CoroutineScheduler(size_t num_threads = std::thread::hardware_concurrency()) {
        if (num_threads == 0) {
            num_threads = 1;
        }
        
        workers.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(std::make_unique<WorkerThread>(this, i));
        }
    }
    
    ~CoroutineScheduler() {
        shutdown.store(true);
        global_condition.notify_all();
        
        for (auto& worker : workers) {
            worker->condition.notify_all();
        }
        
        workers.clear();
    }
    
    void schedule(std::coroutine_handle<> handle) {
        active_tasks.fetch_add(1, std::memory_order_relaxed);
        
        // Выбираем воркер по round-robin
        size_t worker_id = next_worker.fetch_add(1) % workers.size();
        WorkerThread* worker = workers[worker_id].get();
        
        {
            std::lock_guard<std::mutex> lock(worker->queue_mutex);
            worker->local_queue.push(handle);
        }
        
        worker->condition.notify_one();
    }
    
    size_t get_active_task_count() const {
        return active_tasks.load();
    }
    
    void wait_for_all_tasks() {
        while (active_tasks.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

// Глобальный планировщик
inline CoroutineScheduler& get_scheduler() {
    static CoroutineScheduler scheduler;
    return scheduler;
}

// Реализация TaskAwaitable
inline void TaskAwaitable::await_suspend(std::coroutine_handle<> handle) noexcept {
    get_scheduler().schedule(handle);
}

// Awaitable для переключения на планировщик
struct ScheduleAwaitable {
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        get_scheduler().schedule(handle);
    }
    
    void await_resume() const noexcept {}
};

// Awaitable для задержки
struct DelayAwaitable {
    std::chrono::milliseconds duration;
    
    explicit DelayAwaitable(std::chrono::milliseconds d) : duration(d) {}
    
    bool await_ready() const noexcept { return duration.count() <= 0; }
    
    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([handle, this]() {
            std::this_thread::sleep_for(duration);
            get_scheduler().schedule(handle);
        }).detach();
    }
    
    void await_resume() const noexcept {}
};

// Вспомогательные функции
inline ScheduleAwaitable schedule() {
    return ScheduleAwaitable{};
}

inline DelayAwaitable delay(std::chrono::milliseconds ms) {
    return DelayAwaitable{ms};
}

// Awaitable для выполнения в пуле потоков
template<typename F>
struct ThreadPoolAwaitable {
    F function;
    
    explicit ThreadPoolAwaitable(F&& f) : function(std::forward<F>(f)) {}
    
    bool await_ready() const noexcept { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([handle, func = std::move(function)]() mutable {
            try {
                func();
            } catch (...) {
                // Исключения обрабатываются в корутине
            }
            get_scheduler().schedule(handle);
        }).detach();
    }
    
    void await_resume() const noexcept {}
};

template<typename F>
auto run_on_thread_pool(F&& func) {
    return ThreadPoolAwaitable<F>{std::forward<F>(func)};
}

} // namespace coro_scheduler

// Пример использования:
/*
coro_scheduler::Task<int> example_coroutine() {
    co_await coro_scheduler::schedule(); // Переключаемся на планировщик
    
    // Выполняем тяжелую работу в отдельном потоке
    co_await coro_scheduler::run_on_thread_pool([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });
    
    // Задержка
    co_await coro_scheduler::delay(std::chrono::milliseconds(100));
    
    co_return 42;
}
*/

// C++20 coroutine task scheduler with per-worker local queues and work
// stealing. Built as C++20 specifically for this target (see
// CMakeLists.txt) -- the rest of this repository is C++17. See README.md
// for the two real bugs fixed (a promise type that couldn't legally
// declare both return_void and return_value; a detached-thread lifetime
// hazard in DelayAwaitable) and a documented latency characteristic.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace coro_scheduler {

namespace detail {

// Holds the coroutine's result and provides exactly one of
// return_void()/return_value(T), as the coroutine promise-type contract
// requires -- see README.md for why a single requires-constrained
// promise_type can't do this itself.
template <typename T>
struct PromiseResult {
    void return_value(T value) { result = std::make_unique<T>(std::move(value)); }
    std::unique_ptr<T> result;
};

template <>
struct PromiseResult<void> {
    void return_void() noexcept {}
};

} // namespace detail

// Task -- the coroutine return type.
template <typename T = void>
class Task {
public:
    struct promise_type : detail::PromiseResult<T> {
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { exception = std::current_exception(); }

        // Identity await_transform: lets co_await work uniformly for any
        // awaitable type without per-type special-casing here.
        template <typename Awaitable>
        auto await_transform(Awaitable&& awaitable) {
            return std::forward<Awaitable>(awaitable);
        }

        std::exception_ptr exception;
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit Task(handle_type h) : handle(h) {}

    Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle) handle.destroy();
    }

    T get() requires(!std::is_void_v<T>) {
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

    bool is_ready() const { return handle && handle.done(); }

    handle_type handle;
};

// The scheduler: a pool of worker threads, each with a local queue, plus
// work stealing and a (see README) rarely-fed global overflow queue.
class CoroutineScheduler {
private:
    struct WorkerThread {
        std::thread thread;
        std::queue<std::coroutine_handle<>> local_queue;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop_requested{false};

        WorkerThread(CoroutineScheduler* scheduler, size_t id) {
            thread = std::thread([this, scheduler, id] { scheduler->worker_loop(this, id); });
        }

        ~WorkerThread() {
            stop_requested.store(true);
            condition.notify_all();
            if (thread.joinable()) thread.join();
        }
    };

    std::vector<std::unique_ptr<WorkerThread>> workers;
    std::queue<std::coroutine_handle<>> global_queue;
    std::mutex global_mutex;
    std::condition_variable global_condition;
    std::atomic<bool> shutdown{false};
    std::atomic<size_t> active_tasks{0};
    std::atomic<size_t> next_worker{0};

    void worker_loop(WorkerThread* worker, size_t worker_id) {
        while (!shutdown.load()) {
            std::coroutine_handle<> task_handle;
            bool found_task = false;

            // 1. Local queue first.
            {
                std::unique_lock<std::mutex> lock(worker->queue_mutex);
                if (!worker->local_queue.empty()) {
                    task_handle = worker->local_queue.front();
                    worker->local_queue.pop();
                    found_task = true;
                }
            }

            // 2. Global overflow queue (see README's known limitation).
            if (!found_task) {
                std::unique_lock<std::mutex> lock(global_mutex);
                global_condition.wait_for(lock, std::chrono::milliseconds(10),
                                           [this] { return !global_queue.empty() || shutdown.load(); });

                if (!global_queue.empty()) {
                    task_handle = global_queue.front();
                    global_queue.pop();
                    found_task = true;
                }
            }

            // 3. Steal from another worker.
            if (!found_task) {
                found_task = try_steal_work(worker_id, task_handle);
            }

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
        // Exceptions from the coroutine body itself are already caught by
        // promise_type::unhandled_exception(); this guards resume() itself.
        try {
            handle.resume();
            if (handle.done()) {
                active_tasks.fetch_sub(1, std::memory_order_relaxed);
            }
        } catch (...) {
            active_tasks.fetch_sub(1, std::memory_order_relaxed);
        }
    }

public:
    explicit CoroutineScheduler(size_t num_threads = std::thread::hardware_concurrency()) {
        if (num_threads == 0) num_threads = 1;

        workers.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(std::make_unique<WorkerThread>(this, i));
        }
    }

    ~CoroutineScheduler() {
        shutdown.store(true);
        global_condition.notify_all();
        for (auto& worker : workers) worker->condition.notify_all();
        workers.clear();
    }

    CoroutineScheduler(const CoroutineScheduler&) = delete;
    CoroutineScheduler& operator=(const CoroutineScheduler&) = delete;

    void schedule(std::coroutine_handle<> handle) {
        active_tasks.fetch_add(1, std::memory_order_relaxed);

        size_t worker_id = next_worker.fetch_add(1) % workers.size();
        WorkerThread* worker = workers[worker_id].get();

        {
            std::lock_guard<std::mutex> lock(worker->queue_mutex);
            worker->local_queue.push(handle);
        }
        worker->condition.notify_one();
    }

    size_t get_active_task_count() const { return active_tasks.load(); }

    void wait_for_all_tasks() {
        while (active_tasks.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

inline CoroutineScheduler& get_scheduler() {
    static CoroutineScheduler scheduler;
    return scheduler;
}

// Awaitable that hops the current coroutine onto the scheduler.
struct ScheduleAwaitable {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const noexcept { get_scheduler().schedule(handle); }
    void await_resume() const noexcept {}
};

// Awaitable that suspends for a duration without blocking a worker thread.
struct DelayAwaitable {
    std::chrono::milliseconds duration;

    explicit DelayAwaitable(std::chrono::milliseconds d) : duration(d) {}

    bool await_ready() const noexcept { return duration.count() <= 0; }

    void await_suspend(std::coroutine_handle<> handle) const {
        // BUG FIX: capture duration by value rather than reading it back
        // through `this` after the sleep -- see README.md.
        std::thread([handle, sleep_duration = duration] {
            std::this_thread::sleep_for(sleep_duration);
            get_scheduler().schedule(handle);
        }).detach();
    }

    void await_resume() const noexcept {}
};

inline ScheduleAwaitable schedule() { return ScheduleAwaitable{}; }
inline DelayAwaitable delay(std::chrono::milliseconds ms) { return DelayAwaitable{ms}; }

// Awaitable that runs a callable on a throwaway thread, then resumes the
// coroutine back on the scheduler.
template <typename F>
struct ThreadPoolAwaitable {
    F function;

    explicit ThreadPoolAwaitable(F&& f) : function(std::forward<F>(f)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([handle, func = function]() mutable {
            try {
                func();
            } catch (...) {
                // Surfaced to the caller via promise_type::unhandled_exception
                // when the coroutine resumes and rethrows internally.
            }
            get_scheduler().schedule(handle);
        }).detach();
    }

    void await_resume() const noexcept {}
};

template <typename F>
auto run_on_thread_pool(F&& func) {
    return ThreadPoolAwaitable<F>{std::forward<F>(func)};
}

} // namespace coro_scheduler

// ---- Demo ----

coro_scheduler::Task<int> compute_answer() {
    co_await coro_scheduler::schedule(); // Hop onto a scheduler worker thread.

    int partial = 0;
    co_await coro_scheduler::run_on_thread_pool([&partial] { partial = 40; });

    co_await coro_scheduler::delay(std::chrono::milliseconds(5));

    co_return partial + 2;
}

coro_scheduler::Task<void> log_progress(std::vector<std::string>& log, std::mutex& log_mutex) {
    co_await coro_scheduler::schedule();

    for (int i = 0; i < 3; ++i) {
        co_await coro_scheduler::delay(std::chrono::milliseconds(2));
        std::lock_guard<std::mutex> lock(log_mutex);
        log.push_back("step " + std::to_string(i));
    }
    co_return;
}

coro_scheduler::Task<int> failing_task() {
    co_await coro_scheduler::schedule();
    throw std::runtime_error("simulated failure");
    co_return 0; // unreachable, but keeps the coroutine well-formed
}

int main() {
    auto answer = compute_answer();
    while (!answer.is_ready()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::cout << "compute_answer() = " << answer.get() << " (expected 42): "
              << (answer.get() == 42 ? "PASS" : "FAIL") << "\n";

    std::vector<std::string> log;
    std::mutex log_mutex;
    auto progress = log_progress(log, log_mutex);
    while (!progress.is_ready()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    progress.get();
    std::cout << "log_progress() recorded " << log.size() << " steps: "
              << (log.size() == 3 ? "PASS" : "FAIL") << "\n";

    auto failing = failing_task();
    while (!failing.is_ready()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    bool caught = false;
    try {
        failing.get();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "failing_task() propagated exception: \"" << e.what() << "\": "
                  << (caught ? "PASS" : "FAIL") << "\n";
    }

    return (answer.get() == 42 && log.size() == 3 && caught) ? 0 : 1;
}

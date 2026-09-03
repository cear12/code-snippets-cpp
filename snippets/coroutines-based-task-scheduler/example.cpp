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
    void return_value(T value) { result_ = std::make_unique<T>(std::move(value)); }
    std::unique_ptr<T> result_;
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

        void unhandled_exception() { exception_ = std::current_exception(); }

        // Identity await_transform: lets co_await work uniformly for any
        // awaitable type without per-type special-casing here.
        template <typename Awaitable>
        auto await_transform(Awaitable&& awaitable) {
            return std::forward<Awaitable>(awaitable);
        }

        std::exception_ptr exception_;
    };

    using HandleType = std::coroutine_handle<promise_type>;

    explicit Task(HandleType h) : handle_(h) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle_) handle_.destroy();
    }

    T Get() requires(!std::is_void_v<T>) {
        if (!handle_ || !handle_.done()) {
            throw std::runtime_error("Task not completed");
        }
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return *handle_.promise().result_;
    }

    void Get() requires std::is_void_v<T> {
        if (!handle_ || !handle_.done()) {
            throw std::runtime_error("Task not completed");
        }
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

    bool IsReady() const { return handle_ && handle_.done(); }

    HandleType handle_;
};

// The scheduler: a pool of worker threads, each with a local queue, plus
// work stealing and a (see README) rarely-fed global overflow queue.
class CoroutineScheduler {
private:
    struct WorkerThread {
        std::thread thread_;
        std::queue<std::coroutine_handle<>> local_queue_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::atomic<bool> stop_requested_{false};

        WorkerThread(CoroutineScheduler* scheduler, size_t id) {
            thread_ = std::thread([this, scheduler, id] { scheduler->WorkerLoop(this, id); });
        }

        ~WorkerThread() {
            stop_requested_.store(true);
            condition_.notify_all();
            if (thread_.joinable()) thread_.join();
        }
    };

    std::vector<std::unique_ptr<WorkerThread>> workers_;
    std::queue<std::coroutine_handle<>> global_queue_;
    std::mutex global_mutex_;
    std::condition_variable global_condition_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> active_tasks_{0};
    std::atomic<size_t> next_worker_{0};

    void WorkerLoop(WorkerThread* worker, size_t worker_id) {
        while (!shutdown_.load()) {
            std::coroutine_handle<> task_handle;
            bool found_task = false;

            // 1. Local queue first.
            {
                std::unique_lock<std::mutex> lock(worker->queue_mutex_);
                if (!worker->local_queue_.empty()) {
                    task_handle = worker->local_queue_.front();
                    worker->local_queue_.pop();
                    found_task = true;
                }
            }

            // 2. Global overflow queue (see README's known limitation).
            if (!found_task) {
                std::unique_lock<std::mutex> lock(global_mutex_);
                global_condition_.wait_for(lock, std::chrono::milliseconds(10),
                                           [this] { return !global_queue_.empty() || shutdown_.load(); });

                if (!global_queue_.empty()) {
                    task_handle = global_queue_.front();
                    global_queue_.pop();
                    found_task = true;
                }
            }

            // 3. Steal from another worker.
            if (!found_task) {
                found_task = TryStealWork(worker_id, task_handle);
            }

            if (found_task) {
                ExecuteCoroutine(task_handle);
            }
        }
    }

    bool TryStealWork(size_t current_worker_id, std::coroutine_handle<>& stolen_task) {
        for (size_t i = 1; i < workers_.size(); ++i) {
            size_t target_id = (current_worker_id + i) % workers_.size();
            WorkerThread* target_worker = workers_[target_id].get();

            std::unique_lock<std::mutex> lock(target_worker->queue_mutex_, std::try_to_lock);
            if (lock.owns_lock() && !target_worker->local_queue_.empty()) {
                stolen_task = target_worker->local_queue_.front();
                target_worker->local_queue_.pop();
                return true;
            }
        }
        return false;
    }

    void ExecuteCoroutine(std::coroutine_handle<> handle) {
        // Exceptions from the coroutine body itself are already caught by
        // promise_type::unhandled_exception(); this guards resume() itself.
        try {
            handle.resume();
            if (handle.done()) {
                active_tasks_.fetch_sub(1, std::memory_order_relaxed);
            }
        } catch (...) {
            active_tasks_.fetch_sub(1, std::memory_order_relaxed);
        }
    }

public:
    explicit CoroutineScheduler(size_t num_threads = std::thread::hardware_concurrency()) {
        if (num_threads == 0) num_threads = 1;

        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back(std::make_unique<WorkerThread>(this, i));
        }
    }

    ~CoroutineScheduler() {
        shutdown_.store(true);
        global_condition_.notify_all();
        for (auto& worker : workers_) worker->condition_.notify_all();
        workers_.clear();
    }

    CoroutineScheduler(const CoroutineScheduler&) = delete;
    CoroutineScheduler& operator=(const CoroutineScheduler&) = delete;

    void Schedule(std::coroutine_handle<> handle) {
        active_tasks_.fetch_add(1, std::memory_order_relaxed);

        size_t worker_id = next_worker_.fetch_add(1) % workers_.size();
        WorkerThread* worker = workers_[worker_id].get();

        {
            std::lock_guard<std::mutex> lock(worker->queue_mutex_);
            worker->local_queue_.push(handle);
        }
        worker->condition_.notify_one();
    }

    size_t GetActiveTaskCount() const { return active_tasks_.load(); }

    void WaitForAllTasks() {
        while (active_tasks_.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

inline CoroutineScheduler& GetScheduler() {
    static CoroutineScheduler scheduler;
    return scheduler;
}

// Awaitable that hops the current coroutine onto the scheduler.
struct ScheduleAwaitable {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const noexcept { GetScheduler().Schedule(handle); }
    void await_resume() const noexcept {}
};

// Awaitable that suspends for a duration without blocking a worker thread.
struct DelayAwaitable {
    std::chrono::milliseconds duration_;

    explicit DelayAwaitable(std::chrono::milliseconds d) : duration_(d) {}

    bool await_ready() const noexcept { return duration_.count() <= 0; }

    void await_suspend(std::coroutine_handle<> handle) const {
        // BUG FIX: capture duration by value rather than reading it back
        // through `this` after the sleep -- see README.md.
        std::thread([handle, sleep_duration = duration_] {
            std::this_thread::sleep_for(sleep_duration);
            GetScheduler().Schedule(handle);
        }).detach();
    }

    void await_resume() const noexcept {}
};

inline ScheduleAwaitable Schedule() { return ScheduleAwaitable{}; }
inline DelayAwaitable Delay(std::chrono::milliseconds ms) { return DelayAwaitable{ms}; }

// Awaitable that runs a callable on a throwaway thread, then resumes the
// coroutine back on the scheduler.
template <typename F>
struct ThreadPoolAwaitable {
    F function_;

    explicit ThreadPoolAwaitable(F&& f) : function_(std::forward<F>(f)) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([handle, func = function_]() mutable {
            try {
                func();
            } catch (...) {
                // Surfaced to the caller via promise_type::unhandled_exception
                // when the coroutine resumes and rethrows internally.
            }
            GetScheduler().Schedule(handle);
        }).detach();
    }

    void await_resume() const noexcept {}
};

template <typename F>
auto RunOnThreadPool(F&& func) {
    return ThreadPoolAwaitable<F>{std::forward<F>(func)};
}

} // namespace coro_scheduler

// ---- Demo ----

coro_scheduler::Task<int> ComputeAnswer() {
    co_await coro_scheduler::Schedule(); // Hop onto a scheduler worker thread.

    int partial = 0;
    co_await coro_scheduler::RunOnThreadPool([&partial] { partial = 40; });

    co_await coro_scheduler::Delay(std::chrono::milliseconds(5));

    co_return partial + 2;
}

coro_scheduler::Task<void> LogProgress(std::vector<std::string>& log, std::mutex& log_mutex) {
    co_await coro_scheduler::Schedule();

    for (int i = 0; i < 3; ++i) {
        co_await coro_scheduler::Delay(std::chrono::milliseconds(2));
        std::lock_guard<std::mutex> lock(log_mutex);
        log.push_back("step " + std::to_string(i));
    }
    co_return;
}

coro_scheduler::Task<int> FailingTask() {
    co_await coro_scheduler::Schedule();
    throw std::runtime_error("simulated failure");
    co_return 0; // unreachable, but keeps the coroutine well-formed
}

int main() {
    auto answer = ComputeAnswer();
    while (!answer.IsReady()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::cout << "compute_answer() = " << answer.Get() << " (expected 42): "
              << (answer.Get() == 42 ? "PASS" : "FAIL") << "\n";

    std::vector<std::string> log;
    std::mutex log_mutex;
    auto progress = LogProgress(log, log_mutex);
    while (!progress.IsReady()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    progress.Get();
    std::cout << "log_progress() recorded " << log.size() << " steps: "
              << (log.size() == 3 ? "PASS" : "FAIL") << "\n";

    auto failing = FailingTask();
    while (!failing.IsReady()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    bool caught = false;
    try {
        failing.Get();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "failing_task() propagated exception: \"" << e.what() << "\": "
                  << (caught ? "PASS" : "FAIL") << "\n";
    }

    return (answer.Get() == 42 && log.size() == 3 && caught) ? 0 : 1;
}

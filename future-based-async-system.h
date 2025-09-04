#include <future>
#include <functional>
#include <memory>
#include <type_traits>
#include <exception>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <chrono>

namespace async_system {

// Forward declarations
template<typename T> class Future;
template<typename T> class Promise;

// Базовая реализация Future с continuation
template<typename T>
class Future {
private:
    struct SharedState {
        std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> ready{false};
        std::exception_ptr exception;
        
        // Для хранения результата
        union {
            T value;
        };
        
        // Continuation chain
        std::function<void()> continuation;
        
        SharedState() = default;
        
        ~SharedState() {
            if (ready.load() && !exception) {
                value.~T();
            }
        }
        
        template<typename U>
        void set_value(U&& val) {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready.load()) {
                throw std::future_error(std::future_errc::promise_already_satisfied);
            }
            
            new (&value) T(std::forward<U>(val));
            ready.store(true, std::memory_order_release);
            
            if (continuation) {
                continuation();
            }
            condition.notify_all();
        }
        
        void set_exception(std::exception_ptr ex) {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready.load()) {
                throw std::future_error(std::future_errc::promise_already_satisfied);
            }
            
            exception = ex;
            ready.store(true, std::memory_order_release);
            
            if (continuation) {
                continuation();
            }
            condition.notify_all();
        }
        
        T get() {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this] { return ready.load(); });
            
            if (exception) {
                std::rethrow_exception(exception);
            }
            
            return std::move(value);
        }
        
        template<typename Rep, typename Period>
        std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) {
            std::unique_lock<std::mutex> lock(mutex);
            if (condition.wait_for(lock, timeout, [this] { return ready.load(); })) {
                return std::future_status::ready;
            }
            return std::future_status::timeout;
        }
        
        void set_continuation(std::function<void()> cont) {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready.load()) {
                // Уже готов, выполняем continuation немедленно
                cont();
            } else {
                continuation = std::move(cont);
            }
        }
    };
    
    std::shared_ptr<SharedState> state_;
    
    friend class Promise<T>;
    
    explicit Future(std::shared_ptr<SharedState> state) : state_(std::move(state)) {}
    
public:
    Future() = default;
    Future(Future&&) = default;
    Future& operator=(Future&&) = default;
    
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    
    bool valid() const {
        return state_ != nullptr;
    }
    
    T get() {
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        return state_->get();
    }
    
    template<typename Rep, typename Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        return state_->wait_for(timeout);
    }
    
    bool is_ready() const {
        return valid() && state_->ready.load();
    }
    
    // Continuation methods
    template<typename F>
    auto then(F&& func) -> Future<std::invoke_result_t<F, Future<T>&&>> {
        using ResultType = std::invoke_result_t<F, Future<T>&&>;
        
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        
        auto new_promise = std::make_shared<Promise<ResultType>>();
        auto result_future = new_promise->get_future();
        
        state_->set_continuation([state = state_, func = std::forward<F>(func), new_promise]() mutable {
            try {
                Future<T> current_future(state);
                
                if constexpr (std::is_void_v<ResultType>) {
                    func(std::move(current_future));
                    new_promise->set_value();
                } else {
                    auto result = func(std::move(current_future));
                    new_promise->set_value(std::move(result));
                }
            } catch (...) {
                new_promise->set_exception(std::current_exception());
            }
        });
        
        return result_future;
    }
    
    // Continuation с executor
    template<typename F, typename Executor>
    auto then(Executor&& executor, F&& func) -> Future<std::invoke_result_t<F, Future<T>&&>> {
        using ResultType = std::invoke_result_t<F, Future<T>&&>;
        
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        
        auto new_promise = std::make_shared<Promise<ResultType>>();
        auto result_future = new_promise->get_future();
        
        state_->set_continuation([state = state_, func = std::forward<F>(func), 
                                new_promise, executor = std::forward<Executor>(executor)]() mutable {
            executor([state, func = std::move(func), new_promise]() mutable {
                try {
                    Future<T> current_future(state);
                    
                    if constexpr (std::is_void_v<ResultType>) {
                        func(std::move(current_future));
                        new_promise->set_value();
                    } else {
                        auto result = func(std::move(current_future));
                        new_promise->set_value(std::move(result));
                    }
                } catch (...) {
                    new_promise->set_exception(std::current_exception());
                }
            });
        });
        
        return result_future;
    }
    
    // Обработка исключений
    template<typename F>
    auto catch_error(F&& func) -> Future<T> {
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        
        return then([func = std::forward<F>(func)](Future<T>&& fut) -> T {
            try {
                return fut.get();
            } catch (...) {
                return func(std::current_exception());
            }
        });
    }
};

// Специализация для void
template<>
class Future<void> {
private:
    struct SharedState {
        std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> ready{false};
        std::exception_ptr exception;
        std::function<void()> continuation;
        
        void set_value() {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready.load()) {
                throw std::future_error(std::future_errc::promise_already_satisfied);
            }
            
            ready.store(true, std::memory_order_release);
            
            if (continuation) {
                continuation();
            }
            condition.notify_all();
        }
        
        void set_exception(std::exception_ptr ex) {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready.load()) {
                throw std::future_error(std::future_errc::promise_already_satisfied);
            }
            
            exception = ex;
            ready.store(true, std::memory_order_release);
            
            if (continuation) {
                continuation();
            }
            condition.notify_all();
        }
        
        void get() {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this] { return ready.load(); });
            
            if (exception) {
                std::rethrow_exception(exception);
            }
        }
        
        void set_continuation(std::function<void()> cont) {
            std::lock_guard<std::mutex> lock(mutex);
            if (ready.load()) {
                cont();
            } else {
                continuation = std::move(cont);
            }
        }
    };
    
    std::shared_ptr<SharedState> state_;
    
    friend class Promise<void>;
    
    explicit Future(std::shared_ptr<SharedState> state) : state_(std::move(state)) {}
    
public:
    Future() = default;
    Future(Future&&) = default;
    Future& operator=(Future&&) = default;
    
    void get() {
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->get();
    }
    
    bool valid() const {
        return state_ != nullptr;
    }
    
    bool is_ready() const {
        return valid() && state_->ready.load();
    }
    
    template<typename F>
    auto then(F&& func) -> Future<std::invoke_result_t<F, Future<void>&&>> {
        using ResultType = std::invoke_result_t<F, Future<void>&&>;
        
        if (!valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
        
        auto new_promise = std::make_shared<Promise<ResultType>>();
        auto result_future = new_promise->get_future();
        
        state_->set_continuation([state = state_, func = std::forward<F>(func), new_promise]() mutable {
            try {
                Future<void> current_future(state);
                
                if constexpr (std::is_void_v<ResultType>) {
                    func(std::move(current_future));
                    new_promise->set_value();
                } else {
                    auto result = func(std::move(current_future));
                    new_promise->set_value(std::move(result));
                }
            } catch (...) {
                new_promise->set_exception(std::current_exception());
            }
        });
        
        return result_future;
    }
};

// Promise implementation
template<typename T>
class Promise {
private:
    std::shared_ptr<typename Future<T>::SharedState> state_;
    
public:
    Promise() : state_(std::make_shared<typename Future<T>::SharedState>()) {}
    
    Promise(Promise&&) = default;
    Promise& operator=(Promise&&) = default;
    
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;
    
    Future<T> get_future() {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        return Future<T>(state_);
    }
    
    template<typename U>
    void set_value(U&& value) {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->set_value(std::forward<U>(value));
    }
    
    void set_exception(std::exception_ptr ex) {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->set_exception(ex);
    }
};

// Специализация Promise для void
template<>
class Promise<void> {
private:
    std::shared_ptr<Future<void>::SharedState> state_;
    
public:
    Promise() : state_(std::make_shared<Future<void>::SharedState>()) {}
    
    Promise(Promise&&) = default;
    Promise& operator=(Promise&&) = default;
    
    Future<void> get_future() {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        return Future<void>(state_);
    }
    
    void set_value() {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->set_value();
    }
    
    void set_exception(std::exception_ptr ex) {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->set_exception(ex);
    }
};

// Утилитарные функции
template<typename T>
Future<T> make_ready_future(T&& value) {
    Promise<T> promise;
    auto future = promise.get_future();
    promise.set_value(std::forward<T>(value));
    return future;
}

inline Future<void> make_ready_future() {
    Promise<void> promise;
    auto future = promise.get_future();
    promise.set_value();
    return future;
}

template<typename T>
Future<T> make_exceptional_future(std::exception_ptr ex) {
    Promise<T> promise;
    auto future = promise.get_future();
    promise.set_exception(ex);
    return future;
}

// when_all - ждет завершения всех futures
template<typename... Futures>
auto when_all(Futures&&... futures) {
    using TupleType = std::tuple<typename std::decay_t<Futures>::value_type...>;
    
    Promise<TupleType> result_promise;
    auto result_future = result_promise.get_future();
    
    auto shared_promise = std::make_shared<Promise<TupleType>>(std::move(result_promise));
    auto counter = std::make_shared<std::atomic<size_t>>(sizeof...(futures));
    auto results = std::make_shared<TupleType>();
    auto exception_occurred = std::make_shared<std::atomic<bool>>(false);
    
    auto complete_one = [shared_promise, counter, results, exception_occurred]() {
        if (counter->fetch_sub(1) == 1) {
            // Все futures завершены
            if (!exception_occurred->load()) {
                shared_promise->set_value(std::move(*results));
            }
        }
    };
    
    size_t index = 0;
    auto process_future = [&](auto&& future) {
        size_t current_index = index++;
        
        future.then([shared_promise, complete_one, results, exception_occurred, current_index]
                   (auto&& fut) mutable {
            try {
                if constexpr (!std::is_void_v<typename std::decay_t<decltype(fut)>::value_type>) {
                    std::get<current_index>(*results) = fut.get();
                } else {
                    fut.get(); // Просто проверяем на исключения
                }
            } catch (...) {
                if (!exception_occurred->exchange(true)) {
                    shared_promise->set_exception(std::current_exception());
                }
                return;
            }
            complete_one();
        });
    };
    
    (process_future(std::forward<Futures>(futures)), ...);
    
    return result_future;
}

// when_any - завершается при завершении любого future
template<typename... Futures>
auto when_any(Futures&&... futures) {
    Promise<size_t> result_promise;
    auto result_future = result_promise.get_future();
    
    auto shared_promise = std::make_shared<Promise<size_t>>(std::move(result_promise));
    auto completed = std::make_shared<std::atomic<bool>>(false);
    
    size_t index = 0;
    auto process_future = [&](auto&& future) {
        size_t current_index = index++;
        
        future.then([shared_promise, completed, current_index](auto&& fut) mutable {
            if (!completed->exchange(true)) {
                try {
                    fut.get(); // Проверяем на исключения
                    shared_promise->set_value(current_index);
                } catch (...) {
                    shared_promise->set_exception(std::current_exception());
                }
            }
        });
    };
    
    (process_future(std::forward<Futures>(futures)), ...);
    
    return result_future;
}

// Async executor
class ThreadPoolExecutor {
private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    
    void worker() {
        while (!stop_.load()) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { return !tasks_.empty() || stop_.load(); });
                
                if (stop_.load()) break;
                
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            
            task();
        }
    }
    
public:
    explicit ThreadPoolExecutor(size_t num_threads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this] { worker(); });
        }
    }
    
    ~ThreadPoolExecutor() {
        stop_.store(true);
        condition_.notify_all();
        
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    template<typename F>
    void operator()(F&& task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace(std::forward<F>(task));
        }
        condition_.notify_one();
    }
};

// Глобальный executor
inline ThreadPoolExecutor& get_default_executor() {
    static ThreadPoolExecutor executor;
    return executor;
}

// Async функция
template<typename F, typename... Args>
auto async(F&& func, Args&&... args) -> Future<std::invoke_result_t<F, Args...>> {
    using ResultType = std::invoke_result_t<F, Args...>;
    
    Promise<ResultType> promise;
    auto future = promise.get_future();
    
    auto shared_promise = std::make_shared<Promise<ResultType>>(std::move(promise));
    
    get_default_executor()([shared_promise, func = std::forward<F>(func), 
                           args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        try {
            if constexpr (std::is_void_v<ResultType>) {
                std::apply(func, std::move(args));
                shared_promise->set_value();
            } else {
                auto result = std::apply(func, std::move(args));
                shared_promise->set_value(std::move(result));
            }
        } catch (...) {
            shared_promise->set_exception(std::current_exception());
        }
    });
    
    return future;
}

} // namespace async_system

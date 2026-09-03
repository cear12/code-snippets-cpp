// Future/Promise framework with then()/catch_error() continuations,
// when_all()/when_any() combinators, and a thread-pool-backed async().
// See README.md for the four real bugs fixed here, the most serious
// being a self-deadlock: the original ran a Future's continuation while
// still holding the SharedState's own mutex, and a continuation's whole
// point is usually to call .Get() on a Future wrapping that same
// SharedState.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace async_system {

// Forward declarations
template <typename T> class Future;
template <typename T> class Promise;

// Базовая реализация Future с continuation
template <typename T> class Future {
private:
  struct SharedState {
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> ready_{false};
    std::exception_ptr exception_;

    // Для хранения результата
    union {
      T value_;
    };

    // Continuation chain
    std::function<void()> continuation_;

    // NOTE: must NOT be `= default` -- see README.md. A defaulted
    // default constructor tries to default-initialize the anonymous
    // union's `value` member, whose own default constructor is
    // implicitly deleted whenever T is non-trivially default
    // constructible (e.g. T = std::tuple<...>, as when_all() uses).
    // An empty user-provided body leaves `value` untouched, which is
    // exactly what the manual placement-new lifetime scheme needs.
    SharedState() {}

    ~SharedState() {
      if (ready_.load() && !exception_) {
        value_.~T();
      }
    }

    // BUG FIX (see README.md): the original ran continuation() while
    // still holding mutex. Since a continuation's whole point is
    // usually to call fut.Get() on a Future wrapping this same
    // SharedState -- which locks this same, non-recursive mutex on
    // the same thread -- that self-deadlocked on every then()-chained
    // get(). Fixed by moving the continuation out under the lock and
    // invoking it only after releasing the lock.
    template <typename U> void SetValue(U &&val) {
      std::function<void()> cont_to_run;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_.load()) {
          throw std::future_error(std::future_errc::promise_already_satisfied);
        }

        new (&value_) T(std::forward<U>(val));
        ready_.store(true, std::memory_order_release);
        cont_to_run = std::move(continuation_);
      }
      condition_.notify_all();
      if (cont_to_run) {
        cont_to_run();
      }
    }

    void SetException(std::exception_ptr ex) {
      std::function<void()> cont_to_run;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_.load()) {
          throw std::future_error(std::future_errc::promise_already_satisfied);
        }

        exception_ = ex;
        ready_.store(true, std::memory_order_release);
        cont_to_run = std::move(continuation_);
      }
      condition_.notify_all();
      if (cont_to_run) {
        cont_to_run();
      }
    }

    T Get() {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock, [this] { return ready_.load(); });

      if (exception_) {
        std::rethrow_exception(exception_);
      }

      return std::move(value_);
    }

    template <typename Rep, typename Period>
    std::future_status
    WaitFor(const std::chrono::duration<Rep, Period> &timeout) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (condition_.wait_for(lock, timeout,
                              [this] { return ready_.load(); })) {
        return std::future_status::ready;
      }
      return std::future_status::timeout;
    }

    // Same fix as set_value/set_exception: don't invoke user code
    // (the immediate-run case, when the future is already ready by
    // the time then() attaches) while still holding the lock.
    void SetContinuation(std::function<void()> cont) {
      bool run_immediately = false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_.load()) {
          run_immediately = true;
        } else {
          continuation_ = std::move(cont);
        }
      }
      if (run_immediately) {
        cont();
      }
    }
  };

  std::shared_ptr<SharedState> state_;

  friend class Promise<T>;

  explicit Future(std::shared_ptr<SharedState> state)
      : state_(std::move(state)) {}

public:
  using ValueType = T;

  Future() = default;
  Future(Future &&) = default;
  Future &operator=(Future &&) = default;

  Future(const Future &) = delete;
  Future &operator=(const Future &) = delete;

  bool Valid() const { return state_ != nullptr; }

  T Get() {
    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }
    return state_->Get();
  }

  template <typename Rep, typename Period>
  std::future_status
  WaitFor(const std::chrono::duration<Rep, Period> &timeout) {
    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }
    return state_->WaitFor(timeout);
  }

  bool IsReady() const { return Valid() && state_->ready_.load(); }

  // Continuation methods
  template <typename F>
  auto Then(F &&func) -> Future<std::invoke_result_t<F, Future<T> &&>> {
    using ResultType = std::invoke_result_t<F, Future<T> &&>;

    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }

    auto new_promise = std::make_shared<Promise<ResultType>>();
    auto result_future = new_promise->GetFuture();

    state_->SetContinuation(
        [state = state_, func = std::forward<F>(func), new_promise]() mutable {
          try {
            Future<T> current_future(state);

            if constexpr (std::is_void_v<ResultType>) {
              func(std::move(current_future));
              new_promise->SetValue();
            } else {
              auto result = func(std::move(current_future));
              new_promise->SetValue(std::move(result));
            }
          } catch (...) {
            new_promise->SetException(std::current_exception());
          }
        });

    return result_future;
  }

  // Continuation с executor
  template <typename F, typename Executor>
  auto Then(Executor &&executor, F &&func)
      -> Future<std::invoke_result_t<F, Future<T> &&>> {
    using ResultType = std::invoke_result_t<F, Future<T> &&>;

    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }

    auto new_promise = std::make_shared<Promise<ResultType>>();
    auto result_future = new_promise->GetFuture();

    state_->SetContinuation(
        [state = state_, func = std::forward<F>(func), new_promise,
         executor = std::forward<Executor>(executor)]() mutable {
          executor([state, func = std::move(func), new_promise]() mutable {
            try {
              Future<T> current_future(state);

              if constexpr (std::is_void_v<ResultType>) {
                func(std::move(current_future));
                new_promise->SetValue();
              } else {
                auto result = func(std::move(current_future));
                new_promise->SetValue(std::move(result));
              }
            } catch (...) {
              new_promise->SetException(std::current_exception());
            }
          });
        });

    return result_future;
  }

  // Обработка исключений
  template <typename F> auto CatchError(F &&func) -> Future<T> {
    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }

    return Then([func = std::forward<F>(func)](Future<T> &&fut) -> T {
      try {
        return fut.Get();
      } catch (...) {
        return func(std::current_exception());
      }
    });
  }
};

// Специализация для void
template <> class Future<void> {
private:
  struct SharedState {
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> ready_{false};
    std::exception_ptr exception_;
    std::function<void()> continuation_;

    // BUG FIX (see README.md and the matching fix in Future<T>'s
    // primary-template SharedState): don't invoke continuation()
    // while still holding mutex -- a continuation calling fut.Get()
    // on a Future wrapping this same SharedState would self-deadlock
    // on this same, non-recursive mutex.
    void SetValue() {
      std::function<void()> cont_to_run;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_.load()) {
          throw std::future_error(std::future_errc::promise_already_satisfied);
        }

        ready_.store(true, std::memory_order_release);
        cont_to_run = std::move(continuation_);
      }
      condition_.notify_all();
      if (cont_to_run) {
        cont_to_run();
      }
    }

    void SetException(std::exception_ptr ex) {
      std::function<void()> cont_to_run;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_.load()) {
          throw std::future_error(std::future_errc::promise_already_satisfied);
        }

        exception_ = ex;
        ready_.store(true, std::memory_order_release);
        cont_to_run = std::move(continuation_);
      }
      condition_.notify_all();
      if (cont_to_run) {
        cont_to_run();
      }
    }

    void Get() {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock, [this] { return ready_.load(); });

      if (exception_) {
        std::rethrow_exception(exception_);
      }
    }

    // Same fix as set_value/set_exception below: don't invoke user
    // code while still holding the lock.
    void SetContinuation(std::function<void()> cont) {
      bool run_immediately = false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_.load()) {
          run_immediately = true;
        } else {
          continuation_ = std::move(cont);
        }
      }
      if (run_immediately) {
        cont();
      }
    }
  };

  std::shared_ptr<SharedState> state_;

  friend class Promise<void>;

  explicit Future(std::shared_ptr<SharedState> state)
      : state_(std::move(state)) {}

public:
  using ValueType = void;

  Future() = default;
  Future(Future &&) = default;
  Future &operator=(Future &&) = default;

  void Get() {
    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }
    state_->Get();
  }

  bool Valid() const { return state_ != nullptr; }

  bool IsReady() const { return Valid() && state_->ready_.load(); }

  template <typename F>
  auto Then(F &&func) -> Future<std::invoke_result_t<F, Future<void> &&>> {
    using ResultType = std::invoke_result_t<F, Future<void> &&>;

    if (!Valid()) {
      throw std::future_error(std::future_errc::no_state);
    }

    auto new_promise = std::make_shared<Promise<ResultType>>();
    auto result_future = new_promise->GetFuture();

    state_->SetContinuation(
        [state = state_, func = std::forward<F>(func), new_promise]() mutable {
          try {
            Future<void> current_future(state);

            if constexpr (std::is_void_v<ResultType>) {
              func(std::move(current_future));
              new_promise->SetValue();
            } else {
              auto result = func(std::move(current_future));
              new_promise->SetValue(std::move(result));
            }
          } catch (...) {
            new_promise->SetException(std::current_exception());
          }
        });

    return result_future;
  }
};

// Promise implementation
template <typename T> class Promise {
private:
  std::shared_ptr<typename Future<T>::SharedState> state_;

public:
  Promise() : state_(std::make_shared<typename Future<T>::SharedState>()) {}

  Promise(Promise &&) = default;
  Promise &operator=(Promise &&) = default;

  Promise(const Promise &) = delete;
  Promise &operator=(const Promise &) = delete;

  Future<T> GetFuture() {
    if (!state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    return Future<T>(state_);
  }

  template <typename U> void SetValue(U &&value) {
    if (!state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    state_->SetValue(std::forward<U>(value));
  }

  void SetException(std::exception_ptr ex) {
    if (!state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    state_->SetException(ex);
  }
};

// Специализация Promise для void
template <> class Promise<void> {
private:
  std::shared_ptr<Future<void>::SharedState> state_;

public:
  Promise() : state_(std::make_shared<Future<void>::SharedState>()) {}

  Promise(Promise &&) = default;
  Promise &operator=(Promise &&) = default;

  Future<void> GetFuture() {
    if (!state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    return Future<void>(state_);
  }

  void SetValue() {
    if (!state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    state_->SetValue();
  }

  void SetException(std::exception_ptr ex) {
    if (!state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    state_->SetException(ex);
  }
};

// Утилитарные функции
template <typename T> Future<T> MakeReadyFuture(T &&value) {
  Promise<T> promise;
  auto future = promise.GetFuture();
  promise.SetValue(std::forward<T>(value));
  return future;
}

inline Future<void> MakeReadyFuture() {
  Promise<void> promise;
  auto future = promise.GetFuture();
  promise.SetValue();
  return future;
}

template <typename T> Future<T> MakeExceptionalFuture(std::exception_ptr ex) {
  Promise<T> promise;
  auto future = promise.GetFuture();
  promise.SetException(ex);
  return future;
}

namespace detail {

// std::get<N> requires N to be a compile-time constant, so each future's
// continuation is attached by a function template explicitly
// instantiated per index I, rather than by capturing a runtime loop
// counter (see README.md for the bug this replaces).
template <size_t I, typename SharedPromise, typename CompleteOne,
          typename Results, typename ExceptionFlag, typename Fut>
void WhenAllAttach(SharedPromise shared_promise, CompleteOne complete_one,
                   Results results, ExceptionFlag exception_occurred,
                   Fut &&future) {
  future.Then([shared_promise, complete_one, results,
               exception_occurred](auto &&fut) mutable {
    try {
      if constexpr (!std::is_void_v<
                        typename std::decay_t<decltype(fut)>::ValueType>) {
        std::get<I>(*results) = fut.Get();
      } else {
        fut.Get(); // Just check for exceptions.
      }
    } catch (...) {
      if (!exception_occurred->exchange(true)) {
        shared_promise->SetException(std::current_exception());
      }
      return;
    }
    complete_one();
  });
}

// Expands an index_sequence alongside the Futures pack so each future is
// paired with its own compile-time index.
template <typename SharedPromise, typename CompleteOne, typename Results,
          typename ExceptionFlag, typename... Futures, size_t... Is>
void WhenAllAttachEach(std::index_sequence<Is...>, SharedPromise shared_promise,
                       CompleteOne complete_one, Results results,
                       ExceptionFlag exception_occurred, Futures &&...futures) {
  (WhenAllAttach<Is>(shared_promise, complete_one, results, exception_occurred,
                     std::forward<Futures>(futures)),
   ...);
}

} // namespace detail

// when_all - ждет завершения всех futures
template <typename... Futures> auto WhenAll(Futures &&...futures) {
  using TupleType = std::tuple<typename std::decay_t<Futures>::ValueType...>;

  Promise<TupleType> result_promise;
  auto result_future = result_promise.GetFuture();

  auto shared_promise =
      std::make_shared<Promise<TupleType>>(std::move(result_promise));
  auto counter = std::make_shared<std::atomic<size_t>>(sizeof...(futures));
  auto results = std::make_shared<TupleType>();
  auto exception_occurred = std::make_shared<std::atomic<bool>>(false);

  auto complete_one = [shared_promise, counter, results, exception_occurred]() {
    if (counter->fetch_sub(1) == 1) {
      // Все futures завершены
      if (!exception_occurred->load()) {
        shared_promise->SetValue(std::move(*results));
      }
    }
  };

  detail::WhenAllAttachEach(
      std::index_sequence_for<Futures...>{}, shared_promise, complete_one,
      results, exception_occurred, std::forward<Futures>(futures)...);

  return result_future;
}

// when_any - завершается при завершении любого future
template <typename... Futures> auto WhenAny(Futures &&...futures) {
  Promise<size_t> result_promise;
  auto result_future = result_promise.GetFuture();

  auto shared_promise =
      std::make_shared<Promise<size_t>>(std::move(result_promise));
  auto completed = std::make_shared<std::atomic<bool>>(false);

  size_t index = 0;
  auto process_future = [&](auto &&future) {
    size_t current_index = index++;

    future.Then([shared_promise, completed, current_index](auto &&fut) mutable {
      if (!completed->exchange(true)) {
        try {
          fut.Get(); // Проверяем на исключения
          shared_promise->SetValue(current_index);
        } catch (...) {
          shared_promise->SetException(std::current_exception());
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

  void Worker() {
    while (!stop_.load()) {
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock,
                        [this] { return !tasks_.empty() || stop_.load(); });

        if (stop_.load())
          break;

        task = std::move(tasks_.front());
        tasks_.pop();
      }

      task();
    }
  }

public:
  explicit ThreadPoolExecutor(
      size_t num_threads = std::thread::hardware_concurrency()) {
    for (size_t i = 0; i < num_threads; ++i) {
      threads_.emplace_back([this] { Worker(); });
    }
  }

  ~ThreadPoolExecutor() {
    stop_.store(true);
    condition_.notify_all();

    for (auto &thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  template <typename F> void operator()(F &&task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.emplace(std::forward<F>(task));
    }
    condition_.notify_one();
  }
};

// Глобальный executor
inline ThreadPoolExecutor &GetDefaultExecutor() {
  static ThreadPoolExecutor executor;
  return executor;
}

// Async функция
template <typename F, typename... Args>
auto Async(F &&func, Args &&...args)
    -> Future<std::invoke_result_t<F, Args...>> {
  using ResultType = std::invoke_result_t<F, Args...>;

  Promise<ResultType> promise;
  auto future = promise.GetFuture();

  auto shared_promise =
      std::make_shared<Promise<ResultType>>(std::move(promise));

  GetDefaultExecutor()(
      [shared_promise, func = std::forward<F>(func),
       args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        try {
          if constexpr (std::is_void_v<ResultType>) {
            std::apply(func, std::move(args));
            shared_promise->SetValue();
          } else {
            auto result = std::apply(func, std::move(args));
            shared_promise->SetValue(std::move(result));
          }
        } catch (...) {
          shared_promise->SetException(std::current_exception());
        }
      });

  return future;
}

} // namespace async_system

int main() {
  using namespace async_system;
  bool all_ok = true;

  // Basic Promise/Future round trip, non-trivial T.
  {
    Promise<std::string> p;
    auto f = p.GetFuture();
    p.SetValue(std::string("hello"));
    bool ok = f.Get() == "hello";
    std::cout << "[basic promise/future, string] " << (ok ? "PASS" : "FAIL")
              << "\n";
    all_ok &= ok;
  }

  // then() chaining.
  {
    Promise<int> p;
    auto f = p.GetFuture();
    auto chained = f.Then([](Future<int> &&fut) { return fut.Get() * 2; });
    p.SetValue(21);
    bool ok = chained.Get() == 42;
    std::cout << "[then chaining] " << (ok ? "PASS" : "FAIL") << "\n";
    all_ok &= ok;
  }

  // catch_error recovers from an exception.
  {
    Promise<int> p;
    auto f = p.GetFuture();
    auto recovered = f.CatchError([](std::exception_ptr) { return -1; });
    p.SetException(std::make_exception_ptr(std::runtime_error("boom")));
    bool ok = recovered.Get() == -1;
    std::cout << "[catch_error] " << (ok ? "PASS" : "FAIL") << "\n";
    all_ok &= ok;
  }

  // Future<void>.
  {
    Promise<void> p;
    auto f = p.GetFuture();
    p.SetValue();
    f.Get(); // must not throw
    std::cout << "[Future<void>] PASS\n";
  }

  // when_all with a non-trivial tuple element type (this is exactly
  // what used to fail to compile -- see README.md).
  {
    Promise<int> p1;
    Promise<std::string> p2;
    auto f1 = p1.GetFuture();
    auto f2 = p2.GetFuture();
    p1.SetValue(10);
    p2.SetValue(std::string("twenty"));

    auto combined = WhenAll(std::move(f1), std::move(f2));
    auto results = combined.Get();
    bool ok = std::get<0>(results) == 10 && std::get<1>(results) == "twenty";
    std::cout << "[when_all, mixed types] " << (ok ? "PASS" : "FAIL") << "\n";
    all_ok &= ok;
  }

  // when_all with three futures -- makes sure each index lands in the
  // right tuple slot (the bug this replaces would not even compile).
  {
    Promise<int> p1, p2, p3;
    auto f1 = p1.GetFuture();
    auto f2 = p2.GetFuture();
    auto f3 = p3.GetFuture();
    p1.SetValue(1);
    p2.SetValue(2);
    p3.SetValue(3);

    auto combined = WhenAll(std::move(f1), std::move(f2), std::move(f3));
    auto results = combined.Get();
    bool ok = std::get<0>(results) == 1 && std::get<1>(results) == 2 &&
              std::get<2>(results) == 3;
    std::cout << "[when_all, 3-way index correctness] "
              << (ok ? "PASS" : "FAIL") << "\n";
    all_ok &= ok;
  }

  // when_any completes as soon as one future is ready.
  {
    Promise<int> p1, p2;
    auto f1 = p1.GetFuture();
    auto f2 = p2.GetFuture();
    p2.SetValue(99); // only p2 is ever satisfied

    auto first = WhenAny(std::move(f1), std::move(f2));
    bool ok = first.Get() == 1; // index of f2
    std::cout << "[when_any] " << (ok ? "PASS" : "FAIL") << "\n";
    all_ok &= ok;
  }

  // async() + the default thread pool executor.
  {
    auto f = Async([](int a, int b) { return a + b; }, 3, 4);
    bool ok = f.Get() == 7;
    std::cout << "[async()] " << (ok ? "PASS" : "FAIL") << "\n";
    all_ok &= ok;
  }

  std::cout << (all_ok ? "ALL PASS" : "SOME FAILED") << "\n";
  return all_ok ? 0 : 1;
}

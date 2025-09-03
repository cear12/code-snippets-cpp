#include <thread>
#include <future>
#include <type_traits>

template<class F>
auto async(F&& func) {
    using ResultType = std::invoke_result_t<std::decay_t<F>>;
    using PromiseType = std::promise<ResultType>;
    using FutureType = std::future<ResultType>;

    PromiseType promise;
    FutureType future = promise.get_future();

    auto t = std::thread(
        [func = std::forward<F>(func), promise = std::move(promise)]() mutable {
            try {
                ResultType result = func();
                promise.set_value(result);
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        });

    // Эта особенность (авто-join потока при уничтожении future) 
    // не реализуется стандартными средствами.
    // В стандартной async эта логика встроена.
    //
    // future.on_destruction([t = std::move(t)]() {
    //     t.join();
    // });

    return future;
}
// A minimal std::async, reimplemented: spins up a thread and routes its
// result into a future via a promise, the same plumbing std::async uses
// internally.
#include <future>
#include <iostream>
#include <thread>
#include <type_traits>

template <typename F>
auto async_demo(F&& func) {
    using ResultType = std::invoke_result_t<std::decay_t<F>>;
    std::promise<ResultType> promise;
    std::future<ResultType> future = promise.get_future();

    // NOTE: unlike std::async's returned future, this one's destructor
    // does NOT block until the thread finishes -- so the thread here is
    // detached, and it's this function's caller's job to wait on the
    // future before letting the program (or anything the thread touches)
    // go away, exactly as done in main() below.
    std::thread(
        [func = std::forward<F>(func), promise = std::move(promise)]() mutable {
            try {
                ResultType result = func();
                promise.set_value(result);
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        })
        .detach();

    return future;
}

int main() {
    auto future = async_demo([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return 21 * 2;
    });

    std::cout << "waiting for result...\n";
    std::cout << "result = " << future.get() << "\n"; // blocks until the thread is done

    return 0;
}

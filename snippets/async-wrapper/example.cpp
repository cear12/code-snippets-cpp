// RAII std::async Wrapper idiom: AsyncWrapper's destructor always calls
// future.get(), so the async task is guaranteed to be joined (and any
// exception it threw observed) before the wrapper goes out of scope.
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

template <typename T>
class AsyncWrapper {
public:
    explicit AsyncWrapper(std::function<T()> func) {
        std::cout << "starting async task\n";
        result_ = std::async(std::launch::async, std::move(func));
    }

    ~AsyncWrapper() {
        result_.get(); // always joins (and rethrows any exception) on destruction
        std::cout << "async task finished\n";
    }

    AsyncWrapper(const AsyncWrapper&) = delete;
    AsyncWrapper& operator=(const AsyncWrapper&) = delete;

private:
    std::future<T> result_;
};

void CallerFunc() {
    std::cout << "start caller func\n";

    std::function<void()> f = [] {
        for (int i = 0; i < 3; ++i) {
            std::cout << "working... " << i << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    AsyncWrapper<void> async_wrapper(std::move(f));
    std::cout << "finish caller func (destructor above still has to run)\n";
}

int main() {
    CallerFunc();
    return 0;
}

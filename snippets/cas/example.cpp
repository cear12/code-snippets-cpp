// Compare-And-Swap (CAS) idiom: a hand-written single-threaded CAS to
// show the semantics, plus a real multithreaded lock-free increment built
// from std::atomic::compare_exchange_strong.
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

// A hand-written CAS, spelling out exactly what the atomic primitive means.
bool cas(int* addr, int& expected, int new_value) {
    if (*addr != expected) {
        expected = *addr; // report the actual current value back to the caller
        return false;
    }
    *addr = new_value;
    return true;
}

std::atomic<int> counter{0};

void increment_counter(int num_iterations) {
    for (int i = 0; i < num_iterations; ++i) {
        int old_value = counter.load();
        int new_value;
        do {
            new_value = old_value + 1;
        } while (!counter.compare_exchange_strong(old_value, new_value));
        // on failure, compare_exchange_strong updates old_value to the
        // current value itself, so the loop retries with fresh data
    }
}

int main() {
    // Hand-written CAS demo.
    int addr = 23;
    int expected = 23;
    std::cout << "cas succeeded: " << std::boolalpha << cas(&addr, expected, 34) << "\n";
    std::cout << "addr = " << addr << "\n";

    // Real concurrent CAS-based increment: 4 threads x 10000 increments,
    // no mutex, each retrying compare_exchange_strong until it wins.
    constexpr int kThreads = 4;
    constexpr int kIterationsPerThread = 10000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(increment_counter, kIterationsPerThread);
    }
    for (auto& th : threads) th.join();

    std::cout << "counter = " << counter.load()
              << " (expected " << kThreads * kIterationsPerThread << ")\n";

    return 0;
}

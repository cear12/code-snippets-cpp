#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <map>

class ThreadMonitor {
public:
    struct ThreadStats {
        uint64_t total_tasks = 0;
        uint64_t failed_tasks = 0;
        uint64_t total_exec_time_ns = 0;
        std::vector<uint64_t> exec_times_ns;
    };

private:
    std::mutex stats_mutex;
    std::map<std::thread::id, ThreadStats> stats;

public:
    void record_task(std::thread::id tid, uint64_t exec_time_ns, bool success = true) {
        std::lock_guard<std::mutex> lock(stats_mutex);
        auto& s = stats[tid];
        s.total_tasks++;
        if (!success) s.failed_tasks++;
        s.total_exec_time_ns += exec_time_ns;
        s.exec_times_ns.push_back(exec_time_ns);
    }

    void print_report() {
        std::lock_guard<std::mutex> lock(stats_mutex);
        std::cout << "Thread Performance Report:\n";
        for (const auto& [tid, s] : stats) {
            double avg = s.total_tasks ? s.total_exec_time_ns / double(s.total_tasks) : 0;
            std::cout << "Thread " << tid << ": "
                      << s.total_tasks << " tasks, "
                      << s.failed_tasks << " failed, "
                      << "avg exec " << std::fixed << std::setprecision(2) << avg / 1e6 << " ms\n";
        }
    }
};

// глобальный монитор
ThreadMonitor g_monitor;

// Обычная задача с мониторингом
void monitored_task(int input) {
    auto tid = std::this_thread::get_id();
    auto start = std::chrono::high_resolution_clock::now();

    // имитация работы и случайный сбой
    bool success = true;
    try {
        if(input % 25 == 0) throw std::runtime_error("Simulated error");
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (input % 5)));
    } catch(...) {
        success = false;
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();

    g_monitor.record_task(tid, ns, success);
}

int main() {
    constexpr int NUM_THREADS = 4;
    constexpr int NUM_TASKS = 60;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t]{
            for (int i = 0; i < NUM_TASKS; ++i) {
                monitored_task(t * NUM_TASKS + i);
            }
        });
    }

    for (auto& th : threads) th.join();

    g_monitor.print_report();
    return 0;
}

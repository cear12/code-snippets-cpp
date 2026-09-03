// Per-Thread Performance Monitor idiom: a mutex-protected map lets any
// number of worker threads safely record task timings; print_report()
// summarizes them once work is done.
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

class ThreadMonitor {
public:
  struct ThreadStats {
    std::uint64_t total_tasks_ = 0;
    std::uint64_t failed_tasks_ = 0;
    std::uint64_t total_exec_time_ns_ = 0;
  };

  void RecordTask(std::thread::id tid, std::uint64_t exec_time_ns,
                  bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto &s = stats_[tid];
    s.total_tasks_++;
    if (!success)
      s.failed_tasks_++;
    s.total_exec_time_ns_ += exec_time_ns;
  }

  void PrintReport() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Thread Performance Report:\n";
    for (const auto &[tid, s] : stats_) {
      double avg_ms =
          s.total_tasks_
              ? (s.total_exec_time_ns_ / double(s.total_tasks_)) / 1e6
              : 0.0;
      std::cout << "  Thread " << tid << ": " << s.total_tasks_ << " tasks, "
                << s.failed_tasks_ << " failed, avg exec " << std::fixed
                << std::setprecision(2) << avg_ms << " ms\n";
    }
  }

private:
  std::mutex mutex_;
  std::map<std::thread::id, ThreadStats> stats_;
};

void MonitoredTask(ThreadMonitor &monitor, int input) {
  auto tid = std::this_thread::get_id();
  auto start = std::chrono::high_resolution_clock::now();

  bool success = true;
  try {
    if (input % 25 == 0)
      throw std::runtime_error("Simulated error");
    std::this_thread::sleep_for(std::chrono::milliseconds(1 + (input % 5)));
  } catch (...) {
    success = false;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  monitor.RecordTask(tid, static_cast<std::uint64_t>(ns), success);
}

int main() {
  constexpr int kNumThreads = 4;
  constexpr int kTasksPerThread = 20;

  ThreadMonitor monitor;
  std::vector<std::thread> threads;

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&monitor, t] {
      for (int i = 0; i < kTasksPerThread; ++i) {
        MonitoredTask(monitor, t * kTasksPerThread + i);
      }
    });
  }
  for (auto &th : threads)
    th.join();

  monitor.PrintReport();
  return 0;
}

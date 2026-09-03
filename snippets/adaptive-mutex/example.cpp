// Self-tuning adaptive mutex: optimistic CAS, then adaptive spin with
// exponential backoff, then sleeping wait -- statistics from each period
// adjust the spin/sleep parameters for the next one. See README.md for
// the real bug fixed (a const member function mutating non-mutable
// state) and a documented design limitation (no real OS-level wait/wake
// backing kLockedWithWaiters).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class AdaptiveMutex {
private:
  enum class State : uint32_t {
    kUnlocked = 0,
    kLocked = 1,
    kLockedWithWaiters = 2
  };

  std::atomic<uint32_t> state_{static_cast<uint32_t>(State::kUnlocked)};

  // Statistics driving adaptation.
  mutable std::atomic<uint64_t> total_acquisitions_{0};
  mutable std::atomic<uint64_t> spin_acquisitions_{0};
  mutable std::atomic<uint64_t> sleep_acquisitions_{0};
  mutable std::atomic<uint64_t> total_contention_time_{0}; // nanoseconds

  // Adaptive parameters.
  mutable std::atomic<uint32_t> spin_limit_{1000};
  mutable std::atomic<uint32_t> base_sleep_duration_{1}; // microseconds

  mutable std::atomic<uint64_t> last_adaptation_time_{0};
  static constexpr uint64_t kAdaptationIntervalNs = 1'000'000'000; // 1 second

  thread_local static uint64_t g_local_spin_count;
  thread_local static std::mt19937 g_rng;

  void AdaptParameters() const {
    auto now = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    uint64_t last_time = last_adaptation_time_.load(std::memory_order_relaxed);

    if (now - last_time < kAdaptationIntervalNs) {
      return;
    }
    if (!last_adaptation_time_.compare_exchange_weak(
            last_time, now, std::memory_order_relaxed)) {
      return; // Another thread is already adapting.
    }

    uint64_t total = total_acquisitions_.load(std::memory_order_relaxed);
    uint64_t spin_success = spin_acquisitions_.load(std::memory_order_relaxed);
    uint64_t avg_contention =
        total > 0
            ? total_contention_time_.load(std::memory_order_relaxed) / total
            : 0;

    if (total < 100) {
      return; // Not enough samples yet.
    }

    double spin_success_rate =
        static_cast<double>(spin_success) / static_cast<double>(total);

    if (spin_success_rate > 0.8) {
      uint32_t current_limit = spin_limit_.load(std::memory_order_relaxed);
      spin_limit_.store(std::min(current_limit * 2, 10000u),
                        std::memory_order_relaxed);
    } else if (spin_success_rate < 0.3) {
      uint32_t current_limit = spin_limit_.load(std::memory_order_relaxed);
      spin_limit_.store(std::max(current_limit / 2, 100u),
                        std::memory_order_relaxed);
    }

    if (avg_contention > 100'000) { // > 100 microseconds
      uint32_t current_sleep =
          base_sleep_duration_.load(std::memory_order_relaxed);
      base_sleep_duration_.store(std::min(current_sleep * 2, 1000u),
                                 std::memory_order_relaxed);
    } else if (avg_contention < 10'000) { // < 10 microseconds
      uint32_t current_sleep =
          base_sleep_duration_.load(std::memory_order_relaxed);
      base_sleep_duration_.store(std::max(current_sleep / 2, 1u),
                                 std::memory_order_relaxed);
    }

    total_acquisitions_.store(0, std::memory_order_relaxed);
    spin_acquisitions_.store(0, std::memory_order_relaxed);
    sleep_acquisitions_.store(0, std::memory_order_relaxed);
    total_contention_time_.store(0, std::memory_order_relaxed);
  }

  // BUG FIX: was declared `const`, but mutates state_ (not `mutable`)
  // via compare_exchange_weak -- doesn't compile, and every caller is
  // already non-const. See README.md.
  bool TrySpinLock() {
    uint32_t expected = static_cast<uint32_t>(State::kUnlocked);
    return state_.compare_exchange_weak(
        expected, static_cast<uint32_t>(State::kLocked),
        std::memory_order_acquire, std::memory_order_relaxed);
  }

  void ExponentialBackoff(uint32_t &backoff_count) const {
    uint32_t delay = std::min(1u << std::min(backoff_count, 10u), 1024u);
    uint32_t jitter = g_rng() % (delay + 1);

    for (uint32_t i = 0; i < delay + jitter; ++i) {
      std::this_thread::yield();
    }
    ++backoff_count;
  }

public:
  AdaptiveMutex() = default;
  ~AdaptiveMutex() = default;

  AdaptiveMutex(const AdaptiveMutex &) = delete;
  AdaptiveMutex &operator=(const AdaptiveMutex &) = delete;

  void Lock() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Phase 1: optimistic try, no spinning.
    if (TrySpinLock()) {
      total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    // Phase 2: adaptive spin.
    uint32_t spin_count = 0;
    uint32_t spin_limit = spin_limit_.load(std::memory_order_relaxed);
    uint32_t backoff_count = 0;

    while (spin_count < spin_limit) {
      uint32_t current_state = state_.load(std::memory_order_relaxed);
      if (current_state == static_cast<uint32_t>(State::kUnlocked)) {
        if (TrySpinLock()) {
          auto duration =
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  std::chrono::high_resolution_clock::now() - start_time)
                  .count();

          total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
          spin_acquisitions_.fetch_add(1, std::memory_order_relaxed);
          total_contention_time_.fetch_add(static_cast<uint64_t>(duration),
                                           std::memory_order_relaxed);

          if ((total_acquisitions_.load() & 0xFF) == 0) {
            AdaptParameters();
          }
          return;
        }
      }

      ExponentialBackoff(backoff_count);
      ++spin_count;
      ++g_local_spin_count;
    }

    // Phase 3: sleeping wait.
    uint32_t sleep_duration =
        base_sleep_duration_.load(std::memory_order_relaxed);
    uint32_t sleep_backoff = 0;

    while (true) {
      uint32_t expected = static_cast<uint32_t>(State::kUnlocked);
      if (state_.compare_exchange_weak(
              expected, static_cast<uint32_t>(State::kLocked),
              std::memory_order_acquire, std::memory_order_relaxed)) {
        auto duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now() - start_time)
                .count();

        total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
        sleep_acquisitions_.fetch_add(1, std::memory_order_relaxed);
        total_contention_time_.fetch_add(static_cast<uint64_t>(duration),
                                         std::memory_order_relaxed);
        return;
      }

      if (expected == static_cast<uint32_t>(State::kLocked)) {
        state_.compare_exchange_weak(
            expected, static_cast<uint32_t>(State::kLockedWithWaiters),
            std::memory_order_relaxed, std::memory_order_relaxed);
      }

      // See README's "known limitation": this is a timed poll, not
      // a real wake-on-unlock.
      uint32_t actual_sleep =
          sleep_duration * (1u << std::min(sleep_backoff, 6u));
      std::this_thread::sleep_for(std::chrono::microseconds(actual_sleep));
      ++sleep_backoff;
    }
  }

  bool TryLock() {
    bool success = TrySpinLock();
    if (success) {
      total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
    }
    return success;
  }

  template <typename Rep, typename Period>
  bool TryLockFor(const std::chrono::duration<Rep, Period> &timeout_duration) {
    auto timeout_point = std::chrono::steady_clock::now() + timeout_duration;

    if (TrySpinLock()) {
      total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
      return true;
    }

    uint32_t backoff_count = 0;
    while (std::chrono::steady_clock::now() < timeout_point) {
      if (TrySpinLock()) {
        total_acquisitions_.fetch_add(1, std::memory_order_relaxed);
        return true;
      }
      ExponentialBackoff(backoff_count);
    }
    return false;
  }

  void Unlock() {
    uint32_t expected = static_cast<uint32_t>(State::kLocked);
    if (state_.compare_exchange_strong(
            expected, static_cast<uint32_t>(State::kUnlocked),
            std::memory_order_release, std::memory_order_relaxed)) {
      return;
    }

    expected = static_cast<uint32_t>(State::kLockedWithWaiters);
    if (state_.compare_exchange_strong(
            expected, static_cast<uint32_t>(State::kUnlocked),
            std::memory_order_release, std::memory_order_relaxed)) {
      // Waiters re-poll state_ themselves; see README's known limitation.
      return;
    }

    // state_ changed under us during unlock -- should not happen for a
    // correctly paired lock()/unlock(); left as a silent no-op rather
    // than a hard assert so a misuse doesn't crash a demo build.
  }

  struct MutexStatistics {
    uint64_t total_acquisitions_;
    uint64_t spin_acquisitions_;
    uint64_t sleep_acquisitions_;
    uint64_t average_contention_time_ns_;
    uint32_t current_spin_limit_;
    uint32_t current_sleep_duration_;
    uint64_t thread_local_spins_;
  };

  MutexStatistics GetStatistics() const {
    uint64_t total = total_acquisitions_.load(std::memory_order_relaxed);
    uint64_t contention_time =
        total_contention_time_.load(std::memory_order_relaxed);

    return {total,
            spin_acquisitions_.load(std::memory_order_relaxed),
            sleep_acquisitions_.load(std::memory_order_relaxed),
            total > 0 ? contention_time / total : 0,
            spin_limit_.load(std::memory_order_relaxed),
            base_sleep_duration_.load(std::memory_order_relaxed),
            g_local_spin_count};
  }

  void ResetStatistics() {
    total_acquisitions_.store(0, std::memory_order_relaxed);
    spin_acquisitions_.store(0, std::memory_order_relaxed);
    sleep_acquisitions_.store(0, std::memory_order_relaxed);
    total_contention_time_.store(0, std::memory_order_relaxed);
    g_local_spin_count = 0;
  }
};

thread_local uint64_t AdaptiveMutex::g_local_spin_count = 0;
thread_local std::mt19937 AdaptiveMutex::g_rng{std::random_device{}()};

class AdaptiveLockGuard {
public:
  explicit AdaptiveLockGuard(AdaptiveMutex &mutex) : mutex_(mutex) {
    mutex_.Lock();
  }
  ~AdaptiveLockGuard() { mutex_.Unlock(); }

  AdaptiveLockGuard(const AdaptiveLockGuard &) = delete;
  AdaptiveLockGuard &operator=(const AdaptiveLockGuard &) = delete;

private:
  AdaptiveMutex &mutex_;
};

int main() {
  AdaptiveMutex mutex;
  long shared_counter = 0;

  constexpr int kThreads = 8;
  constexpr int kIncrementsPerThread = 20000;

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&mutex, &shared_counter] {
      for (int i = 0; i < kIncrementsPerThread; ++i) {
        AdaptiveLockGuard guard(mutex);
        ++shared_counter;
      }
    });
  }
  for (auto &th : threads)
    th.join();

  constexpr long kExpected = static_cast<long>(kThreads) * kIncrementsPerThread;
  std::cout << "shared_counter = " << shared_counter << " (expected "
            << kExpected
            << "): " << (shared_counter == kExpected ? "PASS" : "FAIL") << "\n";

  auto stats = mutex.GetStatistics();
  std::cout << "Total acquisitions: " << stats.total_acquisitions_ << "\n";
  std::cout << "  via spin:  " << stats.spin_acquisitions_ << "\n";
  std::cout << "  via sleep: " << stats.sleep_acquisitions_ << "\n";
  std::cout << "Final spin_limit: " << stats.current_spin_limit_
            << ", sleep_duration: " << stats.current_sleep_duration_ << " us\n";

  return shared_counter == kExpected ? 0 : 1;
}

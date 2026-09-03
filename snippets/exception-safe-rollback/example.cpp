// ScopeFail idiom: runs its cleanup function only if the enclosing scope
// is unwinding because of a NEW exception, using std::uncaught_exceptions()
// (C++17) to tell "unwinding from a fresh throw" apart from "returning
// normally" or "already unwinding for an unrelated, older exception".
#include <exception>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

class ScopeFail {
public:
  explicit ScopeFail(std::function<void()> f)
      : exception_count_(std::uncaught_exceptions()), func_(std::move(f)) {}

  ~ScopeFail() noexcept {
    if (active_ && std::uncaught_exceptions() > exception_count_) {
      func_();
    }
  }

  void Dismiss() { active_ = false; }

  ScopeFail(const ScopeFail &) = delete;
  ScopeFail &operator=(const ScopeFail &) = delete;

private:
  int exception_count_;
  bool active_ = true;
  std::function<void()> func_;
};

void Example(bool should_throw) {
  std::vector<int> data;
  data.push_back(42);

  ScopeFail rollback_guard{[&]() {
    std::cout << "Rolling back changes!\n";
    data.pop_back();
  }};

  if (should_throw) {
    throw std::runtime_error("Something went wrong!");
  }

  rollback_guard.Dismiss(); // reached only on the success path
  std::cout << "Operation succeeded!\n";
}

int main() {
  try {
    Example(true); // triggers the rollback
  } catch (...) {
    std::cout << "caught the exception in main\n";
  }

  Example(false); // succeeds, no rollback

  return 0;
}

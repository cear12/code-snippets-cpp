// Null Object idiom returned as a non-owning shared_ptr: makeNullLogger()
// hands out a shared_ptr<Logger> that aliases a static NullLogger and
// shares an *empty* shared_ptr<void>'s (no-op) control block, so
// destroying it never tries to delete the static singleton.
#include <iostream>
#include <memory>

struct Logger {
  virtual ~Logger() = default;
  virtual void Log(const char *msg) = 0;
};

struct NullLogger : Logger {
  void Log(const char *) override { /* does nothing, on purpose */ }
};

Logger &GetNullLogger() noexcept {
  static NullLogger null_logger{};
  return null_logger;
}

std::shared_ptr<Logger> MakeNullLogger() {
  // shared_ptr<void>{} is empty (no control block, no ownership); the
  // aliasing constructor makes the result share that "owns nothing"
  // state while pointing at the real (static) NullLogger instance.
  return std::shared_ptr<Logger>(std::shared_ptr<void>{}, &GetNullLogger());
}

int main() {
  std::shared_ptr<Logger> logger = MakeNullLogger();
  logger->Log("this goes nowhere");
  std::cout << "logger.use_count() = " << logger.use_count()
            << " (0: owns nothing)\n";
  return 0; // destroying `logger` here does NOT delete GetNullLogger()'s static
            // object
}

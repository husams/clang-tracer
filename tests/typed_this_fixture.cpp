#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <thread>

#include <unistd.h>

namespace {
volatile std::sig_atomic_t keep_running = 1;
void stop_target(int) { keep_running = 0; }
} // namespace

class TypedThisBase {
public:
  int base_value = 13;
};

class InheritedCalculator : public TypedThisBase {
public:
  int bias = 5;

  __attribute__((visibility("default"), noinline)) int read(int value) const {
    return base_value + bias + value;
  }
};

class PointerCalculator {
public:
  wchar_t *text = nullptr;
  int bias = 5;

  __attribute__((visibility("default"), noinline)) int read(int value) const;
};

int PointerCalculator::read(int value) const {
  timespec stamp{};
  ::clock_gettime(CLOCK_MONOTONIC, &stamp);
  volatile long observed_nanoseconds = stamp.tv_nsec;
  (void)observed_nanoseconds;
  return value + bias + (text && *text != L'\0' ? 1 : 0);
}

int main() {
  std::signal(SIGINT, stop_target);
  std::signal(SIGTERM, stop_target);
  InheritedCalculator calculator;
  wchar_t sample[] = L"T";
  PointerCalculator pointer_calculator;
  pointer_calculator.text = sample;
  std::printf("TYPED_THIS_FIXTURE pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  while (keep_running != 0) {
    volatile int result = calculator.read(1);
    volatile int pointer_result = pointer_calculator.read(2);
    (void)result;
    (void)pointer_result;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

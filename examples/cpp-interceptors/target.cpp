#include <cerrno>
#include <csignal>
#include <cstdio>
#include <ctime>

#include <unistd.h>

namespace {
volatile std::sig_atomic_t keep_running = 1;
int initialization_count = [] {
  std::puts("INITIALIZER fired");
  std::fflush(stdout);
  return 1;
}();

void stop_target(int) { keep_running = 0; }
} // namespace

int add(int left, int right) {
  volatile int saved_left = left;
  volatile int result = saved_left + right;
  return result;
}

class Calculator {
public:
  int bias = 7;
  mutable int calls = 0;

  int multiply(int value, int factor) const {
    ++calls;
    volatile int saved_value = value;
    volatile int result = saved_value * factor + bias + calls;
    return result;
  }

private:
  int hidden = 99;
};

int main() {
  std::signal(SIGINT, stop_target);
  std::signal(SIGTERM, stop_target);

  const Calculator calculator;
  int iteration = 1;
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);

  while (keep_running != 0) {
    const int left = iteration;
    const int right = 10 + iteration;
    const int factor = 2 + iteration;
    const int sum = add(left, right);
    const int product = calculator.multiply(left, factor);
    std::printf("CALL init=%d left=%d right=%d add=%d value=%d factor=%d bias=%d calls=%d multiply=%d\n",
                initialization_count, left, right, sum, left, factor,
                calculator.bias, calculator.calls, product);
    std::fflush(stdout);

    iteration = iteration == 5 ? 1 : iteration + 1;
    timespec delay{0, 200'000'000};
    while (keep_running != 0 && ::nanosleep(&delay, &delay) != 0 &&
           errno == EINTR) {
    }
  }

  std::puts("TARGET stopped");
}

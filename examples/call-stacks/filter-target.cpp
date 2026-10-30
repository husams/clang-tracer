#include "calculator.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
static_assert(std::atomic<bool>::is_always_lock_free);
std::atomic<bool> running{true};
std::atomic<unsigned> failures{0};
std::mutex output;
bool partial_stack = false;
std::atomic<bool> ready{true};
void stop(int) { running = 0; }
void start(int) { ready = 1; }
} // namespace

int no_cfi_iteration(StackCalculator &, int, int);

int run_iteration(StackCalculator &calculator, const std::vector<int> &values) {
  const int answer =
      library_dispatch(calculator, values.front(), values.back());
  return answer;
}

void worker_loop(int worker) {
  StackCalculator calculator;
  calculator.bias += worker;
  while (running) {
    const int value = worker + 2, factor = worker + 3;
    const std::vector<int> values{value, factor};
    const int answer = partial_stack
                           ? no_cfi_iteration(calculator, value, factor)
                           : run_iteration(calculator, values);
    if (answer != value * factor + calculator.bias + calculator.calls)
      ++failures;
    {
      std::lock_guard lock(output);
      std::printf("CALL worker=%d value=%d factor=%d bias=%d calls=%d "
                  "result=%d failures=%u\n",
                  worker, value, factor, calculator.bias, calculator.calls,
                  answer, failures.load());
      std::fflush(stdout);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    partial_stack |= std::string_view(argv[i]) == "--partial";
    if (std::string_view(argv[i]) == "--wait")
      ready = 0;
  }
  std::signal(SIGTERM, stop);
  std::signal(SIGINT, stop);
  std::signal(SIGUSR1, start);
  std::printf("TARGET pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  while (running && !ready)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::vector<std::thread> workers;
  for (int i = 0; i < 3; ++i)
    workers.emplace_back(worker_loop, i);
  while (running)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  for (auto &worker : workers)
    worker.join();
  std::printf("STOP failures=%u\n", failures.load());
  return failures.load() ? 1 : 0;
}

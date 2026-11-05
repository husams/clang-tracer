#include "fixtures/repl_complex_types.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>
#include <unistd.h>

namespace repl_fixture {
__attribute__((visibility("default"), noinline)) bool
string_argument(const std::string &value) {
  volatile int padding_a = 0, padding_b = 0;
  return value.empty() && padding_a == padding_b;
}
__attribute__((visibility("default"), noinline)) std::string
string_result(const std::string &value) {
  volatile int padding_a = 0, padding_b = 0;
  (void)padding_a;
  (void)padding_b;
  return value + "-returned";
}
__attribute__((visibility("default"), noinline)) bool
mutate_argument(Record &value) {
  volatile int padding_a = 0, padding_b = 0;
  value.label.clear();
  value.label.shrink_to_fit();
  value.samples.clear();
  value.id = 99;
  return padding_a == padding_b;
}
__attribute__((visibility("default"), noinline)) Record
round_trip(Record value) {
  volatile int padding_a = 0, padding_b = 0;
  value.id += 1 + padding_a + padding_b;
  value.location.name += "-returned";
  value.tags["key"] = 42;
  value.serial.reset();
  return value;
}
} // namespace repl_fixture

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
} // namespace
int main() {
  std::signal(SIGTERM, stop);
  std::signal(SIGINT, stop);
  std::printf("COMPLEX_FIXTURE pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  const std::string values[] = {
      "", "short", "heap-backed string longer than SSO",
      std::string("a\0\xffz", 4), std::string(8192, 'x')};
  while (running) {
    for (const auto &text : values) {
      volatile bool b = repl_fixture::string_argument(text);
      (void)b;
    }
    volatile auto size = repl_fixture::string_result("return-input").size();
    (void)size;
    repl_fixture::Record value;
    auto result = repl_fixture::round_trip(value);
    (void)result;
    volatile bool b = repl_fixture::mutate_argument(value);
    (void)b;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

#include <atomic>
#include <cstdio>
#include <thread>
#include <fcntl.h>
#include <unistd.h>

namespace {
std::atomic<bool> hold_call{false}, release_call{false};
}

__attribute__((noinline)) int observed(int input) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  if (input == 1) {
    std::puts("call held");
    std::fflush(stdout);
    while (!release_call.load(std::memory_order_relaxed))
      ::usleep(10000);
  }
  return input + 10;
}

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1)
    return 1;
  std::thread([] {
    while (!hold_call.load(std::memory_order_relaxed))
      ::usleep(10000);
    std::printf("held call returned %d\n", observed(1));
    std::fflush(stdout);
  }).detach();
  std::printf("ready entry=%p\n", reinterpret_cast<void *>(&observed));
  std::fflush(stdout);
  for (;;) {
    char command = 0;
    (void)::read(STDIN_FILENO, &command, 1);
    if (command == 'h')
      hold_call.store(true, std::memory_order_relaxed);
    else if (command == 'r')
      release_call.store(true, std::memory_order_relaxed);
    else if (command == 's') {
      std::printf("short call returned %d\n", observed(2));
      std::fflush(stdout);
    }
    ::usleep(10000);
  }
}

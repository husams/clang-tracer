#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include <dlfcn.h>

namespace {
std::atomic<bool> toggle_module{false};
static_assert(std::atomic<bool>::is_always_lock_free);

void request_toggle(int) { toggle_module.store(true, std::memory_order_relaxed); }

void sleep_briefly() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
} // namespace

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3)
    return 1;
  if (argc == 3 && !::dlopen(argv[2], RTLD_NOW | RTLD_LOCAL))
    return 3;
  std::signal(SIGUSR1, request_toggle);
  std::thread([] {
    for (;;)
      sleep_briefly();
  }).detach();
  void *module = nullptr;
  toggle_module = true;
  for (;;) {
    if (toggle_module.exchange(false, std::memory_order_relaxed)) {
      if (module) {
        if (::dlclose(module) != 0)
          return 2;
        module = nullptr;
      } else {
        module = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module)
          return 3;
      }
      std::puts(module ? "loaded" : "unloaded");
      std::fflush(stdout);
    }
    sleep_briefly();
  }
}

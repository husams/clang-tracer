#include <dlfcn.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
template <typename Function>
Function symbol(void *image, const char *name) {
  if (auto *address = ::dlsym(image, name))
    return reinterpret_cast<Function>(address);
  throw std::runtime_error(::dlerror());
}

void check_guard(void *image, bool abort) {
  const auto acquire = symbol<int (*)(std::uint64_t *)>(image, "__cxa_guard_acquire");
  const auto release = symbol<void (*)(std::uint64_t *)>(image, "__cxa_guard_release");
  const auto reset = symbol<void (*)(std::uint64_t *)>(image, "__cxa_guard_abort");
  std::uint64_t guard = 0;
  if (acquire(&guard) != 1 || *reinterpret_cast<unsigned char *>(&guard) != 0)
    throw std::runtime_error("guard advertises initialization before release");

  std::atomic<bool> waiting{false}, initialized{false};
  int acquired = -1;
  bool observed_initialization = false;
  std::thread waiter([&] {
    waiting.store(true, std::memory_order_release);
    acquired = acquire(&guard);
    observed_initialization = initialized.load(std::memory_order_relaxed);
    if (acquired)
      release(&guard);
  });
  while (!waiting.load(std::memory_order_acquire))
    std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  if (abort) {
    reset(&guard);
  } else {
    initialized.store(true, std::memory_order_relaxed);
    release(&guard);
  }
  waiter.join();
  if (acquired != (abort ? 1 : 0) || observed_initialization == abort ||
      *reinterpret_cast<unsigned char *>(&guard) != 1 || acquire(&guard) != 0)
    throw std::runtime_error("guard did not publish or retry initialization");
}

void check_finalizers(void *image) {
  const auto add = symbol<int (*)(void (*)(void *), void *, void *)>(
      image, "__cxa_atexit");
  const auto finalize = symbol<void (*)(void *)>(image, "__cxa_finalize");
  std::array<std::atomic<unsigned>, 64> calls{};
  std::array<std::thread, 8> workers;
  std::atomic<bool> registration_failed{false};
  for (std::size_t worker = 0; worker < workers.size(); ++worker) {
    workers[worker] = std::thread([&, worker] {
      for (std::size_t i = worker; i < calls.size(); i += workers.size()) {
        if (add([](void *entry) {
              static_cast<std::atomic<unsigned> *>(entry)->fetch_add(1);
            }, &calls[i], &calls) != 0)
          registration_failed.store(true);
      }
    });
  }
  for (auto &worker : workers)
    worker.join();
  for (auto &worker : workers)
    worker = std::thread([&] { finalize(&calls); });
  for (auto &worker : workers)
    worker.join();
  if (registration_failed.load())
    throw std::runtime_error("private finalizer registration failed");
  for (const auto &count : calls)
    if (count.load() != 1)
      throw std::runtime_error("private finalizer was lost or ran twice");
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("expected private runtime image path");
    auto *image = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image)
      throw std::runtime_error(::dlerror());
    check_guard(image, false);
    check_guard(image, true);
    check_finalizers(image);
    ::dlclose(image);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

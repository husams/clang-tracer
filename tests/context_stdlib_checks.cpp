#include "context_stdlib.hpp"
#include <atomic>
#include <llvm/Support/Error.h>
#include <stdexcept>
#include <thread>
#include <vector>

void verify_context_stdlib() {
  auto image = tracer::ContextStdlibImage::acquire();
#if defined(__linux__) && defined(__x86_64__)
  if (!image)
    throw std::runtime_error(llvm::toString(image.takeError()));
  if (!(*image)->validate_runtime())
    throw std::runtime_error("private standard-library "
                             "allocator/exception/erasure validation failed");
  auto allocate = (*image)->symbol("_Znwm");
  auto release = (*image)->symbol("_ZdlPv");
  if (!allocate)
    throw std::runtime_error(llvm::toString(allocate.takeError()));
  if (!release)
    throw std::runtime_error(llvm::toString(release.takeError()));
  const auto before = (*image)->outstanding_allocations();
  void *payload = nullptr;
  {
    tracer::ContextStdlibImage::Frame frame(*image);
    if (!frame.entered())
      throw std::runtime_error("could not reserve initial private frame");
    payload = reinterpret_cast<void *(*)(std::size_t)>(*allocate)(1024);
  }
  std::atomic<unsigned> ready{0}, entered{0};
  std::atomic<bool> finish{false};
  std::vector<std::thread> threads;
  struct Join final {
    std::atomic<bool> &finish;
    std::vector<std::thread> &threads;
    ~Join() {
      finish.store(true, std::memory_order_release);
      for (auto &thread : threads)
        thread.join();
    }
  } join{finish, threads};
  for (unsigned i = 0; i < 64; ++i)
    threads.emplace_back([&] {
      tracer::ContextStdlibImage::Frame frame(*image);
      if (frame.entered())
        entered.fetch_add(1, std::memory_order_relaxed);
      ready.fetch_add(1, std::memory_order_release);
      while (!finish.load(std::memory_order_acquire))
        std::this_thread::yield();
    });
  while (ready.load(std::memory_order_acquire) != 64)
    std::this_thread::yield();
  tracer::ContextStdlibImage::Frame unavailable(*image);
  if (entered.load() != 64 || unavailable.entered())
    throw std::runtime_error("exception lane exhaustion was not enforced");
  {
    tracer::ContextStdlibImage::Frame cleanup(
        *image, nullptr, nullptr,
        tracer::ContextStdlibImage::Frame::Purpose::cleanup);
    if (!cleanup.entered())
      throw std::runtime_error(
          "pending finalization depended on an exception lane");
    reinterpret_cast<void (*)(void *)>(*release)(payload);
  }
  if ((*image)->outstanding_allocations() != before)
    throw std::runtime_error(
        "cleanup-only frame did not release private storage");
#else
  if (image)
    throw std::runtime_error(
        "unpackaged host admitted private standard library");
  llvm::consumeError(image.takeError());
#endif
}

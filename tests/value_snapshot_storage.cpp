#include "value_snapshot_storage.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    const auto outer = tracer_snapshot_mark();
    std::size_t capacity = 0;
    auto *first = tracer_snapshot_storage(&capacity);
    require(capacity == 4096 && tracer_snapshot_mark() == outer + capacity,
            "the full writer extent is reserved before encoding");
    for (std::size_t i = 0; i < 64; ++i)
      first[i] = std::byte{0x5a};
    const auto nested = tracer_snapshot_mark();
    auto *second = tracer_snapshot_storage(&capacity);
    require(second >= first + 4096,
            "nested capture cannot reuse a partial writer");
    for (std::size_t i = 0; i < 64; ++i)
      second[i] = std::byte{0x33};
    tracer_snapshot_commit(nested, 64);
    tracer_snapshot_reset(nested);
    for (std::size_t i = 0; i < 64; ++i)
      require(first[i] == std::byte{0x5a},
              "nested capture preserved outer bytes");
    tracer_snapshot_commit(outer, 64);
    require(tracer_snapshot_mark() == outer + 64,
            "commit releases unused reservation");
    std::atomic<bool> independent{false};
    std::thread thread([&] {
      independent = tracer_snapshot_mark() == 0;
      std::size_t available = 0;
      (void)tracer_snapshot_storage(&available);
      independent = independent && available == 4096;
      tracer_snapshot_reset(0);
    });
    thread.join();
    require(independent && tracer_snapshot_mark() == outer + 64,
            "capture storage and cursors are thread-local");
    while (capacity)
      (void)tracer_snapshot_storage(&capacity);
    require(tracer_snapshot_mark() == 16 * 1024, "arena exhaustion is bounded");
    tracer_snapshot_reset(outer);
    require(tracer_snapshot_mark() == outer,
            "cleanup restores storage after exhaustion");
    std::cout << "nested snapshot storage checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

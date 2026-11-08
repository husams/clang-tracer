#include "value_snapshot_storage.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <limits>

namespace {
constexpr std::size_t invalid_mark = std::numeric_limits<std::size_t>::max();
thread_local std::array<std::byte, 16 * 1024> storage;
thread_local std::atomic<std::size_t> used{0};
thread_local std::atomic<bool> updating{false};
static_assert(std::atomic<std::size_t>::is_always_lock_free &&
                  std::atomic<bool>::is_always_lock_free,
              "snapshot signal reentry requires lock-free TLS bookkeeping");
} // namespace

extern "C" std::size_t tracer_snapshot_mark() noexcept {
  return updating.load(std::memory_order_acquire)
             ? invalid_mark
             : used.load(std::memory_order_relaxed);
}
extern "C" void tracer_snapshot_reset(std::size_t mark) noexcept {
  if (mark == invalid_mark ||
      updating.exchange(true, std::memory_order_acquire))
    return;
  if (mark <= used.load(std::memory_order_relaxed))
    used.store(mark, std::memory_order_relaxed);
  updating.store(false, std::memory_order_release);
}
extern "C" std::byte *tracer_snapshot_storage(std::size_t *capacity) noexcept {
  if (updating.exchange(true, std::memory_order_acquire)) {
    *capacity = 0;
    return storage.data() + storage.size();
  }
  const auto begin = used.load(std::memory_order_relaxed);
  *capacity = std::min<std::size_t>(4096, storage.size() - begin);
  used.store(begin + *capacity, std::memory_order_relaxed);
  updating.store(false, std::memory_order_release);
  return storage.data() + begin;
}
extern "C" void tracer_snapshot_commit(std::size_t mark,
                                       std::size_t bytes) noexcept {
  if (mark == invalid_mark ||
      updating.exchange(true, std::memory_order_acquire))
    return;
  const auto end = used.load(std::memory_order_relaxed);
  if (mark <= end && bytes <= end - mark)
    used.store(mark + bytes, std::memory_order_relaxed);
  updating.store(false, std::memory_order_release);
}

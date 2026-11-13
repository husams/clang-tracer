#include "value_snapshot_storage.hpp"
#include "linux_syscalls.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {
constexpr std::size_t invalid_mark = std::numeric_limits<std::size_t>::max();
thread_local std::array<std::byte, 2 * tracer::max_snapshot_bytes> storage;
thread_local std::atomic<std::size_t> used{0};
thread_local std::atomic<bool> updating{false};
static_assert(std::atomic<std::size_t>::is_always_lock_free &&
                  std::atomic<bool>::is_always_lock_free,
              "snapshot signal reentry requires lock-free TLS bookkeeping");

struct PreserveErrno {
  int value = errno;
  ~PreserveErrno() { errno = value; }
};

bool read_snapshot_memory(std::uintptr_t address, void *destination,
                          std::size_t size) noexcept {
  if (!address || !size || size - 1 >
      std::numeric_limits<std::uintptr_t>::max() - address)
    return false;
  iovec local{destination, size};
  iovec remote{reinterpret_cast<void *>(address), size};
  return tracer::linux_api::process_vm_readv(::getpid(), &local, 1,
                                             &remote, 1, 0) ==
         static_cast<ssize_t>(size);
}

bool readable_object(std::uintptr_t address, std::size_t size) noexcept {
  if (size - 1 > std::numeric_limits<std::uintptr_t>::max() - address)
    return false;
  const auto last = address + size - 1;
  const auto system_page_size = ::getpagesize();
  if (system_page_size <= 0)
    return false;
  const auto page_size = static_cast<std::uintptr_t>(system_page_size);
  const auto pid = ::getpid();
  constexpr std::size_t pages_per_probe = 16;
  std::array<std::byte, pages_per_probe> bytes{};
  std::array<iovec, pages_per_probe> remote{};
  // Read one byte from each page of the candidate record. Batch the probes
  // so large records do not require one syscall per page.
  while (address) {
    std::size_t count = 0;
    while (address && count < remote.size()) {
      remote[count++] = {reinterpret_cast<void *>(address), 1};
      const auto advance = page_size - address % page_size;
      address = last - address < advance ? 0 : address + advance;
    }
    iovec local{bytes.data(), count};
    if (tracer::linux_api::process_vm_readv(pid, &local, 1,
                                           remote.data(), count, 0) !=
        static_cast<ssize_t>(count))
      return false;
  }
  return true;
}
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
extern "C" std::byte *tracer_snapshot_storage(std::size_t *capacity, std::size_t requested) noexcept {
  if (updating.exchange(true, std::memory_order_acquire)) {
    *capacity = 0;
    return storage.data() + storage.size();
  }
  const auto begin = used.load(std::memory_order_relaxed);
  *capacity = std::min({requested, tracer::max_snapshot_bytes, storage.size() - begin});
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

extern "C" bool tracer_snapshot_read_rtti(const void *base,
                                          const void **complete,
                                          const char **type_name) noexcept {
  const void *type_info = nullptr, *vtable = nullptr;
  return tracer_snapshot_read_rtti_provider(base, complete, type_name, &type_info, &vtable);
}

extern "C" bool tracer_snapshot_readable(const void *pointer, std::size_t size,
                                           std::size_t alignment) noexcept {
  const PreserveErrno preserve_errno;
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  return address && size && alignment && address % alignment == 0 && readable_object(address, size);
}

extern "C" bool tracer_snapshot_read_rtti_provider(const void *base, const void **complete,
    const char **type_name, const void **type_info, const void **table) noexcept {
  const PreserveErrno preserve_errno;
  const auto address = reinterpret_cast<std::uintptr_t>(base);
  struct VTablePrefix {
    std::ptrdiff_t offset_to_top;
    std::uintptr_t type_info;
  } prefix{};
  struct TypeInfoHeader {
    std::uintptr_t vtable;
    std::uintptr_t name;
  } type{};
  static_assert(sizeof(VTablePrefix) == 2 * sizeof(std::uintptr_t));
  std::uintptr_t vtable = 0;
  if (!complete || !type_name || !type_info || !table ||
      !read_snapshot_memory(address, &vtable, sizeof(vtable)) ||
      vtable < sizeof(prefix) || vtable % alignof(std::uintptr_t) != 0 ||
      !read_snapshot_memory(vtable - sizeof(prefix), &prefix, sizeof(prefix)) ||
      prefix.offset_to_top > 0 ||
      prefix.type_info % alignof(std::uintptr_t) != 0 ||
      !read_snapshot_memory(prefix.type_info, &type, sizeof(type)) ||
      !type.vtable || !type.name)
    return false;
  const auto distance = std::uintptr_t{0} -
                        static_cast<std::uintptr_t>(prefix.offset_to_top);
  if (distance >= address)
    return false;
  *complete = reinterpret_cast<const void *>(address - distance);
  *type_name = reinterpret_cast<const char *>(type.name);
  *type_info = reinterpret_cast<const void *>(prefix.type_info);
  *table = reinterpret_cast<const void *>(vtable);
  return true;
}

extern "C" bool tracer_snapshot_rtti_matches(
    const char *type_name, const char *expected, std::size_t expected_len,
    const void *complete, const void *base, std::size_t object_size,
    std::size_t object_alignment) noexcept {
  const PreserveErrno preserve_errno;
  const auto address = reinterpret_cast<std::uintptr_t>(complete);
  const auto base_address = reinterpret_cast<std::uintptr_t>(base);
  if (!address || !expected || !expected_len || !object_alignment ||
      address % object_alignment != 0 || base_address < address ||
      object_size < sizeof(std::uintptr_t) ||
      base_address - address > object_size - sizeof(std::uintptr_t))
    return false;
  auto name = reinterpret_cast<std::uintptr_t>(type_name);
  char first = '\0';
  if (!read_snapshot_memory(name, &first, sizeof(first)))
    return false;
  // libstdc++ may prefix a non-unique RTTI name with '*'.
  if (first == '*') {
    if (name == std::numeric_limits<std::uintptr_t>::max())
      return false;
    ++name;
  }
  if (expected_len > std::numeric_limits<std::uintptr_t>::max() - name)
    return false;
  std::array<char, 128> chunk{};
  for (std::size_t offset = 0; offset < expected_len;) {
    const auto count = std::min(chunk.size(), expected_len - offset);
    if (!read_snapshot_memory(name + offset, chunk.data(), count) ||
        std::memcmp(chunk.data(), expected + offset, count) != 0)
      return false;
    offset += count;
  }
  char end = '\0';
  return read_snapshot_memory(name + expected_len, &end, sizeof(end)) &&
         end == '\0' && readable_object(address, object_size);
}

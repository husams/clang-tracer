#include "value_snapshot_storage.hpp"
#include "linux_syscalls.hpp"
#include "snapshot_runtime_limits.hpp"
#include <sys/mman.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sys/mman.h>

namespace {
constexpr std::size_t invalid_mark = std::numeric_limits<std::size_t>::max();
// The per-thread storage starts as this array and moves to a mapping when a
// probe asks for more (--snapshot-arena), but only while nothing is in use.
constexpr std::size_t default_arena = tracer::kDefaultArenaBytes;
thread_local std::array<std::byte, default_arena> default_storage;
struct Arena {
  std::byte *data = default_storage.data();
  std::size_t size = default_arena;
  ~Arena() {
    if (data != default_storage.data())
      ::munmap(data, size);
  }
};
thread_local Arena arena;
thread_local std::atomic<std::size_t> used{0};
void ensure_arena() noexcept {
  const std::size_t wanted = tracer::snapshot_arena_request().load(std::memory_order_relaxed);
  if (wanted <= arena.size || used.load(std::memory_order_relaxed) != 0)
    return;
  void *mapping = ::mmap(nullptr, wanted, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED)
    return;
  if (arena.data != default_storage.data())
    ::munmap(arena.data, arena.size);
  arena.data = static_cast<std::byte *>(mapping);
  arena.size = wanted;
}
void ensure_scratch() noexcept;
std::size_t slice_bytes() noexcept {
  return tracer::snapshot_slice_request().load(std::memory_order_relaxed);
}
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

extern "C" void tracer_snapshot_configure(unsigned arena_bytes, unsigned slice, unsigned members,
                                          unsigned depth) noexcept {
  // The largest request of any installed probe wins.
  auto raise = [](std::atomic<std::uint32_t> &target, std::uint32_t value) {
    auto current = target.load(std::memory_order_relaxed);
    while (current < value && !target.compare_exchange_weak(current, value)) {
    }
  };
  raise(tracer::snapshot_arena_request(), arena_bytes);
  raise(tracer::snapshot_slice_request(), slice);
  raise(tracer::snapshot_text_request(), slice);
  raise(tracer::snapshot_member_request(), members);
  raise(tracer::snapshot_depth_request(), depth);
}
extern "C" std::size_t tracer_snapshot_mark() noexcept {
  ensure_arena();
  ensure_scratch();
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
    return arena.data + arena.size;
  }
  const auto begin = used.load(std::memory_order_relaxed);
  *capacity = std::min({requested, slice_bytes(), arena.size - begin});
  used.store(begin + *capacity, std::memory_order_relaxed);
  updating.store(false, std::memory_order_release);
  return arena.data + begin;
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

extern "C" bool tracer_snapshot_readable(const void *object, std::size_t size,
                                         std::size_t alignment) noexcept {
  const PreserveErrno preserve_errno;
  const auto address = reinterpret_cast<std::uintptr_t>(object);
  return address && size && alignment && address % alignment == 0 &&
         readable_object(address, size);
}

namespace {
// Text copied out of the target is staged here, not on the caller's stack.
thread_local std::array<char, tracer::kDefaultSliceBytes> default_scratch;
struct Scratch {
  char *data = default_scratch.data();
  std::size_t size = default_scratch.size();
  ~Scratch() {
    if (data != default_scratch.data())
      ::munmap(data, size);
  }
};
thread_local Scratch text_scratch;
// Grows to hold the longest string a probe may capture (--string-bytes).
void ensure_scratch() noexcept {
  const std::size_t wanted = tracer::snapshot_text_request().load(std::memory_order_relaxed) + 1;
  if (wanted <= text_scratch.size)
    return;
  void *mapping = ::mmap(nullptr, wanted, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED)
    return;
  if (text_scratch.data != default_scratch.data())
    ::munmap(text_scratch.data, text_scratch.size);
  text_scratch.data = static_cast<char *>(mapping);
  text_scratch.size = wanted;
}
} // namespace

extern "C" const char *tracer_snapshot_copy_bytes(const void *source,
                                                  std::size_t size) noexcept {
  const PreserveErrno preserve_errno;
  if (!size || size > text_scratch.size)
    return size ? nullptr : text_scratch.data;
  return read_snapshot_memory(reinterpret_cast<std::uintptr_t>(source),
                              text_scratch.data, size)
             ? text_scratch.data
             : nullptr;
}

extern "C" bool tracer_snapshot_is_scratch(const void *address, std::size_t size) noexcept {
  const auto begin = reinterpret_cast<std::uintptr_t>(text_scratch.data);
  const auto value = reinterpret_cast<std::uintptr_t>(address);
  return value >= begin && size <= text_scratch.size &&
         value - begin <= text_scratch.size - size;
}

extern "C" const char *tracer_snapshot_read_cstring(const char *text,
                                                    std::size_t capacity,
                                                    std::size_t *length) noexcept {
  const PreserveErrno preserve_errno;
  auto address = reinterpret_cast<std::uintptr_t>(text);
  const auto system_page_size = ::getpagesize();
  capacity = std::min(capacity, text_scratch.size);
  if (!address || !length || !capacity || system_page_size <= 0)
    return nullptr;
  const auto page = static_cast<std::uintptr_t>(system_page_size);
  std::size_t used = 0;
  while (used < capacity) {
    // Read up to the next page boundary so a string ending at an unmapped
    // page is read without touching that page.
    const auto chunk = std::min<std::size_t>(
        {static_cast<std::size_t>(page - address % page), capacity - used, 256});
    if (!read_snapshot_memory(address, text_scratch.data + used, chunk))
      return nullptr;
    if (const void *end = std::memchr(text_scratch.data + used, 0, chunk)) {
      *length = static_cast<std::size_t>(static_cast<const char *>(end) - text_scratch.data);
      return text_scratch.data;
    }
    used += chunk;
    address += chunk;
  }
  *length = capacity;
  return text_scratch.data;
}

// Like tracer_snapshot_storage, but leaves `reserve` bytes of the thread's
// storage for the roots that are still to be encoded.
extern "C" std::byte *tracer_snapshot_storage_limited(std::size_t *capacity,
                                                      std::size_t reserve) noexcept {
  if (updating.exchange(true, std::memory_order_acquire)) {
    *capacity = 0;
    return arena.data + arena.size;
  }
  const auto begin = used.load(std::memory_order_relaxed);
  const auto left = arena.size - begin;
  *capacity = left > reserve ? std::min<std::size_t>(slice_bytes(), left - reserve) : 0;
  used.store(begin + *capacity, std::memory_order_relaxed);
  updating.store(false, std::memory_order_release);
  return arena.data + begin;
}

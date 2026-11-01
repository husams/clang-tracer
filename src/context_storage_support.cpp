#include "context_storage_support.hpp"
#include "engine_storage.hpp"

namespace tracer::context_engine {
namespace {
void *allocate(const StorageDomain *domain, std::uint64_t bytes,
               std::uint64_t alignment) noexcept {
  static_assert(sizeof(std::size_t) == sizeof(std::uint64_t));
  return domain->allocate(bytes, alignment).pointer;
}
std::uint32_t release(const StorageDomain *domain, void *pointer,
                      std::uint64_t bytes, std::uint64_t alignment) noexcept {
  return static_cast<std::uint32_t>(domain->release(pointer, bytes, alignment));
}
// Volatile byte operations deliberately prevent loop-idiom lowering from
// reintroducing an application-interposable memcpy/memmove/memset dependency.
void copy(void *destination, const void *source, std::uint64_t bytes) noexcept {
  auto *to = static_cast<volatile unsigned char *>(destination);
  auto *from = static_cast<const volatile unsigned char *>(source);
  for (std::uint64_t i = 0; i < bytes; ++i)
    to[i] = from[i];
}
void move(void *destination, const void *source, std::uint64_t bytes) noexcept {
  const auto to_address = reinterpret_cast<std::uintptr_t>(destination);
  const auto from_address = reinterpret_cast<std::uintptr_t>(source);
  if (to_address <= from_address || to_address - from_address >= bytes) {
    copy(destination, source, bytes);
    return;
  }
  auto *to = static_cast<volatile unsigned char *>(destination);
  auto *from = static_cast<const volatile unsigned char *>(source);
  while (bytes) {
    --bytes;
    to[bytes] = from[bytes];
  }
}
void set(void *destination, std::uint32_t value, std::uint64_t bytes) noexcept {
  auto *to = static_cast<volatile unsigned char *>(destination);
  for (std::uint64_t i = 0; i < bytes; ++i)
    to[i] = static_cast<unsigned char>(value);
}
} // namespace
std::uintptr_t storage_allocate_entrypoint() noexcept {
  return reinterpret_cast<std::uintptr_t>(&allocate);
}
std::uintptr_t storage_release_entrypoint() noexcept {
  return reinterpret_cast<std::uintptr_t>(&release);
}
std::uintptr_t memory_copy_entrypoint() noexcept {
  return reinterpret_cast<std::uintptr_t>(&copy);
}
std::uintptr_t memory_move_entrypoint() noexcept {
  return reinterpret_cast<std::uintptr_t>(&move);
}
std::uintptr_t memory_set_entrypoint() noexcept {
  return reinterpret_cast<std::uintptr_t>(&set);
}
} // namespace tracer::context_engine

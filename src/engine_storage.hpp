#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace tracer::context_engine {

enum class StorageStatus {
  ok,
  exhausted,
  busy,
  foreign_pointer,
  wrong_domain,
  invalid_release
};

// Private standard-library allocation domain. The budget covers payload AND
// metadata. Mapping and atomic construction happen during setup, before guards.
// Allocation never waits, calls application allocation or grows the mapping.
class EngineStorage final {
public:
  static constexpr std::size_t kDefaultBudget = 64 * 1024 * 1024;
  using Domain = std::uint64_t;
  explicit EngineStorage(std::size_t budget = kDefaultBudget);
  ~EngineStorage();
  EngineStorage(const EngineStorage &) = delete;
  EngineStorage &operator=(const EngineStorage &) = delete;
  EngineStorage(EngineStorage &&) = delete;
  EngineStorage &operator=(EngineStorage &&) = delete;

  struct Allocation {
    void *pointer = nullptr;
    StorageStatus status = StorageStatus::exhausted;
  };
  Allocation
  allocate(Domain, std::size_t bytes,
           std::size_t alignment = alignof(std::max_align_t)) noexcept;
  StorageStatus release(Domain, void *, std::size_t expected_bytes = 0,
                        std::size_t expected_alignment = 0) noexcept;
  bool owns_live(Domain, const void *) const noexcept;
  // Caller pins the allocation for this read (the same rule as owns_live).
  std::size_t allocation_size(Domain, const void *) const noexcept;
  std::size_t mapped_bytes() const noexcept { return mapped_bytes_; }
  std::size_t payload_capacity() const noexcept { return units_ * kUnit; }
  std::size_t occupied_bytes() const noexcept {
    return occupied_.load(std::memory_order_relaxed) * kUnit;
  }
  std::uint64_t failures() const noexcept {
    return failures_.load(std::memory_order_relaxed);
  }

private:
  static constexpr std::size_t kUnit = 256;
  struct Unit {
    std::atomic<unsigned> live{0};
    std::atomic<Domain> domain{0};
    std::size_t count = 0, bytes = 0, alignment = 0;
  };
  bool marked(std::size_t) const noexcept;
  void mark(std::size_t, std::size_t, bool) noexcept;
  std::size_t index(const void *) const noexcept;

  void *mapping_ = nullptr;
  std::size_t mapped_bytes_ = 0, units_ = 0, words_ = 0, cursor_ = 0;
  std::atomic<std::uint64_t> *bits_ = nullptr;
  Unit *metadata_ = nullptr;
  std::byte *payload_ = nullptr;
  std::atomic_flag allocating_ = ATOMIC_FLAG_INIT;
  std::atomic<std::size_t> occupied_{0};
  mutable std::atomic<std::uint64_t> failures_{0};
};
// Engine-issued allocation authority, created during setup. Every production
// domain shares one process-wide 64 MiB mapping; generation/module leases pin
// the authority until all owning values and finalizers have returned.
class StorageDomain final {
public:
  static std::shared_ptr<const StorageDomain> create();
  EngineStorage::Allocation allocate(std::size_t, std::size_t) const noexcept;
  StorageStatus release(void *, std::size_t = 0,
                        std::size_t = 0) const noexcept;
  bool owns_live(const void *) const noexcept;
  std::size_t allocation_size(const void *) const noexcept;
  std::size_t outstanding() const noexcept;

private:
  StorageDomain(std::shared_ptr<EngineStorage>, EngineStorage::Domain);
  const std::shared_ptr<EngineStorage> storage_;
  const EngineStorage::Domain domain_;
  mutable std::atomic<std::size_t> outstanding_{0};
};
} // namespace tracer::context_engine

#include "engine_storage.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include <sys/mman.h>
#include <unistd.h>

#include "tracer/compat.hpp"

namespace tracer::context_engine {
namespace {
std::size_t aligned(std::size_t value, std::size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}
} // namespace

EngineStorage::EngineStorage(std::size_t budget) {
  static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
  static_assert(std::atomic<unsigned>::is_always_lock_free);
  const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  if (!::tracer::compat::has_single_bit(page) || budget > kDefaultBudget || budget < page * 2) {
    throw std::invalid_argument("invalid engine storage budget");
  }
  mapped_bytes_ = budget / page * page;
  // Each unit charges one payload quantum, one metadata entry and its bitmap.
  units_ =
      (mapped_bytes_ - page) / (kUnit + sizeof(Unit) + sizeof(std::uint64_t));
  words_ = (units_ + 63) / 64;
  const auto metadata_offset = aligned(words_ * sizeof(*bits_), alignof(Unit));
  const auto payload_offset =
      aligned(metadata_offset + units_ * sizeof(Unit), page);
  if (!units_ || payload_offset > mapped_bytes_ ||
      units_ * kUnit > mapped_bytes_ - payload_offset)
    throw std::invalid_argument("engine storage metadata exceeds budget");
  mapping_ = ::mmap(nullptr, mapped_bytes_, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping_ == MAP_FAILED) {
    mapping_ = nullptr;
    throw std::bad_alloc();
  }
  auto *base = static_cast<std::byte *>(mapping_);
  bits_ = reinterpret_cast<std::atomic<std::uint64_t> *>(base);
  metadata_ = reinterpret_cast<Unit *>(base + metadata_offset);
  payload_ = base + payload_offset;
  for (std::size_t i = 0; i < words_; ++i)
    ::tracer::compat::construct_at(bits_ + i, 0);
  for (std::size_t i = 0; i < units_; ++i)
    ::tracer::compat::construct_at(metadata_ + i);
  // Touch all payload pages now; the first guarded allocation never initializes
  // a lazy mapping or metadata array through the application allocator.
  for (std::size_t i = 0; i < units_ * kUnit; i += page)
    payload_[i] = std::byte{};
}

EngineStorage::~EngineStorage() {
  // Module/record leases must retain this object until all owning values die.
  // A violated internal lifetime contract leaks bounded storage rather than
  // invalidating an outstanding private buffer.
  if (mapping_ && !occupied_.load(std::memory_order_acquire))
    ::munmap(mapping_, mapped_bytes_);
}
bool EngineStorage::marked(std::size_t unit) const noexcept {
  return bits_[unit / 64].load(std::memory_order_acquire) &
         (std::uint64_t{1} << (unit % 64));
}
void EngineStorage::mark(std::size_t start, std::size_t count,
                         bool value) noexcept {
  while (count) {
    const auto bit = start % 64;
    const auto take = std::min(count, 64 - bit);
    const auto mask = (take == 64 ? ~std::uint64_t{}
                                  : ((std::uint64_t{1} << take) - 1) << bit);
    if (value)
      bits_[start / 64].fetch_or(mask, std::memory_order_acq_rel);
    else
      bits_[start / 64].fetch_and(~mask, std::memory_order_acq_rel);
    start += take;
    count -= take;
  }
}

EngineStorage::Allocation
EngineStorage::allocate(Domain domain, std::size_t bytes,
                        std::size_t alignment) noexcept {
  const auto failed = [&](StorageStatus reason) {
    failures_.fetch_add(1, std::memory_order_relaxed);
    return Allocation{nullptr, reason};
  };
  if (!domain || !::tracer::compat::has_single_bit(alignment) || bytes > payload_capacity()) {
    return failed(StorageStatus::exhausted);
  }
  const auto count = (std::max(bytes, std::size_t{1}) + kUnit - 1) / kUnit;
  if (allocating_.test_and_set(std::memory_order_acquire))
    return failed(StorageStatus::busy);
  struct Unlock {
    std::atomic_flag &flag;
    ~Unlock() { flag.clear(std::memory_order_release); }
  } unlock{allocating_};
  const auto search = [&](std::size_t begin, std::size_t end) -> std::size_t {
    std::size_t run = 0, start = begin;
    for (std::size_t i = begin; i < end; ++i) {
      if (marked(i)) {
        run = 0;
        continue;
      }
      if (!run) {
        const auto address =
            reinterpret_cast<std::uintptr_t>(payload_ + i * kUnit);
        if (address % alignment)
          continue;
        start = i;
      }
      if (++run == count)
        return start;
    }
    return units_;
  };
  auto start = search(cursor_, units_);
  if (start == units_)
    start = search(0, units_);
  if (start == units_)
    return failed(StorageStatus::exhausted);
  mark(start, count, true);
  auto &entry = metadata_[start];
  entry.domain.store(domain, std::memory_order_relaxed);
  entry.count = count;
  entry.bytes = bytes;
  entry.alignment = alignment;
  entry.live.store(1, std::memory_order_release);
  occupied_.fetch_add(count, std::memory_order_relaxed);
  cursor_ = (start + count) % units_;
  return {payload_ + start * kUnit, StorageStatus::ok};
}

std::size_t EngineStorage::index(const void *pointer) const noexcept {
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  const auto base = reinterpret_cast<std::uintptr_t>(payload_);
  if (address < base || address - base >= payload_capacity() ||
      (address - base) % kUnit)
    return units_;
  return (address - base) / kUnit;
}
StorageStatus EngineStorage::release(Domain domain, void *pointer,
                                     std::size_t expected_bytes,
                                     std::size_t expected_alignment) noexcept {
  if (!pointer)
    return StorageStatus::ok;
  const auto failed = [&](StorageStatus status) {
    failures_.fetch_add(1, std::memory_order_relaxed);
    return status;
  };
  const auto slot = index(pointer);
  if (slot == units_)
    return failed(StorageStatus::foreign_pointer);
  auto &entry = metadata_[slot];
  unsigned live = 1;
  if (!entry.live.compare_exchange_strong(live, 2, std::memory_order_acq_rel))
    return failed(StorageStatus::invalid_release);
  const auto fail_live = [&](StorageStatus status) {
    entry.live.store(1, std::memory_order_release);
    return failed(status);
  };
  if (entry.domain.load(std::memory_order_relaxed) != domain)
    return fail_live(StorageStatus::wrong_domain);
  if ((expected_bytes && entry.bytes != expected_bytes) ||
      (expected_alignment && entry.alignment != expected_alignment))
    return fail_live(StorageStatus::invalid_release);
  const auto count = entry.count;
  // Publish empty metadata BEFORE returning bitmap units. An allocator can
  // reuse a cleared prefix while the rest of a large old allocation is freed.
  entry.live.store(0, std::memory_order_release);
  occupied_.fetch_sub(count, std::memory_order_relaxed);
  mark(slot, count, false);
  return StorageStatus::ok;
}
bool EngineStorage::owns_live(Domain domain,
                              const void *pointer) const noexcept {
  const auto slot = index(pointer);
  if (slot == units_)
    return false;
  // This query is for call-scoped borrows whose caller already pins lifetime;
  // it does not manufacture an allocation lease from an arbitrary raw pointer.
  const auto &entry = metadata_[slot];
  return entry.live.load(std::memory_order_acquire) == 1 &&
         entry.domain.load(std::memory_order_relaxed) == domain;
}
StorageDomain::StorageDomain(std::shared_ptr<EngineStorage> storage,
                             EngineStorage::Domain domain)
    : storage_(std::move(storage)), domain_(domain) {}

std::size_t EngineStorage::allocation_size(Domain domain,
                                           const void *pointer) const noexcept {
  const auto slot = index(pointer);
  if (slot == units_)
    return static_cast<std::size_t>(-1);
  const auto &entry = metadata_[slot];
  if (entry.live.load(std::memory_order_acquire) != 1 ||
      entry.domain.load(std::memory_order_relaxed) != domain)
    return static_cast<std::size_t>(-1);
  return entry.bytes;
}

std::shared_ptr<const StorageDomain> StorageDomain::create() {
  static const auto storage = std::make_shared<EngineStorage>();
  static std::atomic<EngineStorage::Domain> next_domain{1};
  // Never reuse an exhausted identity, even after a generation has retired.
  auto candidate = next_domain.load(std::memory_order_relaxed);
  do {
    if (candidate == std::numeric_limits<EngineStorage::Domain>::max())
      throw std::overflow_error(
          "engine allocation domain identities exhausted");
  } while (!next_domain.compare_exchange_weak(candidate, candidate + 1,
                                              std::memory_order_relaxed));
  return std::shared_ptr<const StorageDomain>(
      new StorageDomain(storage, candidate));
}

EngineStorage::Allocation
StorageDomain::allocate(std::size_t bytes,
                        std::size_t alignment) const noexcept {
  const auto result = storage_->allocate(domain_, bytes, alignment);
  if (result.pointer)
    outstanding_.fetch_add(1, std::memory_order_relaxed);
  return result;
}

StorageStatus StorageDomain::release(void *pointer, std::size_t bytes,
                                     std::size_t alignment) const noexcept {
  const auto result = storage_->release(domain_, pointer, bytes, alignment);
  if (pointer && result == StorageStatus::ok)
    outstanding_.fetch_sub(1, std::memory_order_relaxed);
  return result;
}

bool StorageDomain::owns_live(const void *pointer) const noexcept {
  return storage_->owns_live(domain_, pointer);
}
std::size_t StorageDomain::allocation_size(const void *pointer) const noexcept {
  return storage_->allocation_size(domain_, pointer);
}
std::size_t StorageDomain::outstanding() const noexcept {
  return outstanding_.load(std::memory_order_relaxed);
}
} // namespace tracer::context_engine

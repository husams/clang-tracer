#pragma once

#include "bounded_proto.hpp"
#include "context_codec.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace tracer::context_engine {
struct SchemaRecord;
struct CaptureArena;
struct CaptureSlotState;
} // namespace tracer::context_engine

namespace tracer {
struct TraceContext;
}

namespace tracer::context_engine {

// The event arena, its fixed slot tables, 64MiB queued attachments, and 16MiB
// native workspace fit within the 128MiB process capture-storage budget.
inline constexpr std::size_t kCaptureEventBytes = 256ull * 1024;
inline constexpr std::size_t kCaptureGlobalBudgetBytes = 128ull * 1024 * 1024;
inline constexpr std::size_t kCaptureQueueBudgetBytes = 64ull * 1024 * 1024;
inline constexpr std::size_t kCaptureInvocationWorkspaceBytes =
    7ull * 1024 * 1024;
inline constexpr std::size_t kCaptureRuntimeWorkspaceBytes = 4ull * 1024 * 1024;
inline constexpr std::size_t kCaptureGuardedWorkspaceBytes = 5ull * 1024 * 1024;
inline constexpr std::size_t kCaptureArenaBytes = 48ull * 1024 * 1024;
static_assert(kCaptureArenaBytes + kCaptureQueueBudgetBytes +
                  kCaptureInvocationWorkspaceBytes +
                  kCaptureRuntimeWorkspaceBytes +
                  kCaptureGuardedWorkspaceBytes ==
              kCaptureGlobalBudgetBytes);
inline constexpr std::size_t kCaptureSlotMetadataReserve = 512ull * 1024;
inline constexpr std::size_t kCaptureSlots =
    (kCaptureArenaBytes - kCaptureSlotMetadataReserve) / kCaptureEventBytes;
inline constexpr std::size_t kCapturePoolBytes =
    kCaptureSlots * kCaptureEventBytes;
inline constexpr std::size_t kCaptureHooks = 256;
inline constexpr std::size_t kCaptureEventsPerHook = 16;
inline constexpr std::size_t kCaptureBytesPerHook =
    kCaptureEventsPerHook * kCaptureEventBytes;

struct CaptureSchemaView {
  std::string_view identity;
  std::string_view root_type;
  std::string_view descriptor_set;
  std::string_view source_lineage_id;
  std::string_view cpp_state_type;
  std::string_view cpp_scope_type;
  std::string_view revision_id;
  std::span<const std::string> container_roles;
  std::uint64_t epoch_schema_id = 0;
  std::shared_ptr<const SchemaRecord> retained_record;
};

// Fixed per-event schema references. Instances live in ContextCapturePool
// slots; the delivery worker reads them while it holds the associated token.
class CaptureSchemaSetLease final {
public:
  static constexpr std::size_t capacity = 64;
  std::size_t size() const noexcept { return size_; }
  std::uint64_t epoch_schema_id(std::size_t) const noexcept;
  const SchemaRecord *record(std::size_t) const noexcept;
  std::span<const std::byte> event_bytes() const noexcept;

private:
  friend struct CaptureSlotState;
  friend struct CaptureArena;
  friend void
  release_capture_schema_set(const std::shared_ptr<const void> &) noexcept;
  std::array<std::shared_ptr<const SchemaRecord>, capacity> records_{};
  std::array<std::uint64_t, capacity> epoch_schema_ids_{};
  std::size_t size_ = 0;
  std::size_t event_bytes_size_ = 0;
  CaptureArena *arena_ = nullptr;
  CaptureSlotState *owner_ = nullptr;
  std::size_t slot_ = 0;
  std::uint32_t hook_ = 0;
  bool published_ = false;
};

// The pointer remains valid while token is held. Release after the queued
// event reaches a terminal delivery state, not when capture returns.
const CaptureSchemaSetLease *
capture_schema_set(const std::shared_ptr<const void> &token) noexcept;
void release_capture_schema_set(
    const std::shared_ptr<const void> &token) noexcept;

struct CaptureStateView {
  using Encode = bool (*)(const void *, BoundedProtoWriter &, const void *,
                          EncodeBudget &) noexcept;
  std::uint64_t scope_id = 0;
  std::string_view schema_identity;
  std::uint64_t epoch_schema_id = 0;
  const void *payload = nullptr;
  const void *encode_context = nullptr;
  Encode encode = nullptr;
};

template <class T>
bool encode_capture_state(const void *, BoundedProtoWriter &writer,
                          const void *payload, EncodeBudget &budget) noexcept {
  return payload &&
         encode_state_payload(writer, *static_cast<const T *>(payload), budget);
}

template <class T>
bool encode_capture_payload_root(const void *, BoundedProtoWriter &writer,
                                 const void *payload,
                                 EncodeBudget &budget) noexcept {
  return payload &&
         encode_value(writer, 1, *static_cast<const T *>(payload), budget);
}

struct CaptureEventMetadata {
  std::string_view event_id;
  std::string_view source_id;
  std::uint64_t source_sequence = 0;
  std::string_view task_id;
  std::string_view parent_task_id;
  std::string_view parent_source_id;
  std::uint64_t parent_submission_sequence = 0;
  std::string_view name;
  std::uint64_t recording_position = 0;
  std::string_view opaque_metadata;
  std::uint32_t effective_max_items = 100;
  std::string_view invocation_id;
  std::string_view probe_id;
  const tracer::TraceContext *trace_context = nullptr;
  bool print_arguments = false;
  bool print_result = false;
};

struct CaptureResult {
  CaptureStatus status = CaptureStatus::ok;
  std::size_t bytes = 0;
  explicit operator bool() const noexcept {
    return status == CaptureStatus::ok;
  }
};

// Owns fixed-capacity capture storage. Construct during hook registration,
// before any guarded interval; reservation and capture perform no allocation.
class ContextCapturePool final {
public:
  class Lease final {
  public:
    Lease() noexcept = default;
    ~Lease() noexcept;
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;
    Lease(Lease &&) noexcept;
    Lease &operator=(Lease &&) noexcept;
    explicit operator bool() const noexcept { return bool(slot_state_); }

    // Serializes one EventRecord. State payloads are read only during this
    // call; schema metadata remains pinned separately by the slot lease.
    CaptureResult capture(const CaptureEventMetadata &,
                          std::span<const CaptureSchemaView>,
                          std::span<const CaptureStateView>) noexcept;
    std::span<const std::byte> bytes() const noexcept;
    // Legacy byte-only publication releases after the callback returns.
    using Publish = bool (*)(void *, std::span<const std::byte>) noexcept;
    bool publish(Publish, void *) noexcept;
    using LeasedPublish =
        bool (*)(void *, std::span<const std::byte>,
                 const std::shared_ptr<const void> &) noexcept;
    bool publish_leased(LeasedPublish, void *) noexcept;
    std::shared_ptr<const void> schema_lease_token() const noexcept;

  private:
    friend class ContextCapturePool;
    Lease(std::shared_ptr<CaptureSlotState>, std::size_t,
          std::uint32_t) noexcept;
    void reset() noexcept;
    std::shared_ptr<CaptureSlotState> slot_state_;
    std::shared_ptr<const void> schema_token_;
    std::size_t slot_ = 0;
    std::uint32_t hook_ = 0;
    std::size_t bytes_ = 0;
    bool captured_ = false;
    bool published_ = false;
  };

  ContextCapturePool();
  ~ContextCapturePool();
  ContextCapturePool(const ContextCapturePool &) = delete;
  ContextCapturePool &operator=(const ContextCapturePool &) = delete;

  // Hook identifiers are assigned at setup. try_reserve is nonblocking and
  // enforces 16 outstanding events/4MiB per hook within its arena allocation.
  Lease try_reserve(std::uint32_t hook_id) noexcept;
  std::size_t outstanding(std::uint32_t hook_id) const noexcept;

private:
  std::shared_ptr<CaptureArena> arena_;
  std::array<std::shared_ptr<CaptureSlotState>, kCaptureSlots> slots_{};
};

// CaptureSlotState consists of the schema lease, arena shared_ptr, active
// flag and slot index. The extra 64 bytes per slot conservatively includes
// the make_shared control block; 512 bytes accounts for the arena header and
// its shared control block. Keep this bookkeeping inside the 48MiB arena cap.
inline constexpr std::size_t kCaptureSlotControlBlockEstimate = 64;
inline constexpr std::size_t kCaptureArenaHeaderEstimate = 512;
static_assert(
    kCaptureSlots * (sizeof(CaptureSchemaSetLease) +
                     sizeof(std::shared_ptr<CaptureArena>) +
                     sizeof(std::atomic<bool>) + sizeof(std::size_t) +
                     kCaptureSlotControlBlockEstimate) +
            sizeof(ContextCapturePool) + kCaptureArenaHeaderEstimate <=
        kCaptureSlotMetadataReserve,
    "capture slot lease/control-block metadata exceeds its reserved budget");

} // namespace tracer::context_engine

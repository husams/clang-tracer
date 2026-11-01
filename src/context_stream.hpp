#pragma once

#include "context_transport.grpc.pb.h"
#include "engine_storage.hpp"

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <array>
#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tracer::context_delivery {
class ContextCollectorStore;
class ContextJournal;
}
namespace tracer::context_engine { struct SchemaRecord; }
namespace tracer::context_delivery {

class ContextFrameProcessor {
public:
  virtual ~ContextFrameProcessor() = default;
  // False with a nonempty error is terminal rejection; false with an empty
  // error is downstream backpressure and retries this frame.
  virtual bool process(const clangtracer::context::v1::ContextFrame &,
                       std::string &error) = 0;
};
}
namespace tracer::context_delivery {

struct StreamLimits {
  std::size_t queued_packets = 4096;
  std::size_t queued_bytes = 64ull * 1024 * 1024;
  std::uint32_t max_packet_bytes = 2 * 1024 * 1024;
  std::uint32_t max_event_bytes = 256 * 1024;
  std::uint32_t max_epoch_schemas = 256;
  std::uint64_t max_epoch_schema_bytes = 16ull * 1024 * 1024;
};

struct ExportCounters {
  std::uint64_t capture_failed = 0, queue_dropped = 0;
  std::uint64_t transport_uncertain = 0, journal_dropped = 0;
};

// A lease pins schema/payload/deleter ownership until the gRPC write returns.
struct LeasedPacket {
  clangtracer::context::v1::ProducerPacket packet;
  std::shared_ptr<const void> lease;
  std::size_t charge = 0;
};

struct HookPacket {
  class StorageCharge final {
  public:
    StorageCharge() noexcept = default;
    ~StorageCharge() noexcept;
    StorageCharge(const StorageCharge &) = delete;
    StorageCharge &operator=(const StorageCharge &) = delete;
    StorageCharge(StorageCharge &&) noexcept;
    StorageCharge &operator=(StorageCharge &&) noexcept;
    void reset() noexcept;
    void take(std::size_t bytes) noexcept;
  private:
    std::size_t bytes_ = 0;
  } storage_charge;
  std::string serialized_frame;
  std::shared_ptr<const void> lease;
  // Worker-only pointers into immutable records pinned by lease.
  std::vector<const tracer::context_engine::SchemaRecord *> schema_records;
  bool capture_event = false;
  std::uint64_t reservation = 0;
};
struct HookPacketView {
  std::span<const std::byte> serialized_frame;
  std::shared_ptr<const void> lease;
};

// Startup-allocated SPSC-consumer/MPSC-producer byte ring. Hook admission uses
// try_lock and memcpy only; it never constructs protobufs or allocates bytes.
class ContextHookRing final {
public:
  explicit ContextHookRing(std::size_t byte_capacity = 64ull * 1024 * 1024,
                           std::size_t packet_limit = 4096,
                           std::size_t packet_bytes = 2 * 1024 * 1024,
                           std::size_t capture_event_bytes = 256 * 1024);
  ~ContextHookRing();
  ContextHookRing(const ContextHookRing &) = delete;
  ContextHookRing &operator=(const ContextHookRing &) = delete;
  bool try_publish(std::span<const std::byte> serialized_frame,
                   std::shared_ptr<const void> lease = {}) noexcept;
  bool try_publish_batch(std::span<const HookPacketView> packets) noexcept;
  bool try_publish_capture(
      std::span<const std::byte> event_record,
      const std::shared_ptr<const void> &schema_lease) noexcept;
  bool try_take(HookPacket &packet) noexcept; // Worker only.
  bool reserve_worker_staging(std::size_t bytes,
                              HookPacket::StorageCharge &charge) noexcept;
  void complete(std::uint64_t reservation) noexcept; // After SDK terminal callback.
  std::uint64_t dropped() const noexcept { return dropped_.load(); }
  std::size_t queued_bytes() const noexcept;
private:
  struct Entry {
    void *storage = nullptr;
    std::size_t staging_bytes = 0;
    std::size_t length = 0, allocation_size = 0;
    std::size_t queue_charge = 0;
    std::uint64_t reservation = 0;
    std::shared_ptr<const void> lease;
    const tracer::context_engine::SchemaRecord **schema_records = nullptr;
    std::size_t schema_count = 0;
    bool capture_event = false;
  };
  const std::size_t capacity_, packet_limit_, packet_bytes_, capture_event_bytes_;
  tracer::context_engine::EngineStorage *storage_ = nullptr;
  tracer::context_engine::EngineStorage::Domain domain_ = 0;
  Entry *entries_ = nullptr; // View into EngineStorage; lifetime owned by storage_.
  std::size_t entries_bytes_ = 0;
  std::size_t metadata_credit_ = 0;
  mutable std::mutex mutex_;
  std::size_t item_head_ = 0, item_tail_ = 0, item_count_ = 0;
  std::size_t queued_bytes_ = 0;
  std::uint64_t next_reservation_ = 1;
  bool in_flight_ = false;
  std::uint64_t in_flight_reservation_ = 0;
  std::atomic<std::uint64_t> dropped_{0};
};

class ContextExportClient final {
public:
  using DurableConfirmation = std::function<bool(
      const clangtracer::context::v1::DurableControl &)>;
  ContextExportClient(std::shared_ptr<grpc::Channel> channel,
                      clangtracer::context::v1::ExportOpen open,
                      StreamLimits limits = {},
                      DurableConfirmation durable_confirmation = {},
                      std::shared_ptr<ContextJournal> journal = {});
  ~ContextExportClient();
  ContextExportClient(const ContextExportClient &) = delete;
  ContextExportClient &operator=(const ContextExportClient &) = delete;
  bool start();
  void stop() noexcept;
  ContextHookRing &hook_ring() noexcept { return hook_ring_; }
  // Worker-only compatibility path; callers from hooks must use hook_ring().
  bool publish_from_worker(const clangtracer::context::v1::ContextFrame &frame,
                           std::shared_ptr<const void> lease = {});
  bool try_publish_capture(
      std::span<const std::byte> event_record,
      const std::shared_ptr<const void> &schema_lease) noexcept;
  ExportCounters counters() const noexcept;
  bool ready() const noexcept;
  bool paused() const noexcept;

private:
  void run() noexcept;
  const StreamLimits limits_;
  ContextHookRing hook_ring_;
  clangtracer::context::v1::ExportOpen open_;
  std::unique_ptr<clangtracer::context::v1::ContextTransport::Stub> stub_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  ExportCounters counters_;
  bool stopping_ = false, worker_done_ = false, ready_ = false, paused_ = false;
  bool setup_complete_ = false, control_fault_ = false;
  DurableConfirmation durable_confirmation_;
  std::shared_ptr<ContextJournal> journal_;
  grpc::ClientContext *active_context_ = nullptr;
  std::chrono::steady_clock::time_point shutdown_deadline_{};
  std::thread worker_;
};

class ContextExportService final : public clangtracer::context::v1::ContextTransport::Service {
public:
  using DurableStoreFactory = std::function<std::shared_ptr<ContextCollectorStore>(
      std::string_view recording_id, std::string_view sink_id)>;
  using FrameHandler = std::function<bool(const clangtracer::context::v1::ContextFrame &)>;
  using CounterHandler = std::function<void(std::string_view, const ExportCounters &)>;
  using DurableHandler = std::function<bool(const clangtracer::context::v1::ExportOpen &,
      clangtracer::context::v1::DurableControl &)>;
  using ProcessorFactory = std::function<std::unique_ptr<ContextFrameProcessor>(
      const clangtracer::context::v1::ExportOpen &, const StreamLimits &)>;
  ContextExportService(FrameHandler frame_handler, DurableHandler durable_handler = {},
                       StreamLimits limits = {}, CounterHandler counter_handler = {},
                       ProcessorFactory processor_factory = {});
  ContextExportService(std::shared_ptr<ContextCollectorStore> durable_store,
                       FrameHandler frame_handler = {}, StreamLimits limits = {},
                       CounterHandler counter_handler = {},
                       ProcessorFactory processor_factory = {},
                       DurableStoreFactory durable_store_factory = {});
  grpc::Status Export(grpc::ServerContext *context,
      grpc::ServerReaderWriter<clangtracer::context::v1::CollectorControl,
                               clangtracer::context::v1::ProducerPacket> *stream) override;
private:
  FrameHandler frame_handler_;
  DurableHandler durable_handler_;
  std::shared_ptr<ContextCollectorStore> durable_store_;
  DurableStoreFactory durable_store_factory_;
  CounterHandler counter_handler_;
  ProcessorFactory processor_factory_;
  std::mutex counter_mutex_;
  std::unordered_map<std::string, ExportCounters> counters_by_session_;
  const StreamLimits limits_;
  std::atomic<std::size_t> active_ingress_streams_{0};
};

} // namespace tracer::context_delivery

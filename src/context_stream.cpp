#include "context_stream.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <stdexcept>
#include <tuple>

#include <grpcpp/channel.h>
#include <unistd.h>

#include "context_capture.hpp"
#include "context_diagnostic_format.hpp"
#include "context_execution.hpp"
#include "context_journal.hpp"
#include "context_schema.hpp"
#include "tracer/compat.hpp"

namespace tracer::context_delivery {
namespace {
using namespace std::chrono_literals;
constexpr std::size_t kGlobalPacketLimit = 4096;
constexpr std::size_t kGlobalRingLimit = 64;
constexpr std::size_t kCaptureSchemasPerEvent = 64;
constexpr std::size_t kRingStorageBudgetBytes = 8ull * 1024 * 1024;
constexpr std::size_t kStagingCreditBudgetBytes =
    tracer::context_engine::kCaptureQueueBudgetBytes - kRingStorageBudgetBytes;
constexpr std::size_t kStagingMultiplier = 8;
constexpr std::size_t kStagingPerPacket = 4096;
std::atomic<std::size_t> g_live_rings{0};
std::atomic<std::size_t> g_live_packets{0};
std::atomic<std::size_t> g_staging_credits_used{0};
std::atomic<std::uint64_t> g_next_storage_domain{1};
bool reserve_staging_credit(std::size_t bytes) noexcept {
  auto used = g_staging_credits_used.load(std::memory_order_relaxed);
  do {
    if (bytes > kStagingCreditBudgetBytes - used) return false;
  } while (!g_staging_credits_used.compare_exchange_weak(
      used, used + bytes, std::memory_order_acq_rel,
      std::memory_order_relaxed));
  return true;
}
void release_staging_credit(std::size_t bytes) noexcept {
  if (bytes) g_staging_credits_used.fetch_sub(bytes, std::memory_order_release);
}
tracer::context_engine::EngineStorage &shared_hook_storage() {
  static tracer::context_engine::EngineStorage storage(
      kRingStorageBudgetBytes);
  return storage;
}
void emit_context_diagnostic(void *,
                             const tracer::context_execution::Diagnostic &d) noexcept {
  char line[320];
  const int size = tracer::context_execution::format_diagnostic(
      d, line, sizeof(line));
  if (size > 0) (void)::write(STDERR_FILENO, line,
                              static_cast<std::size_t>(std::min<int>(size, sizeof(line))));
}
void report_export_failure(const char *label, std::uint64_t code = 0) noexcept {
  char name[tracer::context_execution::kEventNameCapacity]{};
  const int size = code
      ? std::snprintf(name, sizeof(name), "%s_%llu", label,
                      static_cast<unsigned long long>(code))
      : std::snprintf(name, sizeof(name), "%s", label);
  const auto length = size <= 0 ? 0u : static_cast<std::size_t>(
      std::min<int>(size, static_cast<int>(sizeof(name) - 1)));
  (void)tracer::context_execution::runtime_diagnostics().record(
      tracer::context_execution::Severity::warning,
      tracer::context_execution::Reason::transport_failure,
      tracer::context_execution::Phase::transport,
      tracer::context_execution::Action::continued, 0, 0,
      std::string_view(name, length));
}
}

HookPacket::StorageCharge::~StorageCharge() noexcept {
  reset();
}
HookPacket::StorageCharge::StorageCharge(
    StorageCharge &&other) noexcept
    : bytes_(std::exchange(other.bytes_, 0)) {}
HookPacket::StorageCharge &
HookPacket::StorageCharge::operator=(
    StorageCharge &&other) noexcept {
  if (this != &other) {
    reset();
    bytes_ = std::exchange(other.bytes_, 0);
  }
  return *this;
}
void HookPacket::StorageCharge::reset() noexcept {
  release_staging_credit(std::exchange(bytes_, 0));
}
void HookPacket::StorageCharge::take(
    std::size_t bytes) noexcept {
  reset();
  bytes_ = bytes;
}

ContextHookRing::ContextHookRing(std::size_t byte_capacity, std::size_t packet_limit,
                                 std::size_t packet_bytes,
                                 std::size_t capture_event_bytes)
    : capacity_(byte_capacity), packet_limit_(packet_limit), packet_bytes_(packet_bytes),
      capture_event_bytes_(std::min(capture_event_bytes, packet_bytes)),
      storage_(&shared_hook_storage()), domain_(g_next_storage_domain.fetch_add(1)) {
  if (capacity_ == 0 || packet_limit_ == 0 || packet_limit_ > kGlobalPacketLimit ||
      packet_bytes_ == 0 || packet_bytes_ > capacity_ ||
      capture_event_bytes_ == 0 || domain_ == 0)
    throw std::invalid_argument("invalid hook ring limits");
  auto active = g_live_rings.load(std::memory_order_relaxed);
  do {
    if (active >= kGlobalRingLimit) throw std::invalid_argument("global hook ring metadata capacity exhausted");
  } while (!g_live_rings.compare_exchange_weak(active, active + 1,
      std::memory_order_acq_rel, std::memory_order_relaxed));
  if (packet_limit_ > std::numeric_limits<std::size_t>::max() / sizeof(Entry)) {
    g_live_rings.fetch_sub(1, std::memory_order_relaxed);
    throw std::invalid_argument("hook ring metadata size overflow");
  }
  entries_bytes_ = packet_limit_ * sizeof(Entry);
  if (!reserve_staging_credit(entries_bytes_)) {
    g_live_rings.fetch_sub(1, std::memory_order_relaxed);
    throw std::bad_alloc();
  }
  entries_ = new (std::nothrow) Entry[packet_limit_];
  if (!entries_) {
    release_staging_credit(entries_bytes_);
    g_live_rings.fetch_sub(1, std::memory_order_relaxed);
    throw std::bad_alloc();
  }
  metadata_credit_ = entries_bytes_;
}
ContextHookRing::~ContextHookRing() {
  std::lock_guard lock(mutex_);
  for (std::size_t i = 0; i < packet_limit_; ++i) {
    auto &entry = entries_[i];
    if (entry.reservation == 0) continue;
    if (entry.storage)
      storage_->release(domain_, entry.storage, entry.allocation_size);
    if (entry.capture_event)
      tracer::context_engine::release_capture_schema_set(entry.lease);
    release_staging_credit(entry.staging_bytes);
    entry.lease.reset();
    g_live_packets.fetch_sub(1, std::memory_order_release);
  }
  delete[] entries_;
  release_staging_credit(metadata_credit_);
  g_live_rings.fetch_sub(1, std::memory_order_relaxed);
}

bool ContextHookRing::try_publish(::tracer::compat::span<const std::byte> bytes,
                                  std::shared_ptr<const void> lease) noexcept {
  const HookPacketView packet{bytes, std::move(lease)};
  return try_publish_batch(::tracer::compat::span<const HookPacketView>(&packet, 1));
}

bool ContextHookRing::try_publish_batch(
    ::tracer::compat::span<const HookPacketView> packets) noexcept {
  std::unique_lock lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock() || packets.empty() || packets.size() > packet_limit_) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (packets.size() > packet_limit_ - item_count_) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  std::size_t required = 0;
  for (const auto &packet : packets) {
    const auto bytes = packet.serialized_frame;
    if (bytes.empty() || bytes.size() > packet_bytes_ || bytes.size() > capacity_) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    if (bytes.size() > (std::numeric_limits<std::size_t>::max() -
                        kStagingPerPacket) / kStagingMultiplier) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    const auto charge = bytes.size();
    if (charge > std::numeric_limits<std::size_t>::max() - required) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    required += charge;
  }
  if (required > capacity_ - queued_bytes_) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  auto global_packets = g_live_packets.load(std::memory_order_relaxed);
  do {
    if (packets.size() > kGlobalPacketLimit - global_packets) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
  } while (!g_live_packets.compare_exchange_weak(global_packets, global_packets + packets.size(),
      std::memory_order_acq_rel, std::memory_order_relaxed));
  std::size_t staging_total = 0;
  for (const auto &packet : packets) {
    const auto staging = packet.serialized_frame.size() * kStagingMultiplier +
                         kStagingPerPacket;
    if (staging > std::numeric_limits<std::size_t>::max() - staging_total) {
      g_live_packets.fetch_sub(packets.size(), std::memory_order_release);
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    staging_total += staging;
  }
  if (!reserve_staging_credit(staging_total)) {
    g_live_packets.fetch_sub(packets.size(), std::memory_order_release);
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  std::array<void *, kGlobalPacketLimit> allocations{};
  std::size_t allocated = 0;
  for (const auto &packet : packets) {
    const auto allocation = storage_->allocate(
        domain_, packet.serialized_frame.size());
    if (allocation.status != tracer::context_engine::StorageStatus::ok) {
      for (std::size_t i = 0; i < allocated; ++i)
        storage_->release(domain_, allocations[i]);
      release_staging_credit(staging_total);
      g_live_packets.fetch_sub(packets.size(), std::memory_order_release);
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    allocations[allocated++] = allocation.pointer;
  }
  allocated = 0;
  for (const auto &packet : packets) {
    const auto bytes = packet.serialized_frame;
    Entry &entry = entries_[item_tail_];
    entry.storage = allocations[allocated++];
    entry.length = bytes.size();
    entry.staging_bytes = bytes.size() * kStagingMultiplier + kStagingPerPacket;
    entry.allocation_size = bytes.size();
    entry.queue_charge = bytes.size();
    entry.reservation = next_reservation_++;
    entry.schema_records = nullptr;
    entry.schema_count = 0;
    entry.capture_event = false;
    std::memcpy(entry.storage, bytes.data(), bytes.size());
    entry.lease = packet.lease;
    queued_bytes_ += entry.queue_charge;
    item_tail_ = (item_tail_ + 1) % packet_limit_;
    ++item_count_;
  }
  return true;
}

bool ContextHookRing::try_publish_capture(
    ::tracer::compat::span<const std::byte> event_record,
    const std::shared_ptr<const void>& schema_lease) noexcept {
  const auto *schema_set =
      tracer::context_engine::capture_schema_set(schema_lease);
  if (!schema_set || schema_set->size() > kCaptureSchemasPerEvent) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  std::unique_lock lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock() || event_record.empty() ||
      event_record.size() > capture_event_bytes_ || item_count_ == packet_limit_) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  const auto schema_count = schema_set->size();
  std::size_t schema_bytes = 0;
  for (std::size_t i = 0; i < schema_count; ++i) {
    const auto *record = schema_set->record(i);
    if (!record || record->descriptor_set.size() >
                       std::numeric_limits<std::size_t>::max() - schema_bytes) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    schema_bytes += record->descriptor_set.size();
    for (const auto &role : record->container_roles) {
      if (role.size() > std::numeric_limits<std::size_t>::max() - schema_bytes) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
      }
      schema_bytes += role.size();
    }
  }
  if (schema_bytes > std::numeric_limits<std::size_t>::max() - event_record.size() ||
      schema_bytes + event_record.size() >
          (std::numeric_limits<std::size_t>::max() - kStagingPerPacket * (schema_count + 1)) /
              kStagingMultiplier) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  const auto staging_bytes = (schema_bytes + event_record.size()) *
                                 kStagingMultiplier +
                             kStagingPerPacket * (schema_count + 1);
  const auto remainder = event_record.size() % alignof(const tracer::context_engine::SchemaRecord *);
  const auto padding = remainder == 0 ? 0 :
      alignof(const tracer::context_engine::SchemaRecord *) - remainder;
  if (schema_count > (std::numeric_limits<std::size_t>::max() -
                      event_record.size() - padding) /
                         sizeof(const tracer::context_engine::SchemaRecord *)) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  const auto allocation_bytes = event_record.size() + padding +
      schema_count * sizeof(const tracer::context_engine::SchemaRecord *);
  const auto queue_charge = event_record.size() + padding +
      schema_count * sizeof(const tracer::context_engine::SchemaRecord *);
  if (staging_bytes > std::numeric_limits<std::size_t>::max() - queue_charge) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (queue_charge > capacity_ - queued_bytes_) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (!reserve_staging_credit(staging_bytes)) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  auto global_packets = g_live_packets.load(std::memory_order_relaxed);
  do {
    if (global_packets == kGlobalPacketLimit) {
      release_staging_credit(staging_bytes);
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
  } while (!g_live_packets.compare_exchange_weak(
      global_packets, global_packets + 1, std::memory_order_acq_rel,
      std::memory_order_relaxed));
  const auto allocation = storage_->allocate(domain_, allocation_bytes);
  if (allocation.status != tracer::context_engine::StorageStatus::ok) {
    release_staging_credit(staging_bytes);
    g_live_packets.fetch_sub(1, std::memory_order_release);
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  auto *base = static_cast<std::byte *>(allocation.pointer);
  std::memcpy(base, event_record.data(), event_record.size());
  auto *schema_records = reinterpret_cast<const tracer::context_engine::SchemaRecord **>(
      base + event_record.size() + padding);
  for (std::size_t i = 0; i < schema_count; ++i) {
    schema_records[i] = schema_set->record(i);
    if (!schema_records[i]) {
      storage_->release(domain_, allocation.pointer, allocation_bytes);
      release_staging_credit(staging_bytes);
      g_live_packets.fetch_sub(1, std::memory_order_release);
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
  }
  Entry &entry = entries_[item_tail_];
  entry.storage = allocation.pointer;
  entry.length = event_record.size();
  entry.staging_bytes = staging_bytes;
  entry.allocation_size = allocation_bytes;
  entry.queue_charge = queue_charge;
  entry.reservation = next_reservation_++;
  entry.schema_records = schema_records;
  entry.schema_count = schema_count;
  entry.capture_event = true;
  entry.lease = schema_lease;
  queued_bytes_ += queue_charge;
  item_tail_ = (item_tail_ + 1) % packet_limit_;
  ++item_count_;
  return true;
}

bool ContextHookRing::try_take(HookPacket &packet) noexcept {
  std::lock_guard lock(mutex_);
  if (in_flight_ || item_count_ == 0) return false;
  Entry &entry = entries_[item_head_];
  try {
    packet.serialized_frame.assign(reinterpret_cast<const char *>(entry.storage), entry.length);
    if (entry.schema_count)
      packet.schema_records.assign(entry.schema_records,
                                   entry.schema_records + entry.schema_count);
    else
      packet.schema_records.clear();
  } catch (...) { return false; }
  packet.storage_charge.take(entry.staging_bytes);
  entry.staging_bytes = 0;
  packet.lease = entry.lease;
  packet.capture_event = entry.capture_event;
  packet.reservation = entry.reservation;
  in_flight_ = true;
  in_flight_reservation_ = entry.reservation;
  return true;
}

bool ContextHookRing::reserve_worker_staging(
    std::size_t bytes, HookPacket::StorageCharge &charge) noexcept {
  if (bytes == 0) return true;
  if (!reserve_staging_credit(bytes)) return false;
  charge.take(bytes);
  return true;
}

void ContextHookRing::complete(std::uint64_t reservation) noexcept {
  std::lock_guard lock(mutex_);
  if (!in_flight_ || reservation != in_flight_reservation_ || item_count_ == 0) return;
  Entry &entry = entries_[item_head_];
  if (entry.storage)
    storage_->release(domain_, entry.storage, entry.allocation_size);
  queued_bytes_ -= entry.queue_charge;
  release_staging_credit(entry.staging_bytes);
  if (entry.capture_event)
    tracer::context_engine::release_capture_schema_set(entry.lease);
  entry.lease.reset();
  entry.storage = nullptr;
  entry.staging_bytes = 0;
  entry.length = 0;
  entry.allocation_size = 0;
  entry.queue_charge = 0;
  entry.reservation = 0;
  entry.schema_records = nullptr;
  entry.schema_count = 0;
  entry.capture_event = false;
  item_head_ = (item_head_ + 1) % packet_limit_;
  --item_count_;
  g_live_packets.fetch_sub(1, std::memory_order_release);
  in_flight_ = false;
  in_flight_reservation_ = 0;
}

std::size_t ContextHookRing::queued_bytes() const noexcept {
  std::lock_guard lock(mutex_);
  return queued_bytes_;
}

ContextExportClient::ContextExportClient(std::shared_ptr<grpc::Channel> channel,
    clangtracer::context::v1::ExportOpen open, StreamLimits limits,
    DurableConfirmation durable_confirmation, std::shared_ptr<ContextJournal> journal)
    : limits_(limits),
      hook_ring_(limits.queued_bytes, limits.queued_packets,
          std::min<std::size_t>(limits.queued_bytes,
              limits.max_packet_bytes > 16 ? limits.max_packet_bytes - 16 : 0),
          std::min<std::size_t>(limits.max_event_bytes,
              open.max_event_bytes() ? open.max_event_bytes()
                                     : limits.max_event_bytes)),
      open_(std::move(open)),
      stub_(clangtracer::context::v1::ContextTransport::NewStub(std::move(channel))),
      durable_confirmation_(std::move(durable_confirmation)), journal_(std::move(journal)) {
  if (!open_.max_packet_bytes()) open_.set_max_packet_bytes(limits_.max_packet_bytes);
  if (!open_.max_event_bytes()) open_.set_max_event_bytes(limits_.max_event_bytes);
  if (!open_.max_epoch_schemas()) open_.set_max_epoch_schemas(limits_.max_epoch_schemas);
  if (!open_.max_epoch_schema_bytes()) open_.set_max_epoch_schema_bytes(limits_.max_epoch_schema_bytes);
}
ContextExportClient::~ContextExportClient() { stop(); }
bool ContextExportClient::start() {
  std::lock_guard lock(mutex_);
  if (worker_.joinable() || stopping_) return false;
  if (journal_ && open_.profile() == clangtracer::context::v1::DURABLE) {
    const auto journal_state = journal_->state();
    if (open_.operation() == clangtracer::context::v1::CREATE) {
      if (!journal_->begin_create(open_.recording_id(), open_.sink_id())) return false;
    } else if (!journal_->healthy() || !journal_state.creation_confirmed ||
               journal_state.recording_id != open_.recording_id() ||
               journal_state.sink_id != open_.sink_id()) {
      return false;
    } else {
      open_.set_saved_ack_position(journal_state.acknowledged);
    }
  }
  worker_ = std::thread(&ContextExportClient::run, this);
  return true;
}
void ContextExportClient::stop() noexcept {
  {
    std::unique_lock lock(mutex_);
    stopping_ = true;
    shutdown_deadline_ = std::chrono::steady_clock::now() + 1s;
    cv_.notify_all();
    if (worker_.joinable() && !cv_.wait_until(lock, shutdown_deadline_, [&] { return worker_done_; }))
      if (active_context_) active_context_->TryCancel();
  }
  if (worker_.joinable()) worker_.join();
}
bool ContextExportClient::publish_from_worker(const clangtracer::context::v1::ContextFrame &frame,
                                            std::shared_ptr<const void> lease) {
  std::string bytes;
  if (!frame.SerializeToString(&bytes) ||
      bytes.size() > limits_.max_packet_bytes - 16) return false;
  return hook_ring_.try_publish(
      ::tracer::compat::as_bytes(::tracer::compat::span(bytes.data(), bytes.size())),
      std::move(lease));
}

bool ContextExportClient::try_publish_capture(
    ::tracer::compat::span<const std::byte> event_record,
    const std::shared_ptr<const void>& schema_lease) noexcept {
  return hook_ring_.try_publish_capture(event_record, schema_lease);
}
ExportCounters ContextExportClient::counters() const noexcept {
  std::lock_guard lock(mutex_);
  auto result = counters_;
  result.queue_dropped += hook_ring_.dropped();
  return result;
}
bool ContextExportClient::ready() const noexcept { std::lock_guard lock(mutex_); return ready_; }
bool ContextExportClient::paused() const noexcept { std::lock_guard lock(mutex_); return paused_; }

void ContextExportClient::run() noexcept {
  tracer::context_execution::Scope transport_mode(
      tracer::context_execution::Mode::TransportNoTrace);
  auto delay = 100ms;
  auto next_diagnostic = std::chrono::steady_clock::now();
  std::uint64_t epoch = open_.epoch_id();
  HookPacket pending_hook;
  bool has_pending_hook = false;
  for (;;) {
    {
      std::lock_guard lock(mutex_);
      if (stopping_) break;
      ready_ = false; paused_ = false; setup_complete_ = false; control_fault_ = false;
    }
    open_.set_epoch_id(epoch++);
    std::array<HookPacket::StorageCharge, 256> epoch_schema_charges;
    std::size_t epoch_schema_charge_count = 0;
    std::unordered_map<std::string, std::uint64_t> epoch_schema_ids;
    std::unordered_map<std::string, std::string> epoch_schema_definitions;
    std::uint64_t next_schema_id = 1;
    std::uint64_t epoch_schema_bytes = 0;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + 24h);
    { std::lock_guard lock(mutex_); active_context_ = &context; }
    auto stream = stub_->Export(&context);
    bool failed = false;
    bool rotate_epoch = false;
    std::atomic<bool> expected_close{false};
    std::uint64_t submitted_events = 0;
    auto last_progress = std::chrono::steady_clock::now();
    ExportCounters reported_counters{};
    if (!stream || !stream->Write([&] { clangtracer::context::v1::ProducerPacket p; *p.mutable_open() = open_; return p; }())) {
      report_export_failure(stream ? "export_open_write_failed"
                                   : "export_stream_unavailable");
      failed = true;
    } else {
      std::mutex progress_mutex;
      std::condition_variable progress_cv;
      bool progress_stop = false, progress_pending = false, progress_paused = false;
      auto progress_time = std::chrono::steady_clock::now();
      auto write_packet = [&](const clangtracer::context::v1::ProducerPacket &packet) {
        {
          std::lock_guard lock(progress_mutex);
          progress_pending = true;
          progress_time = std::chrono::steady_clock::now();
        }
        const bool ok = stream->Write(packet);
        {
          std::lock_guard lock(progress_mutex);
          progress_pending = false;
          progress_time = std::chrono::steady_clock::now();
        }
        progress_cv.notify_all();
        return ok;
      };
      std::thread watchdog([&] {
        tracer::context_execution::Scope helper_mode(
            tracer::context_execution::Mode::TransportNoTrace);
        std::unique_lock lock(progress_mutex);
        while (!progress_stop) {
          if (!progress_pending || progress_paused) {
            progress_cv.wait_for(lock, 100ms, [&] {
              return progress_stop || (progress_pending && !progress_paused);
            });
            continue;
          }
          const auto deadline = progress_time + 30s;
          if (progress_cv.wait_until(lock, deadline, [&] {
                return progress_stop || !progress_pending || progress_paused || progress_time + 30s != deadline;
              })) continue;
          lock.unlock();
          context.TryCancel();
          lock.lock();
          break;
        }
      });
      std::thread reader([&] {
        tracer::context_execution::Scope helper_mode(
            tracer::context_execution::Mode::TransportNoTrace);
        clangtracer::context::v1::CollectorControl control;
        while (stream->Read(&control)) {
          if (control.ByteSizeLong() > 4 * 1024) {
            report_export_failure("control_packet_oversize");
            std::lock_guard lock(mutex_); control_fault_ = true; cv_.notify_all(); break;
          }
          if (control.epoch_id() != open_.epoch_id()) continue;
          if (control.has_durable()) {
            bool accepted = control.durable().result() == clangtracer::context::v1::DurableControl::ACCEPTED;
            auto operation = open_.operation();
            if (accepted && journal_) {
              if (operation == clangtracer::context::v1::CREATE)
                accepted = journal_->confirm_create(control.durable().recording_id(), control.durable().durable_prefix());
              else
                accepted = journal_->save_ack(control.durable().recording_id(), control.durable().durable_prefix());
            }
            if (accepted && durable_confirmation_) accepted = durable_confirmation_(control.durable());
            else if (accepted && !journal_) accepted = false;
            if (!accepted) {
              if (control.durable().result() !=
                  clangtracer::context::v1::DurableControl::ACCEPTED) {
                report_export_failure(
                    "durable_control_refused",
                    static_cast<std::uint64_t>(control.durable().result()));
              } else {
                report_export_failure(
                    "durable_local_confirmation_failed",
                    static_cast<std::uint64_t>(operation));
              }
            }
            std::lock_guard lock(mutex_);
            if (accepted) {
              ready_ = true;
              open_.set_operation(clangtracer::context::v1::RESUME);
              open_.set_saved_ack_position(std::max(open_.saved_ack_position(), control.durable().durable_prefix()));
            } else {
              ready_ = false;
            }
            setup_complete_ = accepted;
            control_fault_ = !accepted;
            cv_.notify_all();
            continue;
          }
          std::lock_guard lock(mutex_);
          if (control.has_ready()) {
            ready_ = control.ready().protocol_version() == 1 &&
                control.ready().accepted_profile() == open_.profile() &&
                control.ready().max_packet_bytes() <= limits_.max_packet_bytes &&
                control.ready().max_event_bytes() <= limits_.max_event_bytes &&
                control.ready().max_epoch_schemas() <= limits_.max_epoch_schemas &&
                control.ready().max_epoch_schema_bytes() <= limits_.max_epoch_schema_bytes;
            if (!ready_) {
              std::uint64_t reason = 0;
              if (control.ready().protocol_version() != 1) reason = 1;
              else if (control.ready().accepted_profile() != open_.profile()) reason = 2;
              else if (control.ready().max_packet_bytes() > limits_.max_packet_bytes) reason = 3;
              else if (control.ready().max_event_bytes() > limits_.max_event_bytes) reason = 4;
              else if (control.ready().max_epoch_schemas() > limits_.max_epoch_schemas) reason = 5;
              else if (control.ready().max_epoch_schema_bytes() > limits_.max_epoch_schema_bytes) reason = 6;
              report_export_failure("ready_negotiation_rejected", reason);
            }
            setup_complete_ = ready_ && open_.profile() != clangtracer::context::v1::DURABLE;
            control_fault_ = !ready_;
          }
          if (control.has_flow()) {
            paused_ = control.flow().paused();
            {
              std::lock_guard progress_lock(progress_mutex);
              progress_paused = paused_;
              progress_time = std::chrono::steady_clock::now();
            }
            progress_cv.notify_all();
          }
          cv_.notify_all();
        }
        bool cancel = false;
        {
          std::lock_guard lock(mutex_);
          cancel = !stopping_ && !expected_close.load(std::memory_order_acquire);
          if (cancel) control_fault_ = true;
          cv_.notify_all();
        }
        if (cancel) context.TryCancel();
      });
      {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, 30s, [&] { return stopping_ || control_fault_ || setup_complete_; });
        if (!setup_complete_) {
          const bool rejected = control_fault_;
          failed = true;
          report_export_failure(rejected ? "export_setup_rejected"
                                         : "export_ready_timeout");
        }
        if (!failed) last_progress = std::chrono::steady_clock::now();
      }
      std::uint64_t replay_cursor = 0;
      if (journal_) {
        std::lock_guard lock(mutex_);
        replay_cursor = open_.saved_ack_position();
      }
      while (!failed) {
        if (std::chrono::steady_clock::now() >= next_diagnostic) {
          (void)tracer::context_execution::runtime_diagnostics().drain_coalesced(
              emit_context_diagnostic, nullptr, 32);
          next_diagnostic = std::chrono::steady_clock::now() + 1s;
        }
        ExportCounters current_counters;
        bool can_send_counters = false;
        {
          std::lock_guard lock(mutex_);
          current_counters = counters_;
          current_counters.queue_dropped += hook_ring_.dropped();
          can_send_counters = setup_complete_ && !paused_;
        }
        const bool counters_changed = current_counters.capture_failed != reported_counters.capture_failed ||
            current_counters.queue_dropped != reported_counters.queue_dropped ||
            current_counters.transport_uncertain != reported_counters.transport_uncertain ||
            current_counters.journal_dropped != reported_counters.journal_dropped;
        if (can_send_counters && counters_changed) {
          clangtracer::context::v1::ProducerPacket counter_packet;
          auto *counters = counter_packet.mutable_counters();
          counters->set_capture_failed(current_counters.capture_failed);
          counters->set_queue_dropped(current_counters.queue_dropped);
          counters->set_transport_uncertain(current_counters.transport_uncertain);
          counters->set_journal_dropped(current_counters.journal_dropped);
          if (!write_packet(counter_packet)) { failed = true; break; }
          reported_counters = current_counters;
          last_progress = std::chrono::steady_clock::now();
        }
        HookPacket hook_item;
        bool is_paused = false;
        {
          std::unique_lock lock(mutex_);
          // Poll while idle or paused, but drain queued packets without an
          // additional idle interval between writes.
          if (paused_ || hook_ring_.queued_bytes() == 0)
            cv_.wait_for(lock, 10ms, [&] { return stopping_ || control_fault_; });
          if (stopping_) {
            const auto deadline = shutdown_deadline_;
            lock.unlock();
            const bool ring_empty = hook_ring_.queued_bytes() == 0;
            if (ring_empty || std::chrono::steady_clock::now() >= deadline) {
              if (!ring_empty) { context.TryCancel(); failed = true; }
              break;
            }
            lock.lock();
          }
          if (control_fault_) { failed = true; break; }
          is_paused = paused_;
        }
        if (is_paused) continue;
        bool replayed = false;
        if (journal_ && open_.profile() == clangtracer::context::v1::DURABLE) {
          auto records = journal_->read_after(replay_cursor, 1,
              static_cast<std::size_t>(limits_.queued_bytes));
          if (!records.empty()) {
            hook_item.serialized_frame = std::move(records.front().serialized_frame);
            replay_cursor = records.front().position;
            replayed = true;
          }
        }
        if (!replayed && has_pending_hook) {
          hook_item = std::move(pending_hook);
          pending_hook = {};
          has_pending_hook = false;
        } else if (!replayed && !hook_ring_.try_take(hook_item)) {
          const bool pending = hook_ring_.queued_bytes() != 0;
          if (pending && !is_paused && std::chrono::steady_clock::now() - last_progress > 30s) failed = true;
          continue;
        }
        {
          using namespace clangtracer::context::v1;
          ContextBatch batch;
          ContextFrame single;
          std::vector<SchemaRecord> schemas_to_announce;
          EventRecord event;
          bool has_event = false;
          bool malformed = false;
          const bool is_batch = !hook_item.capture_event &&
              batch.ParseFromString(hook_item.serialized_frame) &&
              (batch.schemas_size() != 0 || batch.has_event());
          if (hook_item.capture_event) {
            if (!event.ParseFromString(hook_item.serialized_frame) ||
                hook_item.schema_records.size() > kCaptureSchemasPerEvent) {
              malformed = true;
            } else {
              has_event = true;
              *batch.mutable_event() = event;
              for (const auto *record : hook_item.schema_records) {
                if (!record || record->revision_id.empty() ||
                    record->root_type.empty() || record->descriptor_set.empty() ||
                    record->family.cpp_state_type.empty() ||
                    record->family.cpp_scope_type.empty()) {
                  malformed = true;
                  break;
                }
                auto *schema = batch.add_schemas();
                schema->set_schema_identity(record->revision_id);
                schema->set_root_type(record->root_type);
                schema->set_serialized_file_descriptor_set(record->descriptor_set);
                schema->set_source_lineage_id(
                    reinterpret_cast<const char *>(
                        record->family.source_lineage_id.data()),
                    record->family.source_lineage_id.size());
                schema->set_cpp_state_type(record->family.cpp_state_type);
                schema->set_cpp_scope_type(record->family.cpp_scope_type);
                schema->set_revision_id(record->revision_id);
                for (const auto &role : record->container_roles)
                  schema->add_container_roles(role);
              }
              if (!malformed)
                schemas_to_announce.assign(batch.schemas().begin(),
                                           batch.schemas().end());
            }
          } else if (is_batch) {
            schemas_to_announce.assign(batch.schemas().begin(), batch.schemas().end());
            if (batch.has_event()) { event = batch.event(); has_event = true; }
          } else if (single.ParseFromString(hook_item.serialized_frame) && single.has_schema()) {
            schemas_to_announce.push_back(single.schema());
          } else if (single.ParseFromString(hook_item.serialized_frame) && single.has_event()) {
            event = single.event(); has_event = true;
            *batch.mutable_event() = event;
          } else {
            hook_ring_.complete(hook_item.reservation);
            std::lock_guard lock(mutex_); ++counters_.queue_dropped;
            continue;
          }

          if (journal_ && open_.profile() == DURABLE && has_event) {
            if (event.recording_position() == 0) {
              std::uint64_t position = 0;
              if (!journal_->append(batch, position)) {
                hook_ring_.complete(hook_item.reservation);
                std::lock_guard lock(mutex_); ++counters_.journal_dropped;
                continue;
              }
              event.set_recording_position(position);
              batch.mutable_event()->set_recording_position(position);
              if (!batch.SerializeToString(&hook_item.serialized_frame)) {
                hook_ring_.complete(hook_item.reservation);
                std::lock_guard lock(mutex_); ++counters_.journal_dropped;
                continue;
              }
              replay_cursor = position;
            } else {
              replay_cursor = event.recording_position();
            }
          }

          std::unordered_map<std::string, SchemaRecord> candidate_schemas;
          std::unordered_map<std::string, std::string> complete_schema_definitions;
          std::size_t required_schema_bytes = 0, complete_schema_bytes = 0;
          for (auto schema : schemas_to_announce) {
            const auto identity = schema.schema_identity();
            if (identity.empty() || schema.root_type().empty() || schema.serialized_file_descriptor_set().empty()) {
              malformed = true; break;
            }
            schema.clear_epoch_schema_id();
            const auto canonical = schema.SerializeAsString();
            const auto [complete, unique] = complete_schema_definitions.emplace(identity, canonical);
            if (!unique && complete->second != canonical) { malformed = true; break; }
            if (const auto existing = epoch_schema_definitions.find(identity); existing != epoch_schema_definitions.end()) {
              if (existing->second != canonical) { malformed = true; break; }
              continue;
            }
            const auto [it, inserted] = candidate_schemas.emplace(identity, schema);
            if (!inserted && it->second.SerializeAsString() != canonical) { malformed = true; break; }
          }
          for (const auto &[identity, schema] : candidate_schemas) {
            (void)identity;
            required_schema_bytes += schema.ByteSizeLong();
          }
          for (const auto &[identity, definition] : complete_schema_definitions) {
            (void)identity;
            complete_schema_bytes += definition.size();
          }
          if (has_event && !malformed) {
            for (const auto &state : event.states()) {
              if (!::tracer::compat::contains(complete_schema_definitions,
                                              state.schema_identity())) {
                malformed = true;
                break;
              }
            }
          }
          if (malformed) {
            hook_ring_.complete(hook_item.reservation);
            std::lock_guard lock(mutex_); ++counters_.queue_dropped;
            continue;
          }

          const bool fits_empty = complete_schema_definitions.size() <= limits_.max_epoch_schemas &&
              complete_schema_bytes <= limits_.max_epoch_schema_bytes;
          const bool fits_current = candidate_schemas.size() <= limits_.max_epoch_schemas - epoch_schema_ids.size() &&
              required_schema_bytes <= limits_.max_epoch_schema_bytes - epoch_schema_bytes;
          if (!fits_current) {
            if (fits_empty && !epoch_schema_ids.empty()) {
              pending_hook = std::move(hook_item);
              has_pending_hook = true;
              rotate_epoch = true;
              break;
            }
            hook_ring_.complete(hook_item.reservation);
            std::lock_guard lock(mutex_); ++counters_.queue_dropped;
            continue;
          }
          if (candidate_schemas.size() >
              epoch_schema_charges.size() - epoch_schema_charge_count) {
            if (!epoch_schema_ids.empty()) {
              pending_hook = std::move(hook_item);
              has_pending_hook = true;
              rotate_epoch = true;
              break;
            }
            hook_ring_.complete(hook_item.reservation);
            std::lock_guard lock(mutex_);
            ++counters_.queue_dropped;
            continue;
          }

          std::array<HookPacket::StorageCharge, 256> candidate_schema_charges;
          std::size_t candidate_schema_charge_count = 0;
          bool staging_admission_failed = false;
          for (const auto &[identity, schema] : candidate_schemas) {
            if (candidate_schema_charge_count == candidate_schema_charges.size() ||
                identity.size() >
                    (std::numeric_limits<std::size_t>::max() - 512) / 2 ||
                schema.ByteSizeLong() > std::numeric_limits<std::size_t>::max() -
                    identity.size() * 2 - 512 ||
                !hook_ring_.reserve_worker_staging(
                    schema.ByteSizeLong() + identity.size() * 2 + 512,
                    candidate_schema_charges[candidate_schema_charge_count])) {
              staging_admission_failed = true;
              break;
            }
            ++candidate_schema_charge_count;
          }
          if (staging_admission_failed) {
            if (!epoch_schema_ids.empty()) {
              pending_hook = std::move(hook_item);
              has_pending_hook = true;
              rotate_epoch = true;
              break;
            }
            hook_ring_.complete(hook_item.reservation);
            std::lock_guard lock(mutex_);
            ++counters_.queue_dropped;
            continue;
          }

          std::vector<ProducerPacket> outbound_packets;
          outbound_packets.reserve(candidate_schemas.size() + (has_event ? 1 : 0));
          std::unordered_map<std::string, std::uint64_t> proposed_ids;
          proposed_ids.reserve(candidate_schemas.size());
          std::uint64_t proposed_next_schema_id = next_schema_id;
          std::uint64_t proposed_schema_bytes = 0;
          bool admission_failed = false;
          for (auto &[identity, schema] : candidate_schemas) {
            const auto schema_id = proposed_next_schema_id++;
            schema.set_epoch_schema_id(schema_id);
            proposed_ids.emplace(identity, schema_id);
            ProducerPacket packet;
            *packet.mutable_data()->mutable_schema() = schema;
            if (packet.ByteSizeLong() > limits_.max_packet_bytes) { admission_failed = true; break; }
            proposed_schema_bytes += schema.ByteSizeLong();
            outbound_packets.push_back(std::move(packet));
          }
          if (!admission_failed && has_event) {
            for (auto &state : *event.mutable_states()) {
              const auto inserted = proposed_ids.find(state.schema_identity());
              const auto current = epoch_schema_ids.find(state.schema_identity());
              if (inserted != proposed_ids.end()) state.set_epoch_schema_id(inserted->second);
              else if (current != epoch_schema_ids.end()) state.set_epoch_schema_id(current->second);
              else { admission_failed = true; break; }
            }
            if (!admission_failed) {
              ProducerPacket packet;
              *packet.mutable_data()->mutable_event() = event;
              if (packet.ByteSizeLong() > limits_.max_packet_bytes ||
                  packet.data().event().ByteSizeLong() > limits_.max_event_bytes)
                admission_failed = true;
              else outbound_packets.push_back(std::move(packet));
            }
          }
          if (admission_failed) {
            hook_ring_.complete(hook_item.reservation);
            std::lock_guard lock(mutex_); ++counters_.queue_dropped;
            continue;
          }

          for (const auto &[identity, schema_id] : proposed_ids) epoch_schema_ids.emplace(identity, schema_id);
          for (const auto &[identity, schema] : candidate_schemas) {
            auto canonical = schema;
            canonical.clear_epoch_schema_id();
            epoch_schema_definitions.emplace(identity, canonical.SerializeAsString());
          }
          for (std::size_t i = 0; i < candidate_schema_charge_count; ++i)
            epoch_schema_charges[epoch_schema_charge_count++] =
                std::move(candidate_schema_charges[i]);
          epoch_schema_bytes += proposed_schema_bytes;
          next_schema_id = proposed_next_schema_id;

          bool write_failed = false;
          bool event_write_attempted = false;
          for (std::size_t i = 0; i < outbound_packets.size(); ++i) {
            auto &packet = outbound_packets[i];
            const bool event_packet = packet.data().has_event();
            if (event_packet) {
              ++submitted_events;
              event_write_attempted = true;
            }
            if (!write_packet(packet)) { write_failed = true; break; }
            last_progress = std::chrono::steady_clock::now();
          }
          if (write_failed) failed = true;
          const bool journaled = journal_ &&
              open_.profile() == clangtracer::context::v1::DURABLE && has_event;
          if (write_failed && !event_write_attempted && !journaled) {
            // Schema writes may have succeeded, but the event was never handed
            // to gRPC. Retain the original batch for fresh-epoch ID remapping.
            pending_hook = std::move(hook_item);
            has_pending_hook = true;
          } else {
            // A best-effort event whose Write was attempted is uncertain and
            // must not be replayed. Durable records are safe to replay by
            // their persisted, idempotent recording position.
            hook_ring_.complete(hook_item.reservation);
          }
          if (failed) break;
        }
      }
      bool stopping_requested = false;
      { std::lock_guard lock(mutex_); stopping_requested = stopping_; }
      expected_close.store(rotate_epoch || stopping_requested, std::memory_order_release);
      stream->WritesDone();
      if (failed) context.TryCancel();
      if (reader.joinable()) reader.join();
      {
        std::lock_guard lock(progress_mutex);
        progress_stop = true;
      }
      progress_cv.notify_all();
      if (watchdog.joinable()) watchdog.join();
      const auto status = stream->Finish();
      if (!status.ok()) {
        report_export_failure("export_rpc_status",
                              static_cast<std::uint64_t>(status.error_code()));
        failed = true;
      }
    }
    {
      std::lock_guard lock(mutex_);
      active_context_ = nullptr;
      if (failed) counters_.transport_uncertain += submitted_events;
      if (stopping_) break;
      ready_ = false; paused_ = false; setup_complete_ = false;
    }
    // Failed RPCs are never automatically replayed by gRPC. Items not removed
    // from the bounded producer queue remain eligible for a fresh epoch.
    if (!rotate_epoch) {
      std::mt19937 generator(std::random_device{}());
      std::uniform_real_distribution<double> jitter(0.8, 1.2);
      const auto jittered_until = std::chrono::steady_clock::now() +
          std::chrono::duration_cast<std::chrono::milliseconds>(delay * jitter(generator));
      while (std::chrono::steady_clock::now() < jittered_until) {
        std::unique_lock lock(mutex_);
        if (cv_.wait_until(lock, jittered_until, [&] { return stopping_; })) break;
      }
      if (delay < 5s) delay = std::min(delay * 2, 5000ms);
    }
    (void)failed;
  }
  {
    std::lock_guard lock(mutex_);
    worker_done_ = true;
    active_context_ = nullptr;
  }
  cv_.notify_all();
}

ContextExportService::ContextExportService(FrameHandler frame_handler,
    DurableHandler durable_handler, StreamLimits limits, CounterHandler counter_handler,
    ProcessorFactory processor_factory)
    : frame_handler_(std::move(frame_handler)), durable_handler_(std::move(durable_handler)),
      counter_handler_(std::move(counter_handler)), processor_factory_(std::move(processor_factory)),
      limits_(limits) {}

ContextExportService::ContextExportService(std::shared_ptr<ContextCollectorStore> durable_store,
    FrameHandler frame_handler, StreamLimits limits, CounterHandler counter_handler,
    ProcessorFactory processor_factory, DurableStoreFactory durable_store_factory)
    : frame_handler_(std::move(frame_handler)), durable_store_(std::move(durable_store)),
      durable_store_factory_(std::move(durable_store_factory)),
      counter_handler_(std::move(counter_handler)), processor_factory_(std::move(processor_factory)),
      limits_(limits) {}

grpc::Status ContextExportService::Export(grpc::ServerContext *context,
    grpc::ServerReaderWriter<clangtracer::context::v1::CollectorControl,
                             clangtracer::context::v1::ProducerPacket> *stream) {
  using namespace clangtracer::context::v1;
  auto active = active_ingress_streams_.load(std::memory_order_relaxed);
  do {
    if (active >= 8) return {grpc::StatusCode::RESOURCE_EXHAUSTED, "global raw-ingress slots exhausted"};
  } while (!active_ingress_streams_.compare_exchange_weak(
      active, active + 1, std::memory_order_acq_rel, std::memory_order_relaxed));
  struct IngressRelease {
    std::atomic<std::size_t> &count;
    ~IngressRelease() { count.fetch_sub(1, std::memory_order_release); }
  } release{active_ingress_streams_};
  ProducerPacket packet;
  if (!stream->Read(&packet) || !packet.has_open())
    return {grpc::StatusCode::INVALID_ARGUMENT, "ExportOpen must be first"};
  const ExportOpen open = packet.open();
  if (packet.ByteSizeLong() > 4 * 1024)
    return {grpc::StatusCode::RESOURCE_EXHAUSTED, "ExportOpen control limit exceeded"};
  if (open.protocol_version() != 1 || open.epoch_id() == 0 || open.producer_session_id().empty())
    return {grpc::StatusCode::INVALID_ARGUMENT, "invalid stream identity or protocol version"};
  if (open.profile() != BEST_EFFORT && open.profile() != DURABLE)
    return {grpc::StatusCode::INVALID_ARGUMENT, "delivery profile must be explicit"};
  if (open.profile() == DURABLE && !durable_handler_ && !durable_store_ &&
      !durable_store_factory_)
    return {grpc::StatusCode::FAILED_PRECONDITION, "durable profile unavailable"};
  auto durable_store = durable_store_;
  if (open.profile() == DURABLE && durable_store_factory_) {
    try {
      durable_store = durable_store_factory_(open.recording_id(), open.sink_id());
    } catch (...) {
      return {grpc::StatusCode::RESOURCE_EXHAUSTED,
              "durable recording store admission failed"};
    }
    if (!durable_store)
      return {grpc::StatusCode::RESOURCE_EXHAUSTED,
              "durable recording store capacity exhausted"};
  }
  CollectorControl ready; ready.set_epoch_id(open.epoch_id());
  auto *limits = ready.mutable_ready(); limits->set_protocol_version(1);
  const auto negotiated_packet_bytes = std::min(limits_.max_packet_bytes, open.max_packet_bytes());
  const auto negotiated_event_bytes = std::min(limits_.max_event_bytes, open.max_event_bytes());
  const auto negotiated_schema_count = std::min(limits_.max_epoch_schemas, open.max_epoch_schemas());
  const auto negotiated_schema_bytes = std::min(limits_.max_epoch_schema_bytes, open.max_epoch_schema_bytes());
  limits->set_max_packet_bytes(negotiated_packet_bytes);
  limits->set_max_event_bytes(negotiated_event_bytes);
  limits->set_max_epoch_schemas(negotiated_schema_count);
  limits->set_max_epoch_schema_bytes(negotiated_schema_bytes);
  limits->set_accepted_profile(open.profile() == DURABLE ? DURABLE : BEST_EFFORT);
  if (!stream->Write(ready)) return {grpc::StatusCode::UNAVAILABLE, "cannot send readiness"};
  if (open.profile() == DURABLE) {
    DurableControl result;
    bool accepted = false;
    if (durable_store) {
      const bool create = open.operation() == CREATE;
      if (open.operation() != CREATE && open.operation() != RESUME)
        return {grpc::StatusCode::INVALID_ARGUMENT, "durable operation must be CREATE or RESUME"};
      const auto outcome = durable_store->open(create, open.recording_id(), open.sink_id(), open.saved_ack_position());
      result.set_result(outcome.accepted ? DurableControl::ACCEPTED : DurableControl::REFUSED);
      result.set_operation(open.operation()); result.set_recording_id(open.recording_id());
      result.set_durable_prefix(outcome.durable_prefix); result.set_detail(outcome.detail);
      accepted = outcome.accepted;
    } else accepted = durable_handler_(open, result);
    if (!accepted) {
      CollectorControl reply; reply.set_epoch_id(open.epoch_id()); *reply.mutable_durable() = result;
      stream->Write(reply);
      return {grpc::StatusCode::FAILED_PRECONDITION, "durable setup failed"};
    }
    CollectorControl reply; reply.set_epoch_id(open.epoch_id()); *reply.mutable_durable() = result;
    if (!stream->Write(reply)) return {grpc::StatusCode::UNAVAILABLE, "cannot send durable setup result"};
    if (result.result() != DurableControl::ACCEPTED) return {grpc::StatusCode::FAILED_PRECONDITION, "durable setup refused"};
  }
  std::unique_ptr<ContextFrameProcessor> processor;
  if (processor_factory_) {
    auto negotiated = limits_;
    negotiated.max_packet_bytes = negotiated_packet_bytes;
    negotiated.max_event_bytes = negotiated_event_bytes;
    negotiated.max_epoch_schemas = negotiated_schema_count;
    negotiated.max_epoch_schema_bytes = negotiated_schema_bytes;
    try { processor = processor_factory_(open, negotiated); }
    catch (...) { return {grpc::StatusCode::RESOURCE_EXHAUSTED, "cannot create bounded epoch processor"}; }
    if (!processor) return {grpc::StatusCode::RESOURCE_EXHAUSTED, "epoch processor unavailable"};
  }
  std::unordered_map<std::string, std::string> schemas;
  std::unordered_map<std::string, std::uint64_t> schema_ids;
  std::unordered_map<std::uint64_t, std::string> identities_by_id;
  std::size_t schema_bytes = 0;
  while (!context->IsCancelled() && stream->Read(&packet)) {
    if (packet.has_open()) return {grpc::StatusCode::INVALID_ARGUMENT, "duplicate ExportOpen"};
    if (packet.has_counters()) {
      if (packet.ByteSizeLong() > 4 * 1024)
        return {grpc::StatusCode::RESOURCE_EXHAUSTED, "counter control limit exceeded"};
      ExportCounters cumulative;
      {
        std::lock_guard lock(counter_mutex_);
        if (!::tracer::compat::contains(counters_by_session_, open.producer_session_id()) &&
            counters_by_session_.size() >= 4096) {
          return {grpc::StatusCode::RESOURCE_EXHAUSTED, "producer counter registry full"};
        }
        auto &last = counters_by_session_[open.producer_session_id()];
        last.capture_failed = std::max(last.capture_failed, packet.counters().capture_failed());
        last.queue_dropped = std::max(last.queue_dropped, packet.counters().queue_dropped());
        last.transport_uncertain = std::max(last.transport_uncertain, packet.counters().transport_uncertain());
        last.journal_dropped = std::max(last.journal_dropped, packet.counters().journal_dropped());
        cumulative = last;
      }
      if (counter_handler_) counter_handler_(open.producer_session_id(), cumulative);
      continue;
    }
    if (!packet.has_data()) continue;
    const auto &frame = packet.data();
    const auto size = packet.ByteSizeLong();
    if (size > negotiated_packet_bytes) return {grpc::StatusCode::RESOURCE_EXHAUSTED, "packet limit exceeded"};
    if (frame.has_schema()) {
      const auto &schema = frame.schema();
      if (schema.schema_identity().empty() || schema.root_type().empty() ||
          schema.serialized_file_descriptor_set().empty() || schema.epoch_schema_id() == 0)
        return {grpc::StatusCode::INVALID_ARGUMENT, "incomplete schema record"};
      const auto found = schemas.find(schema.schema_identity());
      if (found != schemas.end()) {
        if (found->second != schema.SerializeAsString() ||
            schema_ids[schema.schema_identity()] != schema.epoch_schema_id())
          return {grpc::StatusCode::DATA_LOSS, "schema identity redefined"};
      } else {
        if (schemas.size() == negotiated_schema_count ||
            schema.ByteSizeLong() > negotiated_schema_bytes - schema_bytes ||
            ::tracer::compat::contains(identities_by_id, schema.epoch_schema_id())) {
          return {grpc::StatusCode::RESOURCE_EXHAUSTED, "schema epoch limit exceeded"};
        }
        schema_bytes += schema.ByteSizeLong(); schemas.emplace(schema.schema_identity(), schema.SerializeAsString());
        schema_ids.emplace(schema.schema_identity(), schema.epoch_schema_id());
        identities_by_id.emplace(schema.epoch_schema_id(), schema.schema_identity());
      }
    } else if (frame.has_event()) {
      if (frame.event().ByteSizeLong() > negotiated_event_bytes) return {grpc::StatusCode::RESOURCE_EXHAUSTED, "event limit exceeded"};
      if (frame.event().effective_max_items() > 4096)
        return {grpc::StatusCode::INVALID_ARGUMENT, "effective max_items exceeds 4096"};
      for (const auto &state : frame.event().states()) {
        const auto found = schema_ids.find(state.schema_identity());
        if (found == schema_ids.end() || state.epoch_schema_id() != found->second)
          return {grpc::StatusCode::FAILED_PRECONDITION, "event references an unannounced or mismatched schema ID"};
      }
    } else return {grpc::StatusCode::INVALID_ARGUMENT, "empty context frame"};
    if (processor || frame_handler_) {
      auto process_frame = [&]() {
        try {
          if (processor) {
            std::string error;
            if (!processor->process(frame, error)) {
              if (!error.empty()) return std::pair<bool, std::string>{false, std::move(error)};
              return std::pair<bool, std::string>{false, {}};
            }
          }
          if (frame_handler_ && !frame_handler_(frame))
            return std::pair<bool, std::string>{false, {}};
          return std::pair<bool, std::string>{true, {}};
        } catch (const std::exception &error) {
          return std::pair<bool, std::string>{false, error.what()};
        } catch (...) {
          return std::pair<bool, std::string>{false, "downstream processor threw"};
        }
      };
      auto [accepted, process_error] = process_frame();
      if (!accepted && !process_error.empty())
        return {grpc::StatusCode::DATA_LOSS, "epoch processor rejected frame: " + process_error};
      const bool was_paused = !accepted;
      if (was_paused) {
        CollectorControl flow; flow.set_epoch_id(open.epoch_id()); flow.mutable_flow()->set_paused(true);
        if (!stream->Write(flow)) return {grpc::StatusCode::UNAVAILABLE, "cannot pause downstream"};
      }
      while (!accepted && !context->IsCancelled()) {
        std::this_thread::sleep_for(10ms);
        std::tie(accepted, process_error) = process_frame();
        if (!accepted && !process_error.empty())
          return {grpc::StatusCode::DATA_LOSS, "epoch processor rejected frame: " + process_error};
      }
      if (!accepted) return {grpc::StatusCode::CANCELLED, "stream cancelled while downstream was paused"};
      if (was_paused) {
        CollectorControl flow; flow.set_epoch_id(open.epoch_id()); flow.mutable_flow()->set_paused(false);
        if (!stream->Write(flow)) return {grpc::StatusCode::UNAVAILABLE, "cannot resume downstream"};
      }
    }
    if (open.profile() == DURABLE && durable_store) {
      const std::uint64_t position = frame.has_event() ? frame.event().recording_position() : 0;
      const auto result = durable_store->persist(frame, position);
      if (!result.accepted) return {grpc::StatusCode::RESOURCE_EXHAUSTED, result.detail};
    }
    if (open.profile() == DURABLE && frame.has_event()) {
      CollectorControl ack; ack.set_epoch_id(open.epoch_id());
      auto *durable = ack.mutable_durable(); durable->set_result(DurableControl::ACCEPTED);
      durable->set_operation(open.operation()); durable->set_recording_id(open.recording_id());
      durable->set_durable_prefix(durable_store ? durable_store->durable_prefix() : frame.event().recording_position());
      if (!stream->Write(ack)) return {grpc::StatusCode::UNAVAILABLE, "cannot send durable prefix"};
    }
  }
  return grpc::Status::OK;
}
} // namespace tracer::context_delivery

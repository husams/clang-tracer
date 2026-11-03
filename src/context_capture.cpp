#include "context_capture.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <utility>
#include <vector>

#include "context_schema.hpp"
#include "tracer/compat.hpp"
#include "tracer/protocol.hpp"

namespace tracer::context_engine {
struct CaptureArena {
  explicit CaptureArena(std::size_t bytes) : storage(bytes) {}
  std::vector<std::byte> storage;
  std::array<std::atomic<std::uint8_t>, kCaptureHooks> hook_outstanding{};

  void release(CaptureSlotState &, std::uint32_t) noexcept;
};

struct CaptureSlotState {
  CaptureSlotState(std::shared_ptr<CaptureArena> storage_arena,
                   std::size_t slot_index)
      : arena(std::move(storage_arena)), slot(slot_index) {
    schema_set.arena_ = arena.get();
    schema_set.owner_ = this;
    schema_set.slot_ = slot;
  }

  void prepare(std::uint32_t hook) noexcept {
    schema_set.size_ = 0;
    schema_set.event_bytes_size_ = 0;
    schema_set.hook_ = hook;
    schema_set.published_ = false;
    for (auto &record : schema_set.records_)
      record.reset();
    schema_set.epoch_schema_ids_.fill(0);
  }

  bool retain(std::shared_ptr<const SchemaRecord> record,
              std::uint64_t epoch_id) noexcept {
    if (schema_set.size_ == CaptureSchemaSetLease::capacity)
      return false;
    schema_set.records_[schema_set.size_] = std::move(record);
    schema_set.epoch_schema_ids_[schema_set.size_] = epoch_id;
    ++schema_set.size_;
    return true;
  }

  void mark_published(bool value) noexcept { schema_set.published_ = value; }
  void set_event_bytes_size(std::size_t bytes) noexcept {
    schema_set.event_bytes_size_ = bytes;
  }

  std::shared_ptr<CaptureArena> arena;
  std::atomic<bool> active{false};
  std::size_t slot = 0;
  CaptureSchemaSetLease schema_set;
};

namespace {
constexpr std::size_t kMaxTruncations = 32;

struct TruncationEntry {
  std::array<CapturePathStep, 32> path{};
  std::size_t path_size = 0;
  std::uint64_t total = 0, captured = 0;
};
struct TruncationLog {
  std::array<TruncationEntry, kMaxTruncations> entries{};
  std::size_t size = 0;
};
static_assert(sizeof(TruncationLog) + sizeof(EncodeBudget) +
                  sizeof(BoundedProtoWriter) <=
              64 * 1024);

bool record_truncation(void* opaque, ::tracer::compat::span<const CapturePathStep> path,
                       std::uint64_t total, std::uint64_t captured) noexcept {
  auto &log = *static_cast<TruncationLog *>(opaque);
  if (log.size == log.entries.size() ||
      path.size() > log.entries[0].path.size())
    return false;
  auto &entry = log.entries[log.size++];
  entry.path_size = path.size();
  std::copy(path.begin(), path.end(), entry.path.begin());
  entry.total = total;
  entry.captured = captured;
  return true;
}

bool write_path_step(BoundedProtoWriter &writer,
                     const CapturePathStep &step) noexcept {
  if (!writer.begin_message(1))
    return false;
  bool ok = false;
  switch (step.selector) {
  case CapturePathSelector::field_number:
    ok = writer.u64(1, step.number);
    break;
  case CapturePathSelector::list_index:
    ok = writer.u64(2, step.number);
    break;
  case CapturePathSelector::string_key:
    ok = writer.string(3, step.string_value);
    break;
  case CapturePathSelector::signed_key:
    ok = writer.s64(4, step.signed_value);
    break;
  case CapturePathSelector::unsigned_key:
    ok = writer.u64(5, step.number);
    break;
  case CapturePathSelector::bool_key:
    ok = writer.boolean(6, step.bool_value);
    break;
  }
  return writer.end_message() && ok;
}

bool write_truncation(BoundedProtoWriter &writer,
                      const TruncationEntry &entry) noexcept {
  if (!writer.begin_message(4))
    return false;
  for (std::size_t i = 0; i < entry.path_size; ++i)
    if (!write_path_step(writer, entry.path[i]))
      return false;
  return writer.u64(2, entry.total) && writer.u64(3, entry.captured) &&
         writer.end_message();
}

bool write_typed_trace_value(BoundedProtoWriter &writer, std::uint32_t field,
                             const tracer::TraceValue &value) noexcept {
  if (!writer.begin_message(field) ||
      !writer.u64(1, static_cast<std::uint32_t>(value.type)))
    return false;
  bool ok = true;
  switch (value.type) {
  case tracer::ValueType::Int32:
  case tracer::ValueType::Bool:
  case tracer::ValueType::Int64:
    ok = writer.s64(2, value.signed_value);
    break;
  case tracer::ValueType::UInt64:
  case tracer::ValueType::Pointer:
  case tracer::ValueType::Object:
    ok = writer.u64(3, value.unsigned_value);
    break;
  case tracer::ValueType::Float32:
  case tracer::ValueType::Float64:
    ok = writer.f64(4, value.real_value);
    break;
  case tracer::ValueType::Unspecified:
  case tracer::ValueType::Void:
    break;
  }
  return ok && writer.end_message();
}

std::size_t bounded_text_length(const char *text,
                                std::size_t capacity) noexcept {
  const volatile char *scan = text;
  std::size_t length = 0;
  while (length < capacity && scan[length] != '\0')
    ++length;
  return length;
}

bool write_trace_context(BoundedProtoWriter &writer,
                         const tracer::TraceContext &context,
                         bool print_arguments, bool print_result) noexcept {
  const auto argument_count =
      std::min<std::uint32_t>(context.argument_count, tracer::kMaxArguments);
  for (std::uint32_t i = 0; print_arguments && i < argument_count; ++i) {
    if (!writer.begin_message(13) || !writer.u64(1, i))
      return false;
    const char *name = context.argument_names[i];
    if (name) {
      const auto length =
          std::min<std::size_t>(bounded_text_length(name, 64), 63);
      if (!writer.string(2, std::string_view(name, length)))
        return false;
    }
    if (!write_typed_trace_value(writer, 3, context.arguments[i]) ||
        !writer.end_message())
      return false;
  }
  const auto named_count = std::min<std::uint32_t>(context.named_count, 16);
  for (std::uint32_t i = 0; print_arguments && i < named_count; ++i) {
    const auto &named = context.named_values[i];
    if (!writer.begin_message(14) || !writer.u64(1, named.kind) ||
        !writer.u64(2, named.argument_index))
      return false;
    const auto name_length = std::min<std::size_t>(
        bounded_text_length(named.name, sizeof(named.name)), 63);
    if (!writer.string(3, std::string_view(named.name, name_length)) ||
        !write_typed_trace_value(writer, 4, named.value) ||
        !writer.end_message())
      return false;
  }
  if (print_result && context.phase == tracer::TracePhase::Post &&
      !write_typed_trace_value(writer, 15, context.result))
    return false;
  if (!writer.u64(16, static_cast<std::uint32_t>(context.phase)))
    return false;

  const auto &stack = context.stack;
  if (stack.requested_depth == 0 && stack.count == 0)
    return true;
  if (stack.requested_depth > tracer::kMaxStackDepth ||
      stack.count > tracer::kMaxStackDepth)
    return false;
  const auto bounded_text = [](const char *text, std::size_t capacity) {
    if (!text)
      return std::string_view{};
    const volatile char *scan = text;
    std::size_t length = 0;
    while (length < capacity && scan[length] != '\0')
      ++length;
    return std::string_view(text, length);
  };
  if (!writer.begin_message(21) ||
      !writer.u64(1, static_cast<std::uint32_t>(stack.status)) ||
      !writer.u64(2, stack.requested_depth))
    return false;
  for (std::uint32_t i = 0; i < stack.count; ++i) {
    const auto &frame = stack.frames[i];
    if (!writer.begin_message(3) || !writer.u64(1, frame.pc) ||
        !writer.u64(2, frame.stack_pointer) ||
        !writer.u64(3, static_cast<std::uint32_t>(frame.kind)) ||
        !writer.string(4, bounded_text(frame.function, 256)) ||
        !writer.string(5, bounded_text(frame.module, 256)) ||
        !writer.string(6, bounded_text(frame.file, 256)) ||
        !writer.u64(7, frame.line) || !writer.u64(8, frame.module_offset) ||
        !writer.boolean(9, frame.application_frame) || !writer.end_message())
      return false;
  }
  if (!writer.string(4, bounded_text(stack.detail, sizeof(stack.detail))) ||
      !writer.boolean(5, stack.application_only) ||
      !writer.u64(6, stack.hidden_frames) ||
      !writer.u64(7, stack.captured_depth) || !writer.end_message())
    return false;
  return true;
}

bool valid_identifier(std::string_view id, bool optional = false) noexcept {
  return (optional && id.empty()) || id.size() == 16;
}

CaptureStatus writer_status(BoundedProtoWriter &writer) noexcept {
  return writer.status();
}

CaptureStatus budget_status(const EncodeBudget &budget) noexcept {
  if (budget.items > budget.limits.items)
    return CaptureStatus::items_exceeded;
  if (budget.visits > budget.limits.visits)
    return CaptureStatus::visits_exceeded;
  if (budget.depth > budget.limits.depth)
    return CaptureStatus::depth_exceeded;
  return CaptureStatus::work_exceeded;
}
} // namespace

ContextCapturePool::ContextCapturePool()
    : arena_(std::make_shared<CaptureArena>(kCapturePoolBytes)) {
  for (std::size_t i = 0; i < slots_.size(); ++i)
    slots_[i] = std::make_shared<CaptureSlotState>(arena_, i);
}

ContextCapturePool::~ContextCapturePool() = default;

ContextCapturePool::Lease::Lease(std::shared_ptr<CaptureSlotState> slot_state,
                                 std::size_t slot, std::uint32_t hook) noexcept
    : slot_state_(std::move(slot_state)),
      schema_token_(
          slot_state_ ? std::shared_ptr<const void>(
                            slot_state_,
                            static_cast<const void *>(&slot_state_->schema_set))
                      : std::shared_ptr<const void>{}),
      slot_(slot), hook_(hook) {}

ContextCapturePool::Lease::~Lease() noexcept { reset(); }

ContextCapturePool::Lease::Lease(Lease &&other) noexcept
    : slot_state_(std::move(other.slot_state_)),
      schema_token_(std::move(other.schema_token_)), slot_(other.slot_),
      hook_(other.hook_), bytes_(other.bytes_), captured_(other.captured_),
      published_(other.published_) {
  other.published_ = false;
}

ContextCapturePool::Lease &
ContextCapturePool::Lease::operator=(Lease &&other) noexcept {
  if (this != &other) {
    reset();
    slot_state_ = std::move(other.slot_state_);
    schema_token_ = std::move(other.schema_token_);
    slot_ = other.slot_;
    hook_ = other.hook_;
    bytes_ = other.bytes_;
    captured_ = other.captured_;
    published_ = other.published_;
    other.published_ = false;
  }
  return *this;
}

void ContextCapturePool::Lease::reset() noexcept {
  if (slot_state_ && !published_)
    slot_state_->arena->release(*slot_state_, hook_);
  slot_state_.reset();
  schema_token_.reset();
}

CaptureResult
ContextCapturePool::Lease::capture(const CaptureEventMetadata& event,
                                   ::tracer::compat::span<const CaptureSchemaView> schemas,
                                   ::tracer::compat::span<const CaptureStateView> states) noexcept {
  if (!slot_state_ || captured_ || !valid_identifier(event.event_id) ||
      !valid_identifier(event.source_id) || !valid_identifier(event.task_id) ||
      !valid_identifier(event.parent_task_id, true) ||
      !valid_identifier(event.parent_source_id, true) ||
      !valid_identifier(event.invocation_id, true) ||
      !valid_identifier(event.probe_id, true) ||
      event.effective_max_items > 4096)
    return {CaptureStatus::invalid_field, 0};
  if (schemas.size() > CaptureSchemaSetLease::capacity || states.size() > 64)
    return {CaptureStatus::invalid_field, 0};
  slot_state_->prepare(hook_);
  for (std::size_t i = 0; i < schemas.size(); ++i) {
    const auto &schema = schemas[i];
    if (schema.identity.empty() || schema.root_type.empty() ||
        schema.descriptor_set.empty() ||
        !valid_identifier(schema.source_lineage_id) ||
        schema.cpp_state_type.empty() || schema.cpp_scope_type.empty() ||
        schema.revision_id.empty() || schema.identity != schema.revision_id ||
        !schema.epoch_schema_id)
      return {CaptureStatus::invalid_field, 0};
    if (!schema.retained_record ||
        schema.retained_record->revision_id != schema.revision_id ||
        schema.retained_record->root_type != schema.root_type ||
        schema.retained_record->descriptor_set != schema.descriptor_set ||
        schema.retained_record->family.cpp_state_type !=
            schema.cpp_state_type ||
        schema.retained_record->family.cpp_scope_type !=
            schema.cpp_scope_type ||
        !std::equal(schema.retained_record->family.source_lineage_id.begin(),
                    schema.retained_record->family.source_lineage_id.end(),
                    reinterpret_cast<const std::uint8_t *>(
                        schema.source_lineage_id.data())) ||
        schema.retained_record->container_roles.size() !=
            schema.container_roles.size())
      return {CaptureStatus::invalid_field, 0};
    for (std::size_t role = 0; role < schema.container_roles.size(); ++role)
      if (schema.retained_record->container_roles[role] !=
          schema.container_roles[role])
        return {CaptureStatus::invalid_field, 0};
    for (std::size_t j = 0; j < i; ++j)
      if (schemas[j].identity == schema.identity)
        return {CaptureStatus::invalid_field, 0};
  }
  for (const auto &state : states) {
    if (state.schema_identity.empty() || !state.epoch_schema_id ||
        std::none_of(schemas.begin(), schemas.end(), [&](const auto &schema) {
          return schema.identity == state.schema_identity &&
                 schema.epoch_schema_id == state.epoch_schema_id;
        }))
      return {CaptureStatus::invalid_field, 0};
  }
  for (const auto &schema : schemas) {
    const bool referenced =
        std::any_of(states.begin(), states.end(), [&](const auto &state) {
          return state.schema_identity == schema.identity &&
                 state.epoch_schema_id == schema.epoch_schema_id;
        });
    if (!referenced)
      continue;
    if (!slot_state_->retain(schema.retained_record, schema.epoch_schema_id))
      return {CaptureStatus::invalid_field, 0};
  }

  CaptureLimits limits;
  limits.bytes = kCaptureEventBytes;
  limits.string_bytes = 64 * 1024;
  limits.visits = 8192; // The codec budget accounts for the other 8192.
  limits.items = 4096;
  limits.depth = 32;
  const ::tracer::compat::span<std::byte> event_storage(
      slot_state_->arena->storage.data() + slot_ * kCaptureEventBytes, kCaptureEventBytes);
  BoundedProtoWriter writer(event_storage, limits);
  if (!writer.string(1, event.event_id) || !writer.string(2, event.source_id) ||
      !writer.u64(3, event.source_sequence) ||
      !writer.string(4, event.task_id) ||
      !writer.string(5, event.parent_task_id) || !writer.string(6, event.name))
    return {writer_status(writer), 0};

  EncodeBudget budget;
  budget.max_items = event.effective_max_items;
  budget.limits = limits;
  budget.limits.visits = 8192;
  TruncationLog truncations;
  budget.truncation = record_truncation;
  budget.truncation_context = &truncations;
  for (const auto &state : states) {
    if (!state.encode || !state.payload || !writer.begin_message(7) ||
        !writer.u64(1, state.scope_id) ||
        !writer.string(2, state.schema_identity) || !writer.begin_message(3))
      return {writer_status(writer), 0};
    truncations.size = 0;
    if (!state.encode(state.encode_context, writer, state.payload, budget)) {
      const auto status = budget_status(budget);
      return {writer.status() == CaptureStatus::ok ? status
                                                   : writer_status(writer),
              0};
    }
    if (!writer.end_message())
      return {writer_status(writer), 0};
    for (std::size_t i = 0; i < truncations.size; ++i)
      if (!write_truncation(writer, truncations.entries[i]))
        return {writer_status(writer), 0};
    if (!writer.u64(5, state.epoch_schema_id) || !writer.end_message())
      return {writer_status(writer), 0};
  }
  if (event.recording_position && !writer.u64(8, event.recording_position))
    return {writer_status(writer), 0};
  if (!event.opaque_metadata.empty() &&
      !writer.string(9, event.opaque_metadata))
    return {writer_status(writer), 0};
  if (!writer.u64(10, event.effective_max_items))
    return {writer_status(writer), 0};
  if (!event.invocation_id.empty() && !writer.string(11, event.invocation_id))
    return {writer_status(writer), 0};
  if (!event.probe_id.empty() && !writer.string(12, event.probe_id))
    return {writer_status(writer), 0};
  if (event.trace_context &&
      !write_trace_context(writer, *event.trace_context, event.print_arguments,
                           event.print_result))
    return {writer_status(writer), 0};
  if (!event.parent_source_id.empty() &&
      !writer.string(17, event.parent_source_id))
    return {writer_status(writer), 0};
  if (event.parent_submission_sequence &&
      !writer.u64(18, event.parent_submission_sequence))
    return {writer_status(writer), 0};
  if ((event.print_arguments && !writer.boolean(19, true)) ||
      (event.print_result && !writer.boolean(20, true)))
    return {writer_status(writer), 0};
  const auto bytes = writer.finish();
  if (bytes.empty())
    return {writer.status(), 0};
  bytes_ = bytes.size();
  slot_state_->set_event_bytes_size(bytes_);
  captured_ = true;
  return {CaptureStatus::ok, bytes_};
}

bool ContextCapturePool::Lease::publish(Publish publish,
                                        void *argument) noexcept {
  if (!slot_state_ || !captured_)
    return false;
  const auto packet = bytes();
  const bool ok = publish && publish(argument, packet);
  reset();
  return ok;
}

bool ContextCapturePool::Lease::publish_leased(LeasedPublish publish,
                                               void *argument) noexcept {
  if (!slot_state_ || !captured_ || published_)
    return false;
  slot_state_->mark_published(true);
  const auto packet = bytes();
  const bool ok = publish && publish(argument, packet, schema_token_);
  if (!ok) {
    slot_state_->mark_published(false);
    reset();
    return false;
  }
  published_ = true;
  slot_state_.reset();
  schema_token_.reset();
  return true;
}

std::shared_ptr<const void>
ContextCapturePool::Lease::schema_lease_token() const noexcept {
  if (!slot_state_ || !captured_)
    return {};
  return schema_token_;
}

::tracer::compat::span<const std::byte> ContextCapturePool::Lease::bytes() const noexcept {
  if (!slot_state_ || !captured_)
    return {};
  return {slot_state_->arena->storage.data() + slot_ * kCaptureEventBytes,
          bytes_};
}

ContextCapturePool::Lease
ContextCapturePool::try_reserve(std::uint32_t hook_id) noexcept {
  if (hook_id >= kCaptureHooks)
    return {};
  auto &outstanding = arena_->hook_outstanding[hook_id];
  auto count = outstanding.load(std::memory_order_relaxed);
  do {
    if (count >= kCaptureEventsPerHook)
      return {};
  } while (!outstanding.compare_exchange_weak(
      count, static_cast<std::uint8_t>(count + 1), std::memory_order_acq_rel,
      std::memory_order_relaxed));
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    bool free = false;
    if (slots_[i]->active.compare_exchange_strong(
            free, true, std::memory_order_acq_rel, std::memory_order_relaxed))
      return Lease(slots_[i], i, hook_id);
  }
  outstanding.fetch_sub(1, std::memory_order_release);
  return {};
}

std::size_t
ContextCapturePool::outstanding(std::uint32_t hook_id) const noexcept {
  if (hook_id >= kCaptureHooks)
    return {};
  return arena_->hook_outstanding[hook_id].load(std::memory_order_acquire);
}

void CaptureArena::release(CaptureSlotState &slot,
                           std::uint32_t hook) noexcept {
  if (hook >= hook_outstanding.size())
    return;
  auto &schema_set = slot.schema_set;
  schema_set.size_ = 0;
  schema_set.event_bytes_size_ = 0;
  schema_set.published_ = false;
  for (auto &record : schema_set.records_)
    record.reset();
  schema_set.epoch_schema_ids_.fill(0);
  if (!slot.active.exchange(false, std::memory_order_acq_rel))
    return;
  hook_outstanding[hook].fetch_sub(1, std::memory_order_release);
}

std::uint64_t
CaptureSchemaSetLease::epoch_schema_id(std::size_t index) const noexcept {
  return index < size_ ? epoch_schema_ids_[index] : 0;
}

const SchemaRecord *
CaptureSchemaSetLease::record(std::size_t index) const noexcept {
  return index < size_ ? records_[index].get() : nullptr;
}

::tracer::compat::span<const std::byte> CaptureSchemaSetLease::event_bytes() const noexcept {
  if (!arena_ || !event_bytes_size_ || event_bytes_size_ > kCaptureEventBytes)
    return {};
  return {arena_->storage.data() + slot_ * kCaptureEventBytes,
          event_bytes_size_};
}

const CaptureSchemaSetLease *
capture_schema_set(const std::shared_ptr<const void> &token) noexcept {
  return token ? static_cast<const CaptureSchemaSetLease *>(token.get())
               : nullptr;
}

void release_capture_schema_set(
    const std::shared_ptr<const void> &token) noexcept {
  const auto *lease = capture_schema_set(token);
  if (!lease || !lease->arena_ || !lease->owner_ ||
      lease->slot_ >= kCaptureSlots || !lease->published_)
    return;
  auto *mutable_lease = const_cast<CaptureSchemaSetLease *>(lease);
  mutable_lease->published_ = false;
  mutable_lease->arena_->release(*mutable_lease->owner_, mutable_lease->hook_);
}

} // namespace tracer::context_engine

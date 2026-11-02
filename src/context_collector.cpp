#include "context_collector.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "tracer/compat.hpp"

namespace tracer::context_delivery {
namespace {
using namespace clangtracer::context::v1;
using namespace context_engine;

class EnvelopeMemoryLedger final {
public:
  bool reserve_schema(std::size_t bytes) noexcept {
    std::lock_guard lock(mutex_);
    if (schemas_ >= 4096 || bytes > 64 * 1024 * 1024 - schema_bytes_)
      return false;
    schema_bytes_ += bytes;
    ++schemas_;
    return true;
  }
  void release_schemas(std::size_t bytes, std::size_t count) noexcept {
    std::lock_guard lock(mutex_);
    schema_bytes_ -= std::min(schema_bytes_, bytes);
    schemas_ -= std::min(schemas_, count);
  }
  bool reserve_envelope(std::size_t bytes) noexcept {
    std::lock_guard lock(mutex_);
    if (bytes > 32 * 1024 * 1024 - envelope_bytes_)
      return false;
    envelope_bytes_ += bytes;
    return true;
  }
  void release_envelope(std::size_t bytes) noexcept {
    std::lock_guard lock(mutex_);
    envelope_bytes_ -= std::min(envelope_bytes_, bytes);
  }

private:
  std::mutex mutex_;
  std::size_t schema_bytes_ = 0, schemas_ = 0, envelope_bytes_ = 0;
};

std::shared_ptr<EnvelopeMemoryLedger> process_envelope_ledger() {
  static auto ledger = std::make_shared<EnvelopeMemoryLedger>();
  return ledger;
}

class ReceiverProcessor final : public ContextFrameProcessor {
public:
  ReceiverProcessor(std::shared_ptr<ReceiverLedger> ledger,
                    ReceiverLimits limits, StreamLimits stream_limits,
                    ExportOpen open,
                    std::shared_ptr<EnvelopeMemoryLedger> envelope_ledger,
                    ContextCollector::EventSink sink)
      : receiver_(std::move(ledger), limits), stream_limits_(stream_limits),
        open_(std::move(open)), envelope_ledger_(std::move(envelope_ledger)),
        sink_(std::move(sink)) {}

  ~ReceiverProcessor() override {
    envelope_ledger_->release_schemas(schema_storage_bytes_,
                                      schema_storage_count_);
  }

  bool process(const ContextFrame &frame, std::string &error) override {
    try {
      if (frame.has_schema()) {
        return stage_schema(frame.schema(), error);
      }
      if (!frame.has_event()) {
        error = "empty context frame";
        return false;
      }
      if (!pending_schemas_.empty() && !register_schema_set(error)) {
        return false;
      }
      const auto key = frame.event().SerializeAsString();
      if (!pending_event_ || key != pending_key_) {
        std::shared_ptr<const DecodedEvent> decoded;
        if (!decode_event(frame.event(), decoded, error)) {
          return false;
        }
        pending_key_ = key;
        pending_event_ = std::move(decoded);
      }
      clangtracer::context::v1::ContextEventEnvelope envelope;
      envelope.set_producer_session_id(open_.producer_session_id());
      envelope.set_epoch_id(open_.epoch_id());
      std::size_t envelope_charge = frame.event().ByteSizeLong();
      std::set<std::uint64_t> charged_schema_ids;
      for (const auto &state : frame.event().states()) {
        if (!charged_schema_ids.insert(state.epoch_schema_id()).second)
          continue;
        const auto schema = schemas_by_id_.find(state.epoch_schema_id());
        if (schema == schemas_by_id_.end()) {
          error = "event references an unregistered schema in this epoch";
          return false;
        }
        envelope_charge += schema->second.ByteSizeLong() + 16;
      }
      envelope_charge += 64;
      if (!envelope_ledger_->reserve_envelope(envelope_charge))
        return false;
      struct EnvelopeReservation {
        std::shared_ptr<EnvelopeMemoryLedger> ledger;
        std::size_t bytes;
        ~EnvelopeReservation() { ledger->release_envelope(bytes); }
      } reservation{envelope_ledger_, envelope_charge};
      envelope.mutable_event()->CopyFrom(frame.event());
      std::set<std::uint64_t> included_schema_ids;
      for (const auto &state : frame.event().states()) {
        const auto schema = schemas_by_id_.find(state.epoch_schema_id());
        if (schema == schemas_by_id_.end()) {
          error = "event references an unregistered schema in this epoch";
          return false;
        }
        if (included_schema_ids.insert(state.epoch_schema_id()).second) {
          envelope.add_schemas()->CopyFrom(schema->second);
        }
      }
      if (sink_ && !sink_(open_, envelope, pending_event_)) {
        return false;
      }
      pending_event_.reset();
      pending_key_.clear();
      return true;
    } catch (const std::exception &exception) {
      error = exception.what();
    } catch (...) {
      error = "receiver allocation or decode failed";
    }
    return false;
  }

private:
  bool stage_schema(const clangtracer::context::v1::SchemaRecord &schema,
                    std::string &error) {
    if (pending_schemas_.size() >= stream_limits_.max_epoch_schemas) {
      error = "pending schema set exceeds negotiated count";
      return false;
    }
    if (schema.source_lineage_id().size() != 16) {
      error = "schema source lineage must be a 16-byte UUID";
      return false;
    }
    google::protobuf::FileDescriptorSet descriptors;
    if (!descriptors.ParseFromString(schema.serialized_file_descriptor_set())) {
      error = "invalid serialized FileDescriptorSet";
      return false;
    }
    StateFamilyKey family;
    std::copy(schema.source_lineage_id().begin(),
              schema.source_lineage_id().end(),
              family.source_lineage_id.begin());
    family.cpp_state_type = schema.cpp_state_type();
    family.cpp_scope_type = schema.cpp_scope_type();
    std::vector<std::string> roles(schema.container_roles().begin(),
                                   schema.container_roles().end());
    std::string canonical_descriptor;
    if (!descriptors.SerializeToString(&canonical_descriptor) ||
        canonical_descriptor != schema.serialized_file_descriptor_set()) {
      error = "descriptor set is not in canonical serialized form";
      return false;
    }
    const auto identity = compute_schema_revision_id(
        canonical_descriptor, family, schema.root_type(), roles);
    if (schema.schema_identity() != identity ||
        schema.revision_id() != identity) {
      error =
          "schema identity does not match family and exact descriptor revision";
      return false;
    }
    SchemaRegistration registration;
    registration.id = schema.epoch_schema_id();
    registration.descriptor_set = std::move(descriptors);
    registration.root = schema.root_type();
    registration.family = std::move(family);
    registration.container_roles = std::move(roles);
    const auto charge = schema.ByteSizeLong();
    pending_schemas_.push_back(std::move(registration));
    pending_wire_schemas_.push_back(schema);
    if (!envelope_ledger_->reserve_schema(charge)) {
      pending_schemas_.pop_back();
      pending_wire_schemas_.pop_back();
      error = "global retained descriptor envelope budget is full";
      return false;
    }
    schema_storage_bytes_ += charge;
    ++schema_storage_count_;
    return true;
  }

  bool register_schema_set(std::string &error) {
    if (!receiver_.register_schema_set(pending_schemas_, &error)) {
      return false;
    }
    for (auto &schema : pending_wire_schemas_) {
      if (!schemas_by_id_.emplace(schema.epoch_schema_id(), std::move(schema))
               .second) {
        error = "duplicate schema ID in stream epoch";
        return false;
      }
    }
    pending_schemas_.clear();
    pending_wire_schemas_.clear();
    return true;
  }

  bool decode_event(const EventRecord &event,
                    std::shared_ptr<const DecodedEvent> &decoded,
                    std::string &error) {
    std::vector<std::vector<CapturePathStep>> paths;
    std::vector<std::vector<CaptureLimitView>> limits;
    std::vector<StateAttachmentView> attachments;
    std::size_t path_count = 0;
    for (const auto &state : event.states()) {
      path_count += static_cast<std::size_t>(state.truncations_size());
    }
    if (path_count > 4096) {
      error = "truncation record count exceeds receiver cap";
      return false;
    }
    paths.reserve(path_count);
    limits.reserve(event.states_size());
    attachments.reserve(event.states_size());
    for (const auto &state : event.states()) {
      auto &state_limits = limits.emplace_back();
      state_limits.reserve(state.truncations_size());
      for (const auto &truncation : state.truncations()) {
        auto &path = paths.emplace_back();
        path.reserve(truncation.path_size());
        for (const auto &step : truncation.path()) {
          CapturePathStep item;
          switch (step.value_case()) {
          case TruncationPathStep::kFieldNumber:
            item.selector = CapturePathSelector::field_number;
            item.number = step.field_number();
            break;
          case TruncationPathStep::kListIndex:
            item.selector = CapturePathSelector::list_index;
            item.number = step.list_index();
            break;
          case TruncationPathStep::kStringKey:
            item.selector = CapturePathSelector::string_key;
            item.string_value = step.string_key();
            break;
          case TruncationPathStep::kSignedKey:
            item.selector = CapturePathSelector::signed_key;
            item.signed_value = step.signed_key();
            break;
          case TruncationPathStep::kUnsignedKey:
            item.selector = CapturePathSelector::unsigned_key;
            item.number = step.unsigned_key();
            break;
          case TruncationPathStep::kBoolKey:
            item.selector = CapturePathSelector::bool_key;
            item.bool_value = step.bool_key();
            break;
          case TruncationPathStep::VALUE_NOT_SET:
            error = "truncation path contains an empty typed step";
            return false;
          }
          path.push_back(item);
        }
        state_limits.push_back(
            {path, truncation.total_items(), truncation.captured_items()});
      }
      const auto *payload =
          reinterpret_cast<const std::byte *>(state.payload().data());
      attachments.push_back(
          {state.epoch_schema_id(),
           ::tracer::compat::span<const std::byte>(payload, state.payload().size()), state_limits});
    }
    auto result = receiver_.decode_event(attachments, &error);
    if (!result) {
      return false;
    }
    decoded = std::make_shared<const DecodedEvent>(std::move(*result));
    return true;
  }

  ContextReceiver receiver_;
  const StreamLimits stream_limits_;
  const ExportOpen open_;
  std::shared_ptr<EnvelopeMemoryLedger> envelope_ledger_;
  ContextCollector::EventSink sink_;
  std::size_t schema_storage_bytes_ = 0, schema_storage_count_ = 0;
  std::vector<SchemaRegistration> pending_schemas_;
  std::vector<clangtracer::context::v1::SchemaRecord> pending_wire_schemas_;
  std::unordered_map<std::uint64_t, clangtracer::context::v1::SchemaRecord>
      schemas_by_id_;
  std::string pending_key_;
  std::shared_ptr<const DecodedEvent> pending_event_;
};

std::shared_ptr<ReceiverLedger> process_receiver_ledger() {
  static auto ledger = std::make_shared<ReceiverLedger>();
  return ledger;
}
} // namespace

ContextCollector::ContextCollector(
    std::shared_ptr<ContextCollectorStore> durable_store, StreamLimits limits,
    EventSink sink, DurableStoreFactory durable_store_factory)
    : receiver_ledger_(process_receiver_ledger()),
      service_(std::move(durable_store), {}, limits, {},
               [ledger = receiver_ledger_,
                envelope_ledger = process_envelope_ledger(),
                sink = std::make_shared<EventSink>(std::move(sink))](
                   const ExportOpen &open, const StreamLimits &negotiated) {
                 ReceiverLimits receiver_limits;
                 receiver_limits.frame_bytes = negotiated.max_packet_bytes;
                 receiver_limits.event_bytes = negotiated.max_event_bytes;
                 receiver_limits.schemas_per_epoch =
                     negotiated.max_epoch_schemas;
                 receiver_limits.epoch_schema_bytes =
                     negotiated.max_epoch_schema_bytes;
                 auto event_sink = *sink;
                 return std::make_unique<ReceiverProcessor>(
                     ledger, receiver_limits, negotiated, open, envelope_ledger,
                     std::move(event_sink));
               }, std::move(durable_store_factory)) {}

} // namespace tracer::context_delivery

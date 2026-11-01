#pragma once

#include "context_schema.hpp"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>

namespace tracer::context_engine {
struct ReceiverLimits {
  std::size_t frame_bytes = 2 * 1024 * 1024, event_bytes = 256 * 1024,
              descriptor_bytes = 1024 * 1024;
  std::size_t schemas_per_epoch = 256, epoch_schema_bytes = 16 * 1024 * 1024;
  std::size_t global_schema_bytes = 64 * 1024 * 1024,
              decoded_bytes = 16 * 1024 * 1024;
  std::size_t files = 64, messages = 1024, fields = 8192, depth = 32;
  std::size_t truncation_records = 4096, path_depth = 32;
};
class ReceiverLedger;
struct SchemaEntry;
struct DecodedPathStep {
  CapturePathSelector selector = CapturePathSelector::field_number;
  std::uint64_t number = 0;
  std::int64_t signed_value = 0;
  bool bool_value = false;
  std::string string_value;
};
struct DecodedCaptureLimit {
  std::vector<DecodedPathStep> path;
  std::uint64_t total_items = 0, captured_items = 0;
};
class DecodedState final {
public:
  DecodedState() = default;
  const google::protobuf::Message *message() const noexcept {
    return message_.get();
  }
  const SchemaRecord *schema() const noexcept;
  std::span<const DecodedCaptureLimit> truncations() const noexcept {
    return truncations_;
  }

private:
  friend class ContextReceiver;
  std::shared_ptr<const SchemaEntry> entry_;
  std::shared_ptr<google::protobuf::Message> message_;
  std::shared_ptr<void> lease_;
  std::vector<DecodedCaptureLimit> truncations_;
};
struct StateAttachmentView {
  std::uint64_t schema_id = 0;
  std::span<const std::byte> payload;
  std::span<const CaptureLimitView> truncations{};
};
struct DecodedEvent {
  std::vector<DecodedState> states;
};
struct SchemaRegistration {
  std::uint64_t id = 0;
  google::protobuf::FileDescriptorSet descriptor_set;
  std::string root;
  StateFamilyKey family;
  std::vector<std::string> container_roles;
};

// An admitted schema snapshot that pins its DescriptorPool and ledger charge.
// The protobuf Descriptor remains valid for the complete lifetime of the view.
class ReceiverSchemaView final {
public:
  const SchemaRecord &record() const noexcept;
  const google::protobuf::Descriptor *root_descriptor() const noexcept;
  const StateFamilyKey &family() const noexcept;
  std::string_view root_type() const noexcept;
  std::string_view revision_id() const noexcept;
  std::span<const std::string> container_roles() const noexcept;

private:
  friend class ContextReceiver;
  explicit ReceiverSchemaView(std::shared_ptr<const SchemaEntry>);
  std::shared_ptr<const SchemaEntry> entry_;
};

// An epoch receiver owns its mapping leases. DecodedState separately retains
// the immutable pool entry and decoded-memory lease across close_epoch().
class ContextReceiver final {
public:
  explicit ContextReceiver(std::shared_ptr<ReceiverLedger>,
                           ReceiverLimits = {});
  ~ContextReceiver();
  ContextReceiver(const ContextReceiver &) = delete;
  ContextReceiver &operator=(const ContextReceiver &) = delete;
  bool register_schema(std::uint64_t id,
                       const google::protobuf::FileDescriptorSet &,
                       const std::string &root, StateFamilyKey,
                       std::string *error = nullptr,
                       std::span<const std::string> container_roles = {});
  bool register_schema_set(std::span<const SchemaRegistration>,
                           std::string *error = nullptr);
  // Engine-only compatibility path for descriptor-only legacy records. The
  // synthesized family is bound to this receiver epoch and schema ID.
  bool register_legacy_schema(std::uint64_t id,
                              const google::protobuf::FileDescriptorSet &,
                              const std::string &root,
                              std::string *error = nullptr);
  std::optional<DecodedState> decode(std::uint64_t id,
                                     std::span<const std::byte>,
                                     std::string *error = nullptr);
  std::optional<DecodedState> decode(std::uint64_t id,
                                     std::span<const std::byte>,
                                     std::span<const CaptureLimitView>,
                                     std::string *error = nullptr);
  std::optional<DecodedEvent> decode_event(std::span<const StateAttachmentView>,
                                           std::string *error = nullptr);
  void close_epoch() noexcept;
  std::size_t schema_count() const noexcept;
  std::vector<ReceiverSchemaView> admitted_schemas() const;

private:
  struct Mapping;
  std::shared_ptr<ReceiverLedger> ledger_;
  ReceiverLimits limits_;
  mutable std::mutex mutex_;
  std::unordered_map<std::uint64_t, Mapping> schemas_;
  std::size_t epoch_bytes_ = 0;
  std::uint64_t legacy_epoch_nonce_ = 0;
  bool closed_ = false;
};

class ReceiverLedger final {
public:
  struct State;
  explicit ReceiverLedger(ReceiverLimits = {});
  std::size_t retained_schema_bytes() const noexcept;
  std::size_t retained_decoded_bytes() const noexcept;

private:
  friend class ContextReceiver;
  std::shared_ptr<State> state_;
};
} // namespace tracer::context_engine

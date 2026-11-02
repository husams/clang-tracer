#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "context_capture.hpp"
#include "context_schema.hpp"
#include "context_transport.pb.h"
#include "tracer/compat.hpp"
#include "tracer/protocol.hpp"

namespace tracer::context_engine::test {
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

std::string id(char c) { return std::string(16, c); }

struct LeasedSink {
  std::shared_ptr<const void> token;
  std::size_t bytes = 0;
};

bool retain_capture(void* opaque, ::tracer::compat::span<const std::byte> bytes,
                    const std::shared_ptr<const void>& token) noexcept {
  auto &sink = *static_cast<LeasedSink *>(opaque);
  sink.token = token;
  sink.bytes = bytes.size();
  return true;
}
} // namespace

void verify_context_capture() {
  auto pool = std::make_unique<ContextCapturePool>();
  auto lease = pool->try_reserve(3);
  require(bool(lease), "capture reservation failed");
  require(pool->outstanding(3) == 1, "capture quota was not reserved");

  const auto lineage = id('L');
  const auto event_id = id('E');
  const auto source_id = id('S');
  const auto task_id = id('T');
  const auto parent_task_id = id('P');
  const auto parent_source_id = id('R');
  const auto invocation_id = id('I');
  const auto probe_id = id('Q');
  tracer::TraceContext trace{};
  trace.argument_count = 2;
  trace.argument_names[0] = "count";
  trace.arguments[0] = {tracer::ValueType::Int32, 0, -7, 0, 0.0};
  trace.argument_names[1] = "address";
  trace.arguments[1] = {tracer::ValueType::Pointer, 0, 0, 0x1234, 0.0};
  trace.named_count = 1;
  trace.named_values[0].kind = 2;
  trace.named_values[0].name[0] = 'f';
  trace.named_values[0].name[1] = '\0';
  trace.named_values[0].value = {tracer::ValueType::Float64, 0, 0, 0, 2.5};
  trace.phase = tracer::TracePhase::Post;
  trace.result = {tracer::ValueType::UInt64, 0, 0, 42, 0};
  std::array<std::string, 1> roles{"1=vector"};
  std::array<std::uint8_t, 16> lineage_bytes{};
  lineage_bytes.fill('L');
  StateFamilyKey family{lineage_bytes, "int", "demo::Scope"};
  const std::string large_descriptor(400 * 1024, 'D');
  auto schema_record = std::make_shared<const SchemaRecord>(
      family, "demo::Numbers", "schema-A", large_descriptor, nullptr,
      std::vector<std::string>{"1=vector"});
  std::array<CaptureSchemaView, 1> schemas{
      {{"schema-A", "demo::Numbers", large_descriptor, lineage, "int",
        "demo::Scope", "schema-A", roles, 7, schema_record}}};
  const CaptureEventMetadata metadata{event_id,
                                      source_id,
                                      19,
                                      task_id,
                                      parent_task_id,
                                      parent_source_id,
                                      123,
                                      "numbers",
                                      23,
                                      "opaque",
                                      2,
                                      invocation_id,
                                      probe_id,
                                      &trace,
                                      true,
                                      true};
  const std::vector<int> values{10, 20, 30, 40};
  const CaptureStateView state{
      11,      "schema-A", 7,
      &values, nullptr,    encode_capture_payload_root<std::vector<int>>};
  const auto result = lease.capture(metadata, schemas, {&state, 1});
  require(bool(result), "capture serialization failed");
  require(result.bytes <= kCaptureEventBytes, "capture exceeded event bound");
  require(schema_record->descriptor_set.size() > kCaptureEventBytes,
          "large descriptor fixture did not exceed the event limit");
  require(lease.bytes().size() == result.bytes, "captured byte view mismatch");

  clangtracer::context::v1::EventRecord event;
  require(event.ParseFromArray(lease.bytes().data(),
                               static_cast<int>(lease.bytes().size())),
          "serialized event record was not valid protobuf");
  require(event.states_size() == 1, "event omitted state payload");
  const auto schema_token = lease.schema_lease_token();
  const auto *schema_set = capture_schema_set(schema_token);
  require(schema_set && schema_set->size() == 1 &&
              schema_set->epoch_schema_id(0) == 7 &&
              schema_set->record(0) == schema_record.get() &&
              schema_set->record(0)->container_roles.size() == 1 &&
              schema_set->record(0)->container_roles[0] == "1=vector",
          "schema lease did not preserve retained metadata");
  require(event.event_id() == event_id && event.source_sequence() == 19 &&
              event.parent_source_id() == parent_source_id &&
              event.parent_submission_sequence() == 123 &&
              event.print_arguments() && event.print_result() &&
              event.states(0).scope_id() == 11,
          "event identity metadata was not preserved");
  require(event.arguments_size() == 2 && event.arguments(0).name() == "count" &&
              event.arguments(0).value().value_type() ==
                  static_cast<std::uint32_t>(tracer::ValueType::Int32) &&
              event.arguments(0).value().value_case() ==
                  clangtracer::context::v1::TypedTraceValue::kSignedValue &&
              event.arguments(0).value().signed_value() == -7 &&
              event.arguments(1).value().value_case() ==
                  clangtracer::context::v1::TypedTraceValue::kUnsignedValue &&
              event.arguments(1).value().unsigned_value() == 0x1234,
          "typed PRE/POST arguments were not preserved");
  require(event.named_values_size() == 1 && event.named_values(0).kind() == 2 &&
              event.named_values(0).name() == "f" &&
              event.named_values(0).value().value_case() ==
                  clangtracer::context::v1::TypedTraceValue::kRealValue &&
              event.named_values(0).value().real_value() == 2.5 &&
              event.has_result() &&
              event.result().value_type() ==
                  static_cast<std::uint32_t>(tracer::ValueType::UInt64) &&
              event.result().unsigned_value() == 42 &&
              event.phase() ==
                  static_cast<std::uint32_t>(tracer::TracePhase::Post),
          "named values, POST result, or phase were not preserved");
  require(event.states(0).truncations_size() == 1,
          "truncated vector had no truncation record");
  const auto &truncation = event.states(0).truncations(0);
  require(truncation.total_items() == 4 && truncation.captured_items() == 2 &&
              truncation.path_size() == 1 &&
              truncation.path(0).value_case() ==
                  clangtracer::context::v1::TruncationPathStep::kFieldNumber &&
              truncation.path(0).field_number() == 1,
          "typed truncation path or item counts were incorrect");

  LeasedSink sink;
  require(lease.publish_leased(&retain_capture, &sink),
          "capture did not publish with its schema lease");
  require(sink.bytes == result.bytes && pool->outstanding(3) == 1,
          "published schema lease released capture storage too early");
  schema_record.reset();
  schemas[0].retained_record.reset();
  const auto *retained = capture_schema_set(sink.token);
  require(retained && retained->size() == 1 &&
              retained->record(0)->descriptor_set.size() > kCaptureEventBytes,
          "queued schema lease failed to pin large descriptor metadata");
  release_capture_schema_set(sink.token);
  require(pool->outstanding(3) == 0,
          "terminal schema lease did not release capture reservation");
  sink.token.reset();

  auto second = pool->try_reserve(3);
  require(bool(second), "same hook quota rejected an available slot");
  std::array<ContextCapturePool::Lease, 14> more;
  for (auto &entry : more) {
    entry = pool->try_reserve(3);
    require(bool(entry), "hook quota rejected one of its 16 slots");
  }
  auto sixteenth = pool->try_reserve(3);
  require(bool(sixteenth), "hook quota rejected its sixteenth reservation");
  require(!pool->try_reserve(3), "hook admitted more than 16 reservations");
  require(pool->outstanding(3) == 16, "hook outstanding count mismatch");
  sixteenth = {};
  require(pool->outstanding(3) == 15, "lease release did not free quota");
  require(bool(pool->try_reserve(3)), "released quota was not reusable");
  require(!pool->try_reserve(kCaptureHooks), "invalid hook id was accepted");
  second = {};
  for (auto &entry : more)
    entry = {};
  pool.reset();

  auto short_lived_pool = std::make_unique<ContextCapturePool>();
  auto detached = short_lived_pool->try_reserve(9);
  const std::string detached_descriptor(300 * 1024, 'X');
  std::array<std::uint8_t, 16> detached_lineage{};
  detached_lineage.fill('V');
  StateFamilyKey detached_family{detached_lineage, "int", "demo::Scope"};
  auto detached_record = std::make_shared<const SchemaRecord>(
      detached_family, "demo::Numbers", "detached-revision",
      detached_descriptor, nullptr);
  const auto detached_lineage_bytes = id('V');
  std::array<CaptureSchemaView, 1> detached_schemas{{{"detached-revision",
                                                      "demo::Numbers",
                                                      detached_descriptor,
                                                      detached_lineage_bytes,
                                                      "int",
                                                      "demo::Scope",
                                                      "detached-revision",
                                                      {},
                                                      3,
                                                      detached_record}}};
  const CaptureStateView detached_state{
      4,       "detached-revision",
      3,       &values,
      nullptr, encode_capture_payload_root<std::vector<int>>};
  const auto detached_result =
      detached.capture(metadata, detached_schemas, {&detached_state, 1});
  require(bool(detached_result), "short-lived pool capture failed");
  LeasedSink detached_sink;
  require(detached.publish_leased(&retain_capture, &detached_sink),
          "short-lived pool lease publication failed");
  detached_record.reset();
  detached_schemas[0].retained_record.reset();
  short_lived_pool.reset();
  const auto *detached_set = capture_schema_set(detached_sink.token);
  require(detached_set && detached_set->size() == 1 &&
              detached_set->record(0)->descriptor_set == detached_descriptor,
          "queued metadata lease did not pin its arena after pool destruction");
  clangtracer::context::v1::EventRecord detached_event;
  const auto detached_bytes = detached_set->event_bytes();
  require(detached_event.ParseFromArray(
              detached_bytes.data(), static_cast<int>(detached_bytes.size())) &&
              detached_event.event_id() == metadata.event_id,
          "queued token did not retain its event bytes after pool destruction");
  release_capture_schema_set(detached_sink.token);
  detached_sink.token.reset();
}

} // namespace tracer::context_engine::test

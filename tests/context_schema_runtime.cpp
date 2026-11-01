// clang-format off
#include "tracer/callback.hpp"
#include "fixtures/context_schema_catalog.cpp"
#include "fixtures/context_schema_generic.cpp"
#include "fixtures/context_schema_history_v1.cpp"
#include "fixtures/context_schema_query.cpp"
#include "fixtures/shared_context_nested.cpp"
#include "context_codec.hpp"
#include "context_schema_generated.hpp"
#include "context_query.hpp"
#include "context_receiver.hpp"
// clang-format on

#include <algorithm>
#include <array>
#include <stdexcept>

namespace {
using namespace tracer::context_engine;
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <std::size_t N>
std::string as_string(const unsigned char (&bytes)[N]) {
  return {reinterpret_cast<const char *>(bytes), N};
}
template <class T, std::size_t N, std::size_t M>
void register_schema(ContextReceiver &r, std::uint64_t id,
                     const unsigned char (&bytes)[N], const char *root,
                     const char *state, const char *scope,
                     const unsigned char (&lineage)[16],
                     const char *const (&role_values)[M],
                     std::size_t role_count) {
  google::protobuf::FileDescriptorSet set;
  auto encoded = as_string(bytes);
  require(set.ParseFromString(encoded), "descriptor bundle failed to parse");
  StateFamilyKey key;
  key.cpp_state_type = state;
  key.cpp_scope_type = scope;
  std::copy(lineage, lineage + 16, key.source_lineage_id.begin());
  std::vector<std::string> roles;
  roles.reserve(role_count);
  for (std::size_t i = 0; i < role_count; ++i)
    roles.emplace_back(role_values[i]);
  std::string error;
  require(r.register_schema(id, set, root, key, &error, roles), error.c_str());
}
struct Limits {
  std::size_t truncations = 0;
  bool saw_billing = false, saw_unsigned_key = false;
  static bool record(void *context,
                     std::span<const EncodeBudget::PathStep> path,
                     std::uint64_t total, std::uint64_t captured) noexcept {
    auto &self = *static_cast<Limits *>(context);
    if (total <= captured)
      return false;
    ++self.truncations;
    for (const auto &step : path) {
      if (step.selector == EncodeBudget::Selector::string_key &&
          step.string_value == "billing")
        self.saw_billing = true;
      if (step.selector == EncodeBudget::Selector::unsigned_key)
        self.saw_unsigned_key = true;
    }
    return true;
  }
};
template <class T>
std::vector<std::byte> encode_root(const T &value, Limits &limits,
                                  std::size_t max_items = 1) {
  std::vector<std::byte> storage(256 * 1024);
  BoundedProtoWriter writer(storage);
  EncodeBudget budget;
  budget.max_items = max_items;
  budget.truncation = &Limits::record;
  budget.truncation_context = &limits;
  require(encode_state_payload(writer, value, budget),
          "generated bounded codec rejected fixture state");
  auto bytes = writer.finish();
  require(!bytes.empty(), "generated bounded codec wrote no root payload");
  return {bytes.begin(), bytes.end()};
}
const google::protobuf::FieldDescriptor *
field(const google::protobuf::Message &m, const char *name) {
  return m.GetDescriptor()->FindFieldByName(name);
}
const google::protobuf::Reflection *
reflection(const google::protobuf::Message &m) {
  return m.GetReflection();
}
} // namespace

void verify_context_schema_roundtrip() {
  auto ledger = std::make_shared<ReceiverLedger>();
  ContextReceiver receiver(ledger);
  register_schema<demo::RequestState>(
      receiver, 1, context_schema_generated::demo_descriptor,
      context_schema_generated::demo_root, context_schema_generated::demo_state,
      context_schema_generated::demo_scope,
      context_schema_generated::demo_lineage,
      context_schema_generated::demo_roles,
      context_schema_generated::demo_role_count);
  register_schema<catalog::CatalogState>(
      receiver, 2, context_schema_generated::catalog_descriptor,
      context_schema_generated::catalog_root,
      context_schema_generated::catalog_state,
      context_schema_generated::catalog_scope,
      context_schema_generated::catalog_lineage,
      context_schema_generated::catalog_roles,
      context_schema_generated::catalog_role_count);
  register_schema<history::State>(
      receiver, 3, context_schema_generated::history_v1_descriptor,
      context_schema_generated::history_v1_root,
      context_schema_generated::history_v1_state,
      context_schema_generated::history_v1_scope,
      context_schema_generated::history_v1_lineage,
      context_schema_generated::history_v1_roles,
      context_schema_generated::history_v1_role_count);
  // Same automatically generated StateFamily key, distinct original schema
  // revision.
  register_schema<history::State>(
      receiver, 4, context_schema_generated::history_v2_descriptor,
      context_schema_generated::history_v2_root,
      context_schema_generated::history_v2_state,
      context_schema_generated::history_v2_scope,
      context_schema_generated::history_v2_lineage,
      context_schema_generated::history_v2_roles,
      context_schema_generated::history_v2_role_count);
  register_schema<query_fixture::State>(
      receiver, 9, context_schema_generated::query_descriptor,
      context_schema_generated::query_root,
      context_schema_generated::query_state,
      context_schema_generated::query_scope,
      context_schema_generated::query_lineage,
      context_schema_generated::query_roles,
      context_schema_generated::query_role_count);
  register_schema<int>(receiver, 5,
                       context_schema_generated::generic_int_descriptor,
                       context_schema_generated::generic_int_root,
                       context_schema_generated::generic_int_state,
                       context_schema_generated::generic_int_scope,
                       context_schema_generated::generic_int_lineage,
                       context_schema_generated::generic_int_roles,
                       context_schema_generated::generic_int_role_count);
  register_schema<std::string>(
      receiver, 6, context_schema_generated::generic_text_descriptor,
      context_schema_generated::generic_text_root,
      context_schema_generated::generic_text_state,
      context_schema_generated::generic_text_scope,
      context_schema_generated::generic_text_lineage,
      context_schema_generated::generic_text_roles,
      context_schema_generated::generic_text_role_count);
  register_schema<std::vector<int>>(
      receiver, 7, context_schema_generated::generic_values_descriptor,
      context_schema_generated::generic_values_root,
      context_schema_generated::generic_values_state,
      context_schema_generated::generic_values_scope,
      context_schema_generated::generic_values_lineage,
      context_schema_generated::generic_values_roles,
      context_schema_generated::generic_values_role_count);
  register_schema<generic_payload::Object>(
      receiver, 8, context_schema_generated::generic_object_descriptor,
      context_schema_generated::generic_object_root,
      context_schema_generated::generic_object_state,
      context_schema_generated::generic_object_scope,
      context_schema_generated::generic_object_lineage,
      context_schema_generated::generic_object_roles,
      context_schema_generated::generic_object_role_count);

  query_fixture::State query_state;
  query_state.matrix = {{1, 2}, {3}};
  query_state.groups["billing"] = {{10, "invoice"}, {11, "refund"}};
  query_state.recent = std::vector<query_fixture::Record>{{90, "latest"}};
  Limits query_limits;
  auto query_bytes = encode_root(query_state, query_limits, 100);
  StateAttachmentView query_attachment{9, query_bytes};
  auto query_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&query_attachment, 1));
  require(bool(query_event), "container-aware query fixture decode failed");
  const auto &query_message = *query_event->states[0].message();
  auto admitted = receiver.admitted_schemas();
  const auto query_schema = std::find_if(admitted.begin(), admitted.end(),
      [](const auto &schema) {
        return schema.revision_id() == context_schema_generated::query_revision;
      });
  require(query_schema != admitted.end(),
          "receiver did not expose a leased query schema view");
  const auto query_paths = tracer::context_query::discover_field_paths(*query_schema);
  require(!query_paths.truncated && query_paths.paths.contains("matrix") &&
              query_paths.paths.contains("groups.label") &&
              query_paths.paths.contains("recent.id") &&
              !query_paths.paths.contains("matrix.items") &&
              !query_paths.paths.contains("groups.key") &&
              !query_paths.paths.contains("groups.value") &&
              !query_paths.paths.contains("recent.value"),
          "logical paths exposed protobuf container wrappers");
  require(tracer::context_query::query_field(query_message, *query_schema,
                                              "matrix") ==
              "[[int32:1, int32:2], [int32:3]]" &&
              tracer::context_query::query_field(query_message, *query_schema,
                                                  "groups.label") ==
              "[string:\"billing\"=[string:\"invoice\", string:\"refund\"]]" &&
              tracer::context_query::query_field(query_message, *query_schema,
                                                  "recent.id") ==
              "[int32:90]",
          "container-aware query failed to unwrap nested values or typed map keys");
  const auto capped_paths =
      tracer::context_query::discover_field_paths(*query_schema, 1);
  require(capped_paths.truncated && capped_paths.paths.size() == 1,
          "field-path discovery cap was not reported explicitly");

  int generic_int = 42;
  std::array<std::byte, 128> generic_storage{};
  BoundedProtoWriter generic_writer(generic_storage);
  EncodeBudget generic_budget;
  require(
      encode_generic_payload_root(generic_writer, generic_int, generic_budget),
      "generic int payload encode failed");
  auto generic_bytes = generic_writer.finish();
  StateAttachmentView generic_int_attachment{5, generic_bytes};
  auto generic_int_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&generic_int_attachment, 1));
  require(bool(generic_int_event), "generic int payload decode failed");
  const auto &generic_int_message = *generic_int_event->states[0].message();
  const auto *generic_value = field(generic_int_message, "value");
  require(generic_value &&
              reflection(generic_int_message)
                      ->GetInt32(generic_int_message, generic_value) == 42,
          "generic int payload value did not roundtrip");
  require(generic_int_event->states[0].schema()->revision_id ==
              context_schema_generated::generic_int_revision,
          "receiver revision identity differs from sender revision");
  std::string generic_text = "hello";
  BoundedProtoWriter text_writer(generic_storage);
  EncodeBudget text_budget;
  require(encode_generic_payload_root(text_writer, generic_text, text_budget),
          "generic string payload encode failed");
  auto text_bytes = text_writer.finish();
  StateAttachmentView text_attachment{6, text_bytes};
  auto text_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&text_attachment, 1));
  require(bool(text_event), "generic string payload decode failed");
  const auto &text_message = *text_event->states[0].message();
  require(reflection(text_message)
                  ->GetString(text_message, field(text_message, "value")) ==
              "hello",
          "generic string payload value did not roundtrip");
  std::vector<int> generic_values{3, 4, 5};
  Limits generic_limits;
  BoundedProtoWriter vector_writer(generic_storage);
  EncodeBudget vector_budget;
  vector_budget.max_items = 1;
  vector_budget.truncation = &Limits::record;
  vector_budget.truncation_context = &generic_limits;
  require(
      encode_generic_payload_root(vector_writer, generic_values, vector_budget),
      "generic vector payload encode failed");
  auto vector_bytes = vector_writer.finish();
  const std::array<CapturePathStep, 1> generic_path{
      {{CapturePathSelector::field_number, 1}}};
  const CaptureLimitView generic_limit{generic_path, 3, 1};
  StateAttachmentView vector_attachment{
      7, vector_bytes, std::span<const CaptureLimitView>(&generic_limit, 1)};
  auto vector_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&vector_attachment, 1));
  require(bool(vector_event), "generic vector payload decode failed");
  const auto &vector_message = *vector_event->states[0].message();
  const auto *vector_field = field(vector_message, "value");
  require(reflection(vector_message)->FieldSize(vector_message, vector_field) ==
                  1 &&
              reflection(vector_message)
                      ->GetRepeatedInt32(vector_message, vector_field, 0) == 3,
          "generic vector payload value/cap did not roundtrip");
  require(vector_event->states[0].truncations().size() == 1,
          "generic vector truncation metadata was lost");
  const auto generic_admitted = receiver.admitted_schemas();
  const auto generic_schema = std::find_if(generic_admitted.begin(),
      generic_admitted.end(), [](const auto &schema) {
        return schema.revision_id() ==
               context_schema_generated::generic_values_revision;
      });
  require(generic_schema != generic_admitted.end() &&
              tracer::context_query::discover_field_paths(*generic_schema)
                  .paths.contains("value") &&
              tracer::context_query::query_field(vector_message, *generic_schema,
                                                  "value") == "[int32:3]",
          "generic container root was not queried through its admitted schema");
  generic_payload::Object generic_object{17, "payload"};
  BoundedProtoWriter object_writer(generic_storage);
  EncodeBudget object_budget;
  require(
      encode_generic_payload_root(object_writer, generic_object, object_budget),
      "generic object payload encode failed");
  auto object_bytes = object_writer.finish();
  StateAttachmentView object_attachment{8, object_bytes};
  auto object_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&object_attachment, 1));
  require(bool(object_event), "generic object payload decode failed");
  const auto &object_root = *object_event->states[0].message();
  const auto &object_message =
      reflection(object_root)
          ->GetMessage(object_root, field(object_root, "value"));
  require(reflection(object_message)
                      ->GetInt32(object_message, field(object_message, "id")) ==
                  17 &&
              reflection(object_message)
                      ->GetString(object_message,
                                  field(object_message, "label")) == "payload",
          "generic recursive object payload did not roundtrip");

  demo::RequestState request;
  request.request_id = "request-42";
  request.items.push_back({"book", 2});
  auto &billing = request.services["billing"];
  billing.calls = 3;
  billing.durations_ms = {12.5, 8.0};
  request.failure = demo::Failure{-17, "declined"};
  std::array<std::byte, 256 * 1024> shared_slot_a{}, shared_slot_b{};
  EncodeBudget event_budget;
  Limits shared_event_limits;
  event_budget.max_items = 1;
  event_budget.truncation = &Limits::record;
  event_budget.truncation_context = &shared_event_limits;
  BoundedProtoWriter slot_a_writer(shared_slot_a);
  require(encode_state_payload(slot_a_writer, request, event_budget),
          "first shared-budget state slot failed");
  const auto first_slot_items = event_budget.items,
             first_slot_visits = event_budget.visits;
  BoundedProtoWriter slot_b_writer(shared_slot_b);
  require(encode_state_payload(slot_b_writer, request, event_budget),
          "second shared-budget state slot failed");
  require(event_budget.items > first_slot_items &&
              event_budget.visits > first_slot_visits,
          "state slot codec reset event-wide capture budgets");
  Limits limits;
  auto request_bytes = encode_root(request, limits);
  require(limits.truncations > 0 && limits.saw_billing,
          "nested map truncation metadata lost its typed string key");
  const std::array<CapturePathStep, 4> duration_path{
      {{CapturePathSelector::field_number, 3},
       {CapturePathSelector::string_key, 0, 0, false, "billing"},
       {CapturePathSelector::field_number, 2},
       {CapturePathSelector::field_number, 2}}};
  const CaptureLimitView duration_limit{duration_path, 2, 1};
  StateAttachmentView request_attachment{
      1, request_bytes, std::span<const CaptureLimitView>(&duration_limit, 1)};
  auto request_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&request_attachment, 1));
  require(bool(request_event), "request event decode failed");
  const auto decoded_limits = request_event->states[0].truncations();
  require(decoded_limits.size() == 1 && decoded_limits[0].total_items == 2 &&
              decoded_limits[0].captured_items == 1 &&
              decoded_limits[0].path[1].selector ==
                  CapturePathSelector::string_key &&
              decoded_limits[0].path[1].string_value == "billing",
          "receiver did not retain typed truncation metadata");
  const auto &request_message = *request_event->states[0].message();
  const auto *request_id = field(request_message, "request_id");
  require(
      request_id &&
          reflection(request_message)->GetString(request_message, request_id) ==
              "request-42",
      "request scalar did not roundtrip");
  const auto *items = field(request_message, "items");
  require(items &&
              reflection(request_message)->FieldSize(request_message, items) ==
                  1,
          "request repeated item was not captured");
  const auto &item = reflection(request_message)
                         ->GetRepeatedMessage(request_message, items, 0);
  require(reflection(item)->GetString(item, field(item, "sku")) == "book" &&
              reflection(item)->GetUInt32(item, field(item, "quantity")) == 2,
          "nested request item did not roundtrip");
  const auto *services = field(request_message, "services");
  require(
      services &&
          reflection(request_message)->FieldSize(request_message, services) ==
              1,
      "request map entry missing");
  const auto &entry = reflection(request_message)
                          ->GetRepeatedMessage(request_message, services, 0);
  require(reflection(entry)->GetString(entry, field(entry, "key")) == "billing",
          "request map key did not roundtrip");
  const auto &service =
      reflection(entry)->GetMessage(entry, field(entry, "value"));
  require(reflection(service)->GetUInt64(service, field(service, "calls")) == 3,
          "nested map value did not roundtrip");
  const auto *durations = field(service, "durations_ms");
  require(reflection(service)->FieldSize(service, durations) == 1 &&
              reflection(service)->GetRepeatedDouble(service, durations, 0) ==
                  12.5,
          "nested vector cap was not enforced");
  const auto *failure = field(request_message, "failure");
  const auto &optional =
      reflection(request_message)->GetMessage(request_message, failure);
  const auto &failure_value =
      reflection(optional)->GetMessage(optional, field(optional, "value"));
  require(
      reflection(failure_value)
                  ->GetInt32(failure_value, field(failure_value, "code")) ==
              -17 &&
          reflection(failure_value)
                  ->GetString(failure_value, field(failure_value, "message")) ==
              "declined",
      "nested optional value did not roundtrip");

  catalog::CatalogState catalog_state;
  catalog_state.queues.push_back({"first", "omitted"});
  catalog_state.queues.push_back({"second"});
  catalog_state.ids = {4, 9};
  catalog_state.flags = {false, true};
  catalog::Nested nested;
  nested.mode = catalog::Mode::active;
  nested.fixed = {1.25, 2.5};
  catalog_state.values["tenant"][7] = nested;
  catalog_state.values["tenant"][8] = nested;
  Limits catalog_limits;
  auto catalog_bytes = encode_root(catalog_state, catalog_limits);
  require(catalog_limits.truncations >= 5 && catalog_limits.saw_unsigned_key,
          "catalog container caps or typed map keys were not reported");
  StateAttachmentView catalog_attachment{2, catalog_bytes};
  auto catalog_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&catalog_attachment, 1));
  require(bool(catalog_event), "catalog event decode failed");
  const auto &catalog_message = *catalog_event->states[0].message();
  const auto *queues = field(catalog_message, "queues");
  require(reflection(catalog_message)->FieldSize(catalog_message, queues) == 1,
          "deque cap did not select one entry");
  const auto &queue = reflection(catalog_message)
                          ->GetRepeatedMessage(catalog_message, queues, 0);
  require(reflection(queue)->FieldSize(queue, field(queue, "items")) == 1,
          "nested list cap did not select one entry");
  require(reflection(catalog_message)
                      ->FieldSize(catalog_message,
                                  field(catalog_message, "ids")) == 1 &&
              reflection(catalog_message)
                      ->FieldSize(catalog_message,
                                  field(catalog_message, "flags")) == 1,
          "set and unordered_set codecs did not roundtrip");
  const auto *values = field(catalog_message, "values");
  require(reflection(catalog_message)->FieldSize(catalog_message, values) == 1,
          "unordered_map cap did not select one entry");
  const auto &outer = reflection(catalog_message)
                          ->GetRepeatedMessage(catalog_message, values, 0);
  const auto *inner_value = field(outer, "value");
  require(reflection(outer)->FieldSize(outer, inner_value) == 1,
          "nested unordered_map cap did not select one entry");
  const auto &inner =
      reflection(outer)->GetRepeatedMessage(outer, inner_value, 0);
  const auto &optional_nested =
      reflection(inner)->GetMessage(inner, field(inner, "value"));
  const auto &nested_message =
      reflection(optional_nested)
          ->GetMessage(optional_nested, field(optional_nested, "value"));
  require(
      reflection(nested_message)
              ->GetEnumValue(nested_message, field(nested_message, "mode")) ==
          static_cast<int>(catalog::Mode::active),
      "enum did not roundtrip");
  require(reflection(nested_message)
                  ->FieldSize(nested_message, field(nested_message, "fixed")) ==
              1,
          "std::array cap did not select one value");

  history::State old_state;
  old_state.retained = 42;
  old_state.removed = "legacy";
  old_state.changed = -7;
  Limits history_limits;
  auto old_bytes = encode_root(old_state, history_limits);
  StateAttachmentView old_attachment{3, old_bytes};
  auto old_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&old_attachment, 1));
  require(bool(old_event), "old revision decode failed");
  const auto &old_message = *old_event->states[0].message();
  require(field(old_message, "removed") &&
              reflection(old_message)
                      ->GetString(old_message, field(old_message, "removed")) ==
                  "legacy",
          "old revision field was not retained");
  require(field(old_message, "changed")->type() ==
                  google::protobuf::FieldDescriptor::TYPE_SINT32 &&
              reflection(old_message)
                      ->GetInt32(old_message, field(old_message, "changed")) ==
                  -7,
          "old revision type/value changed");
  std::array<std::byte, 32> newer_storage{};
  BoundedProtoWriter newer_writer(newer_storage);
  newer_writer.u64(1, 42);
  newer_writer.f64(2, 3.25);
  newer_writer.boolean(3, true);
  auto newer_bytes = newer_writer.finish();
  StateAttachmentView newer_attachment{4, newer_bytes};
  auto newer_event = receiver.decode_event(
      std::span<const StateAttachmentView>(&newer_attachment, 1));
  require(bool(newer_event), "new revision decode failed");
  const auto &new_message = *newer_event->states[0].message();
  require(!field(new_message, "removed") && field(new_message, "added") &&
              reflection(new_message)
                  ->GetBool(new_message, field(new_message, "added")),
          "schema-absent fields were not kept absent");
  require(field(new_message, "changed")->type() ==
                  google::protobuf::FieldDescriptor::TYPE_DOUBLE &&
              reflection(new_message)
                      ->GetDouble(new_message, field(new_message, "changed")) ==
                  3.25,
          "changed field was not decoded with its original revision type");

  receiver.close_epoch();
  require(reflection(request_message)->GetString(request_message, request_id) ==
              "request-42",
          "decoded wrapper lost its schema lease after epoch close");
}

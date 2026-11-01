#include "context_runtime.hpp"

#include <any>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "context_stdlib.hpp"
#include "context_transport.pb.h"
#include "tracer/callback.hpp"

namespace {
struct RuntimeScope {};
struct GenericRuntimeScope {};
struct MultiRuntimeScope {};
struct RuntimeState : tracer::shared_state<RuntimeScope> {
  std::int64_t value = 3;
};
struct MultiStateA : tracer::shared_state<MultiRuntimeScope> {
  std::int64_t value = 17;
};
struct MultiStateB : tracer::shared_state<MultiRuntimeScope> {
  std::int64_t value = 29;
};

struct FakeOwnerInvocation {
  std::shared_ptr<const tracer::PrivateContextCode> code;
  std::shared_ptr<const tracer::ContextStdlibImage> standard_library;
};
struct FixtureMetadata {
  std::array<const tracer::RegisteredContextState *, 4> states{};
};
const void *fixture_state_metadata(
    const void *opaque, tracer::context_engine::Identity identity) noexcept {
  const auto *metadata = static_cast<const FixtureMetadata *>(opaque);
  if (metadata)
    for (const auto *state : metadata->states)
      if (state && state->identity == identity) return state;
  return nullptr;
}

template <class T>
tracer::context_engine::ErrorCode construct_multi(
    const tracer::context_engine::ScopeInit &, std::any &payload) noexcept {
  try {
    payload = std::make_shared<T>();
    return tracer::context_engine::ErrorCode::ok;
  } catch (...) {
    return tracer::context_engine::ErrorCode::construction_failed;
  }
}
template <class T> void destroy_multi(std::any &payload) noexcept {
  payload.reset();
}
template <class T> void *address_multi(std::any &payload) noexcept {
  auto *value = std::any_cast<std::shared_ptr<T>>(&payload);
  return value ? value->get() : nullptr;
}
bool encode_multi_a(tracer::context_engine::BoundedProtoWriter &writer,
                    const void *payload,
                    tracer::context_engine::EncodeBudget &) noexcept {
  const auto *state = static_cast<const MultiStateA *>(payload);
  return state && writer.s64(1, state->value);
}
bool encode_multi_b(tracer::context_engine::BoundedProtoWriter &writer,
                    const void *payload,
                    tracer::context_engine::EncodeBudget &) noexcept {
  const auto *state = static_cast<const MultiStateB *>(payload);
  return state && writer.s64(1, state->value);
}

tracer::context_engine::ErrorCode construct_generic_int(
    const tracer::context_engine::ScopeInit &init, std::any &payload) noexcept {
  try {
    if (init.payload_source && init.payload_identity == 55)
      payload = std::make_shared<int>(
          *static_cast<const int *>(init.payload_source));
    else
      payload = std::make_shared<int>(0);
    return tracer::context_engine::ErrorCode::ok;
  } catch (...) {
    return tracer::context_engine::ErrorCode::construction_failed;
  }
}
void destroy_generic_int(std::any &payload) noexcept { payload.reset(); }
void *address_generic_int(std::any &payload) noexcept {
  auto *value = std::any_cast<std::shared_ptr<int>>(&payload);
  return value ? value->get() : nullptr;
}

tracer::context_engine::ErrorCode construct_state(
    const tracer::context_engine::ScopeInit &init, std::any &payload) noexcept {
  if (!init.root_call)
    return tracer::context_engine::ErrorCode::construction_failed;
  try {
    payload = std::make_shared<RuntimeState>();
    return tracer::context_engine::ErrorCode::ok;
  } catch (...) {
    return tracer::context_engine::ErrorCode::construction_failed;
  }
}
void destroy_state(std::any &payload) noexcept { payload.reset(); }
void *address_state(std::any &payload) noexcept {
  auto *state = std::any_cast<std::shared_ptr<RuntimeState>>(&payload);
  return state ? state->get() : nullptr;
}
bool encode_state(tracer::context_engine::BoundedProtoWriter &writer,
                  const void *payload,
                  tracer::context_engine::EncodeBudget &) noexcept {
  const auto *state = static_cast<const RuntimeState *>(payload);
  return state && writer.s64(1, state->value);
}

struct Sink {
  std::size_t count = 0;
  std::vector<std::byte> last;
  std::shared_ptr<const void> schemas;
};
bool publish(void *opaque, std::span<const std::byte> bytes,
             const std::shared_ptr<const void> &schemas) noexcept {
  auto &sink = *static_cast<Sink *>(opaque);
  try {
    sink.last.assign(bytes.begin(), bytes.end());
    sink.schemas = schemas;
    ++sink.count;
    return true;
  } catch (...) {
    return false;
  }
}

std::string id(char fill) { return std::string(16, fill); }

std::shared_ptr<const tracer::RegisteredContextScope> registered_fixture(
    bool alternate_schema = false) {
  using namespace tracer;
  using namespace tracer::context_engine;
  std::array<std::uint8_t, 16> lineage{};
  lineage[0] = 4;
  StateFamilyKey family{lineage, "RuntimeState", "RuntimeScope"};
  auto root = std::make_shared<SchemaMessage>();
  root->cpp_type = "RuntimeState";
  root->full_name = alternate_schema ? "test.OtherRuntimeState"
                                     : "test.RuntimeState";
  root->fields.push_back(
      SchemaField{"value", "long", {}, {}, 1, SchemaKind::signed_integer,
                  64, false, false, {}, {}, {}, 0});
  auto schema = std::make_shared<const SchemaRecord>(
      family,
      alternate_schema ? "OtherRuntimeStateRoot" : "RuntimeStateRoot",
      "runtime-rev-1", "runtime-descriptor",
      root);
  auto registration = std::make_shared<ScopeRegistration>();
  registration->identity = 101;
  auto metadata = std::make_shared<FixtureMetadata>();
  registration->owner_code = metadata;
  registration->state_metadata_context = metadata.get();
  registration->state_metadata = &fixture_state_metadata;
  registration->states.push_back(
      {201, Policy::shared, &construct_state, &destroy_state, &address_state});
  static const FakeOwnerInvocation owner_invocation{};
  std::vector<RegisteredContextState> states;
  states.emplace_back(
      201, 101,
      ContextDependency{"RuntimeState", "RuntimeScope", true, {}, false, {}},
      1, "", schema, &encode_state, &owner_invocation);
  metadata->states[0] = &states.back();
  return std::make_shared<const RegisteredContextScope>(
      lineage, "RuntimeScope", nullptr, registration, std::move(states),
      ContextCompilationContract{});
}

std::shared_ptr<const tracer::RegisteredContextScope>
registered_generic_fixture() {
  using namespace tracer;
  using namespace tracer::context_engine;
  std::array<std::uint8_t, 16> lineage{};
  lineage[0] = 9;
  auto registration = std::make_shared<ScopeRegistration>();
  registration->identity = 102;
  auto metadata = std::make_shared<FixtureMetadata>();
  registration->owner_code = metadata;
  registration->state_metadata_context = metadata.get();
  registration->state_metadata = &fixture_state_metadata;
  registration->states.push_back(
      {202, Policy::confined, &construct_generic_int, &destroy_generic_int,
       &address_generic_int});
  static const FakeOwnerInvocation owner_invocation{};
  std::vector<RegisteredContextState> states;
  states.emplace_back(
      202, 102,
      ContextDependency{"", "GenericRuntimeScope", false, {}, true, "int"},
      55, "", nullptr, nullptr, &owner_invocation);
  metadata->states[0] = &states.back();
  return std::make_shared<const RegisteredContextScope>(
      lineage, "GenericRuntimeScope", nullptr, registration, std::move(states),
      ContextCompilationContract{});
}

std::shared_ptr<const tracer::RegisteredContextScope>
registered_multi_fixture() {
  using namespace tracer;
  using namespace tracer::context_engine;
  std::array<std::uint8_t, 16> lineage{};
  lineage[0] = 13;
  const auto make_schema = [&](const char *state_type, const char *root_name) {
    StateFamilyKey family{lineage, state_type, "MultiRuntimeScope"};
    auto root = std::make_shared<SchemaMessage>();
    root->cpp_type = state_type;
    root->full_name = root_name;
    root->fields.push_back(
        SchemaField{"value", "long", {}, {}, 1, SchemaKind::signed_integer,
                    64, false, false, {}, {}, {}, 0});
    return std::make_shared<const SchemaRecord>(
        family, root_name, std::string(root_name) + "-revision",
        std::string(root_name) + "-descriptor", root);
  };
  const auto schema_a = make_schema("MultiStateA", "test.MultiStateARoot");
  const auto schema_b = make_schema("MultiStateB", "test.MultiStateBRoot");
  auto registration = std::make_shared<ScopeRegistration>();
  registration->identity = 103;
  auto metadata = std::make_shared<FixtureMetadata>();
  registration->owner_code = metadata;
  registration->state_metadata_context = metadata.get();
  registration->state_metadata = &fixture_state_metadata;
  registration->states.push_back(
      {301, Policy::shared, &construct_multi<MultiStateA>,
       &destroy_multi<MultiStateA>, &address_multi<MultiStateA>});
  registration->states.push_back(
      {302, Policy::shared, &construct_multi<MultiStateB>,
       &destroy_multi<MultiStateB>, &address_multi<MultiStateB>});
  static const FakeOwnerInvocation owner_invocation{};
  std::vector<RegisteredContextState> states;
  states.emplace_back(
      301, 103,
      ContextDependency{"MultiStateA", "MultiRuntimeScope", true, {}, false,
                        {}},
      1, "", schema_a, &encode_multi_a, &owner_invocation);
  states.emplace_back(
      302, 103,
      ContextDependency{"MultiStateB", "MultiRuntimeScope", true, {}, false,
                        {}},
      2, "", schema_b, &encode_multi_b, &owner_invocation);
  metadata->states[0] = &states[0];
  metadata->states[1] = &states[1];
  return std::make_shared<const RegisteredContextScope>(
      lineage, "MultiRuntimeScope", nullptr, registration, std::move(states),
      ContextCompilationContract{});
}
} // namespace

namespace tracer::context_facade {
template <> struct StateTraits<::RuntimeState> {
  static constexpr RequestDescriptor descriptor{
      201, 101, "RuntimeState", "RuntimeScope", "", false, true};
};
template <> struct ScopeTraits<::RuntimeScope> {
  static constexpr ScopeId id = 101;
};
template <> struct PayloadTraits<::GenericRuntimeScope, int> {
  static constexpr RequestDescriptor descriptor{
      202, 102, "", "GenericRuntimeScope", "int", true, false};
};
template <> struct ScopeTraits<::GenericRuntimeScope> {
  static constexpr ScopeId id = 102;
};
template <> struct StateTraits<::MultiStateA> {
  static constexpr RequestDescriptor descriptor{
      301, 103, "MultiStateA", "MultiRuntimeScope", "", false, true};
};
template <> struct StateTraits<::MultiStateB> {
  static constexpr RequestDescriptor descriptor{
      302, 103, "MultiStateB", "MultiRuntimeScope", "", false, true};
};
} // namespace tracer::context_facade

namespace tracer {
template <class T> T &Context::get() const {
  return *context_facade::request<T>();
}
template <class Scope, class Payload> Payload &Context::get() const {
  return *context_facade::request_payload<Scope, Payload>();
}
template <class T, class Visitor> void Context::with(Visitor &&visitor) const {
  context_facade::with_state<T>(std::forward<Visitor>(visitor));
}
template <class Scope, class Payload, class Visitor>
void Context::with(Visitor &&visitor) const {
  context_facade::with_payload<Scope, Payload>(std::forward<Visitor>(visitor));
}
template <class Scope> void Context::init_scope() const {
  context_facade::init_scope<Scope>();
}
template <class Scope, class Payload>
void Context::init_scope(Payload &&payload) const {
  context_facade::init_scope<Scope>(std::forward<Payload>(payload));
}
template <class Scope> void Context::end_scope() const {
  context_facade::end_scope<Scope>();
}
} // namespace tracer

void verify_context_runtime() {
  using namespace tracer;
  using namespace tracer::context_engine;
  using namespace tracer::context_runtime;
  ContextCapturePool pool;
  Sink sink;
  EventMetadata metadata_a{id('a'), id('t'), {}, id('i'), id('p'), "", 7, 1, 1};
  EventMetadata metadata_b{id('a'), id('t'), {}, id('i'), id('q'), "", 7, 1, 2};
  metadata_a.print_arguments = true;
  auto scope = registered_fixture();
  SharedSourceRuntime first({scope}, pool, metadata_a, &publish, &sink);
  SharedSourceRuntime second({scope}, pool, metadata_b, &publish, &sink);
  TraceContext trace{};
  trace.call_id = 0x1020304050607080ULL;
  trace.phase = TracePhase::Pre;
  trace.probe_id = 7;
  trace.argument_count = 1;
  trace.argument_names[0] = "delta";
  trace.arguments[0] = {ValueType::Int64, 0, 5, 0, 0};
  trace.named_count = 1;
  trace.named_values[0].kind = 2;
  std::memcpy(trace.named_values[0].name, "field", 6);
  trace.named_values[0].value = {ValueType::Int64, 0, 8, 0, 0};
  Call call(trace);

  // Distinct states in one scope may have distinct schemas. Capture-local IDs
  // must remain unique and each encoded state must reference its own schema.
  Sink multi_sink;
  EventMetadata multi_metadata{id('m'), id('t'), {}, id('i'), id('m'), "",
                               1, 20, 8};
  auto multi_scope = registered_multi_fixture();
  SharedSourceRuntime multi_runtime({multi_scope}, pool, multi_metadata,
                                    &publish, &multi_sink);
  TraceContext multi_trace{};
  multi_trace.call_id = 0x2030405060708090ULL;
  multi_trace.phase = TracePhase::Pre;
  multi_trace.probe_id = 8;
  Call multi_call(multi_trace);
  {
    SharedSourceRuntime::Invocation invocation(multi_runtime, &multi_call);
    context_facade::InvocationFrame frame(context_facade::current_context());
    frame.push_create_or_attach<MultiStateA>();
    frame.push_create_or_attach<MultiStateB>();
    context_facade::run_direct_hook<MultiStateA, MultiStateB>(
        multi_call, true, [&] { multi_call.context().event("two states", 2); });
    frame.pop_all_and_restore();
  }
  clangtracer::context::v1::EventRecord multi_batch;
  if (!multi_batch.ParseFromArray(multi_sink.last.data(),
                                  static_cast<int>(multi_sink.last.size())) ||
      multi_batch.states_size() != 2)
    throw std::runtime_error(
        "distinct same-scope schemas did not receive unique capture IDs");
  const auto *multi_schemas = capture_schema_set(multi_sink.schemas);
  if (!multi_schemas || multi_schemas->size() != 2 ||
      multi_schemas->epoch_schema_id(0) == multi_schemas->epoch_schema_id(1))
    throw std::runtime_error("capture did not pin two distinct schemas");
  bool saw_a = false;
  bool saw_b = false;
  for (const auto &state : multi_batch.states()) {
    for (std::size_t schema_index = 0; schema_index < multi_schemas->size();
         ++schema_index) {
      const auto *schema = multi_schemas->record(schema_index);
      if (!schema || state.schema_identity() != schema->revision_id ||
          state.epoch_schema_id() !=
              multi_schemas->epoch_schema_id(schema_index))
        continue;
      const auto payload = state.payload();
      if (schema->family.cpp_state_type == "MultiStateA")
        saw_a = payload == std::string("\x08\x22", 2);
      if (schema->family.cpp_state_type == "MultiStateB")
        saw_b = payload == std::string("\x08\x3a", 2);
    }
  }
  if (!saw_a || !saw_b)
    throw std::runtime_error(
        "same-scope state payloads did not map to their distinct schemas");

  // Generic init_scope passes a stable descriptor ID through the facade. The
  // owner factory expects its frozen owner-local initializer ordinal instead.
  auto generic_scope = registered_generic_fixture();
  SharedSourceRuntime generic_runtime({generic_scope}, pool, metadata_b,
                                      &publish, &sink);
  {
    SharedSourceRuntime::Invocation invocation(generic_runtime, &call);
    if (!invocation.entered())
      throw std::runtime_error("generic payload runtime did not enter");
    context_facade::InvocationFrame generic_frame(
        context_facade::current_context());
    context_facade::run_short_hook(call, false, [&] {
      call.context().init_scope<GenericRuntimeScope>(42);
    });
    bool got_initialized_payload = false;
    context_facade::run_direct_hook<
        context_facade::PayloadAccess<GenericRuntimeScope, int>>(
        call, false, [&] {
          got_initialized_payload =
              call.context().get<GenericRuntimeScope, int>() == 42;
        });
    if (!got_initialized_payload)
      throw std::runtime_error(
          "generic init_scope did not translate to owner initializer identity");
    context_facade::run_short_hook(call, false, [&] {
      call.context().end_scope<GenericRuntimeScope>();
    });
    generic_frame.pop_all_and_restore();
  }
  bool first_access = false;
  bool nested_access = false;
  bool restored_access = false;

  {
    SharedSourceRuntime::Invocation outer(first, &call);
    if (!outer.entered() || !active_context())
      throw std::runtime_error("source runtime did not install a native context");
    context_facade::InvocationFrame frame(context_facade::current_context());
    frame.push_create_or_attach<RuntimeState>();
    context_facade::run_direct_hook<RuntimeState>(call, true, [&] {
      auto &state = call.context().get<RuntimeState>();
      state.value += 10;
      first_access = state.value == 13;
      call.context().event("outer", 4);
      call.context().event("oversized", 4097);
    });

    {
      context_execution::OriginalDispatch application;
      if (!application.entered())
        throw std::runtime_error("original target dispatch retained access guards");
      SharedSourceRuntime::Invocation inner(second, &call);
      if (!inner.entered() || active_context() == nullptr)
        throw std::runtime_error("nested source generation lost the shared context");
      context_facade::InvocationFrame nested(context_facade::current_context());
      nested.push_create_or_attach<RuntimeState>();
      context_facade::run_direct_hook<RuntimeState>(call, false, [&] {
        auto &state = call.context().get<RuntimeState>();
        ++state.value;
        nested_access = state.value == 14;
      });
    }

    context_facade::run_direct_hook<RuntimeState>(call, false, [&] {
      restored_access = call.context().get<RuntimeState>().value == 14;
    });

    context_facade::run_short_hook(call, true, [&] {
      call.context().event("short snapshot", 4);
    });
    context_facade::run_short_hook(call, true, [&] {
      context_facade::with_state<RuntimeState>([](RuntimeState &) {});
      call.context().event("after visitor", 4);
    });
    context_facade::run_short_hook(call, true, [&] {
      context_facade::with_state<RuntimeState>([&](RuntimeState &) {
        call.context().event("nested visitor event", 4);
      });
    });
    frame.pop_all_and_restore();
  }

  if (!first_access || !nested_access || !restored_access)
    throw std::runtime_error("nested source generations did not share and restore state");
  if (sink.count != 3 || sink.last.empty())
    throw std::runtime_error("short snapshot intervals did not publish complete batches");
  clangtracer::context::v1::EventRecord batch;
  if (!batch.ParseFromArray(sink.last.data(), static_cast<int>(sink.last.size())))
    throw std::runtime_error("captured context event was not a valid EventRecord");
  const auto &event = batch;
  if (event.name() != "after visitor" || event.arguments_size() != 1 ||
      event.arguments(0).name() != "delta" ||
      event.arguments(0).value().value_type() !=
          static_cast<std::uint32_t>(ValueType::Int64) ||
      event.arguments(0).value().value_case() !=
          clangtracer::context::v1::TypedTraceValue::kSignedValue ||
      event.arguments(0).value().signed_value() != 5 ||
      event.named_values_size() != 1 ||
      event.named_values(0).name() != "field" ||
      event.named_values(0).value().signed_value() != 8 ||
      event.has_result() ||
      event.phase() != static_cast<std::uint32_t>(TracePhase::Pre) ||
      event.states_size() != 1 || event.source_id().size() != 16 ||
      event.task_id().size() != 16 || event.source_sequence() == 0 ||
      event.invocation_id().size() != 16 || event.probe_id().size() != 16 ||
      first.capture_drops() != 2)
    throw std::runtime_error(
        "TraceContext values/states were not preserved in event capture: name=" +
        event.name() + ", args=" + std::to_string(event.arguments_size()) +
        ", named=" + std::to_string(event.named_values_size()) +
        ", phase=" + std::to_string(event.phase()) +
        ", states=" + std::to_string(event.states_size()) +
        ", source/task/invocation/probe=" +
        std::to_string(event.source_id().size()) + "/" +
        std::to_string(event.task_id().size()) + "/" +
        std::to_string(event.invocation_id().size()) + "/" +
        std::to_string(event.probe_id().size()) +
        ", drops=" + std::to_string(first.capture_drops()));

  Sink failure_sink;
  EventMetadata failure_metadata{id('f'), id('t'), {}, id('i'), id('x'), "", 7, 1, 5};
  SharedSourceRuntime failure_runtime({scope}, pool, failure_metadata, &publish,
                                      &failure_sink);
  bool pre_failed = false;
  bool nested_original_state = false;
  unsigned original_runs = 0;
  unsigned post_runs = 0;
  {
    SharedSourceRuntime::Invocation invocation(failure_runtime, &call);
    context_facade::InvocationFrame frame(context_facade::current_context());
    frame.push_create_or_attach<RuntimeState>();
    try {
      context_facade::run_direct_hook<RuntimeState>(call, true, [&] {
        auto &state = call.context().get<RuntimeState>();
        state.value = 55;
        call.context().event("retained before failure", 4);
        throw std::runtime_error("PRE failure");
      });
    } catch (const context_facade::HookFailure &) {
      pre_failed = true;
    }
    {
      context_execution::OriginalDispatch application;
      ++original_runs;
      SharedSourceRuntime::Invocation nested(second, &call);
      context_facade::InvocationFrame nested_frame(
          context_facade::current_context());
      nested_frame.push_create_or_attach<RuntimeState>();
      context_facade::run_direct_hook<RuntimeState>(call, false, [&] {
        nested_original_state =
            call.context().get<RuntimeState>().value == 55;
      });
      nested_frame.pop_all_and_restore();
    }
    if (!pre_failed) ++post_runs;
    frame.pop_all_and_restore();
  }
  clangtracer::context::v1::EventRecord failure_batch;
  if (!pre_failed || original_runs != 1 || post_runs != 0 ||
      !nested_original_state || failure_sink.count != 1 ||
      !failure_batch.ParseFromArray(failure_sink.last.data(),
                                    static_cast<int>(failure_sink.last.size())) ||
      failure_batch.name() != "retained before failure")
    throw std::runtime_error(
        "PRE failure did not retain its event, skip POST, and preserve bindings "
        "through one original dispatch");

  Sink foreign_sink;
  EventMetadata foreign_metadata{id('g'), id('t'), {}, id('i'), id('y'), "", 7, 1, 6};
  SharedSourceRuntime foreign_runtime({registered_fixture(true)}, pool,
                                      foreign_metadata, &publish, &foreign_sink);
  {
    SharedSourceRuntime::Invocation outer(first, &call);
    context_facade::InvocationFrame frame(context_facade::current_context());
    frame.push_create_or_attach<RuntimeState>();
    {
      context_execution::OriginalDispatch application;
      SharedSourceRuntime::Invocation nested(foreign_runtime, &call);
      context_facade::run_short_hook(call, true, [&] {
        call.context().event("foreign registration snapshot", 1);
      });
    }
    frame.pop_all_and_restore();
  }
  clangtracer::context::v1::EventRecord foreign_batch;
  const auto *foreign_schemas = capture_schema_set(foreign_sink.schemas);
  if (!foreign_batch.ParseFromArray(foreign_sink.last.data(),
                                    static_cast<int>(foreign_sink.last.size())) ||
      !foreign_schemas || foreign_schemas->size() != 1 ||
      !foreign_schemas->record(0) ||
      foreign_schemas->record(0)->root_type !=
          scope->states.front().schema->root_type ||
      foreign_batch.arguments_size() != 0 ||
      foreign_batch.named_values_size() != 0 ||
      foreign_batch.has_result())
    throw std::runtime_error(
        "visible state capture used another generation's schema metadata: count=" +
        std::to_string(foreign_sink.count) + " bytes=" +
        std::to_string(foreign_sink.last.size()) + " schemas=" +
        std::to_string(foreign_schemas ? foreign_schemas->size() : 0) + " root=" +
        (foreign_schemas && foreign_schemas->size() && foreign_schemas->record(0)
             ? foreign_schemas->record(0)->root_type
                                      : std::string("<none>")));

  if (active_context() != nullptr)
    throw std::runtime_error("top-level runtime did not restore native context TLS");

  Sink concurrent_a, concurrent_b;
  EventMetadata metadata_c{id('c'), id('t'), {}, id('i'), id('r'), "", 7, 1, 3};
  EventMetadata metadata_d{id('d'), id('t'), {}, id('i'), id('s'), "", 7, 1, 4};
  metadata_c.print_arguments = true;
  metadata_d.print_result = true;
  SharedSourceRuntime third({scope}, pool, metadata_c, &publish, &concurrent_a);
  SharedSourceRuntime fourth({scope}, pool, metadata_d, &publish, &concurrent_b);
  const auto publish_first_event = [&](SharedSourceRuntime &runtime) {
    SharedSourceRuntime::Invocation invocation(runtime, &call);
    if (!invocation.entered()) return;
    context_facade::run_short_hook(call, true, [&] {
      call.context().event("unique id", 1);
    });
  };
  std::thread thread_a(publish_first_event, std::ref(third));
  std::thread thread_b(publish_first_event, std::ref(fourth));
  thread_a.join();
  thread_b.join();
  clangtracer::context::v1::EventRecord concurrent_batch_a,
      concurrent_batch_b;
  if (!concurrent_batch_a.ParseFromArray(concurrent_a.last.data(),
                                         static_cast<int>(concurrent_a.last.size())) ||
      !concurrent_batch_b.ParseFromArray(concurrent_b.last.data(),
                                         static_cast<int>(concurrent_b.last.size())) ||
      concurrent_batch_a.event_id().size() != 16 ||
      concurrent_batch_b.event_id().size() != 16 ||
      !concurrent_batch_a.print_arguments() ||
      concurrent_batch_a.print_result() ||
      concurrent_batch_b.print_arguments() ||
      !concurrent_batch_b.print_result() ||
      concurrent_batch_a.event_id() == concurrent_batch_b.event_id())
    throw std::runtime_error(
        "first events from concurrent source runtimes reused an event ID");

  const auto before = sink.count;
  {
    LifecycleBoundary lifecycle;
    if (!lifecycle.entered())
      throw std::runtime_error("lifecycle no-trace scope was not admitted");
    context_facade::current_context().event("lifecycle-event", 1);
  }
  if (sink.count != before)
    throw std::runtime_error("lifecycle event escaped without an active hook");

  auto &native_diagnostics = context_execution::runtime_diagnostics();
  const auto no_plan_before = native_diagnostics.count(
      context_execution::Reason::access_failure,
      context_execution::Phase::access);
  bool no_plan_status = false;
  {
    SharedSourceRuntime::Invocation invocation(first, &call);
    context_facade::run_short_hook(call, [&] {
      try {
        (void)call.context().get<RuntimeState>();
      } catch (const context_facade::ContextError &error) {
        no_plan_status = error.code ==
            static_cast<unsigned>(ErrorCode::unplanned_access);
      }
    });
  }
  const auto no_plan_after = native_diagnostics.count(
      context_execution::Reason::access_failure,
      context_execution::Phase::access);
  if (!no_plan_status || no_plan_after != no_plan_before + 1)
    throw std::runtime_error(
        "caught no-plan get did not retain its bounded native diagnostic");

  const auto invalid_status_before = native_diagnostics.count(
      context_execution::Reason::incompatible_state,
      context_execution::Phase::access);
  const auto borrowed_end_before = native_diagnostics.count(
      context_execution::Reason::invalid_ownership,
      context_execution::Phase::access);
  bool access_rejected = false, borrowed_end_rejected = false;
  {
    SharedSourceRuntime::Invocation invocation(first, &call);
    context_facade::InvocationFrame owner(context_facade::current_context());
    owner.push_create_or_attach<RuntimeState>();
    {
      context_facade::InvocationFrame borrowed(context_facade::current_context());
      borrowed.push_create_or_attach<RuntimeState>();
      context_facade::run_direct_hook<RuntimeState>(call, false, [&] {
        context_facade::Status status = 0;
        const context_facade::RequestDescriptor invalid_request{
            999, 101, "Missing", "RuntimeScope", "", false, true};
        const auto token = first.function_table().access_begin(
            &first, &invalid_request, 1,
            context_facade::AccessKind::independent, &status);
        access_rejected = !token && status == static_cast<unsigned>(
            ErrorCode::incompatible_registration);
        try {
          context_facade::end_scope<RuntimeScope>();
        } catch (const context_facade::ContextError &error) {
          borrowed_end_rejected = error.code == static_cast<unsigned>(
              ErrorCode::borrowed_end);
        }
      });
    }
    owner.pop_all_and_restore();
  }
  if (!access_rejected || !borrowed_end_rejected ||
      native_diagnostics.count(context_execution::Reason::incompatible_state,
                               context_execution::Phase::access) !=
          invalid_status_before + 1 ||
      native_diagnostics.count(context_execution::Reason::invalid_ownership,
                               context_execution::Phase::access) !=
          borrowed_end_before + 1)
    throw std::runtime_error(
        "caught access/end status did not retain bounded native diagnostics");

  Sink workspace_sink;
  EventMetadata workspace_metadata{id('w'), id('t'), {}, id('i'), id('w'), "",
                                   1, 1, 11};
  SharedSourceRuntime workspace_runtime({scope}, pool, workspace_metadata,
                                        &publish, &workspace_sink);
  TraceContext workspace_trace{};
  workspace_trace.call_id = 0x30405060708090a0ULL;
  workspace_trace.phase = TracePhase::Pre;
  workspace_trace.probe_id = 11;
  Call workspace_call(workspace_trace);
  const auto workspace_baseline = workspace_slots_in_use();
  if (workspace_baseline != 0)
    throw std::runtime_error("runtime workspace leaked before thread reuse test: " +
                             std::to_string(workspace_baseline));
  for (unsigned index = 0; index < 96; ++index) {
    std::atomic<bool> entered{false};
    std::atomic<unsigned> failure{0};
    std::thread worker_thread([&] {
      try {
        // A scheduler may ask for the stable worker Context before entering
        // the source Invocation. The thread-exit anchor must release that
        // pinned workspace after the Invocation has reused it.
        (void)worker_context();
        SharedSourceRuntime::Invocation invocation(workspace_runtime,
                                                   &workspace_call);
        entered.store(invocation.entered(), std::memory_order_release);
      } catch (const context_engine::Error &error) {
        failure.store(static_cast<unsigned>(error.code()) + 1,
                      std::memory_order_release);
      } catch (...) {
        failure.store(std::numeric_limits<unsigned>::max(),
                      std::memory_order_release);
      }
    });
    worker_thread.join();
    if (!entered.load(std::memory_order_acquire) ||
        workspace_slots_in_use() != workspace_baseline)
      throw std::runtime_error(
          "bounded runtime workspace was not released for sequential threads: " +
          std::to_string(workspace_slots_in_use()) + " baseline=" +
          std::to_string(workspace_baseline) + " entered=" +
          std::to_string(entered.load()) + " failure=" +
          std::to_string(failure.load()));
  }
}

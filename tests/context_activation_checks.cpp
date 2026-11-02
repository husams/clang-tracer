#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/descriptor.pb.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

#include "context_activation.hpp"
#include "context_receiver.hpp"
#include "context_runtime.hpp"
#include "context_stdlib.hpp"
#include "context_test_config.hpp"
#include "context_transport.pb.h"
#include "tracer/compat.hpp"

namespace {
void require(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}

#if defined(__linux__) && defined(__x86_64__)
struct CaptureSink {
  std::vector<clangtracer::context::v1::ContextBatch> batches;
};

bool capture_batch(void* opaque, ::tracer::compat::span<const std::byte> bytes,
                   const std::shared_ptr<const void>& schema_token) noexcept {
  struct ReleaseSchemaLease {
    const std::shared_ptr<const void> &token;
    ~ReleaseSchemaLease() {
      tracer::context_engine::release_capture_schema_set(token);
    }
  } release{schema_token};
  auto &sink = *static_cast<CaptureSink *>(opaque);
  const auto *schema_set =
      tracer::context_engine::capture_schema_set(schema_token);
  if (!schema_set)
    return false;
  try {
    clangtracer::context::v1::ContextBatch batch;
    for (std::size_t i = 0; i < schema_set->size(); ++i) {
      const auto *record = schema_set->record(i);
      if (!record)
        return false;
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
      schema->set_epoch_schema_id(schema_set->epoch_schema_id(i));
    }
    if (!batch.mutable_event()->ParseFromArray(bytes.data(),
                                               static_cast<int>(bytes.size())))
      return false;
    sink.batches.push_back(std::move(batch));
    return true;
  } catch (...) {
    return false;
  }
}

using Entry = bool (*)(void *, const tracer::TraceContext *);
using Destroy = void (*)(void *);
struct PrivateCall {
  Entry entry = nullptr;
  Destroy destroy = nullptr;
  void *storage = nullptr;
  const tracer::TraceContext *trace = nullptr;
  bool result = false;
};
void invoke_private(void *opaque) {
  auto &call = *static_cast<PrivateCall *>(opaque);
  if (call.entry)
    call.result = call.entry(call.storage, call.trace);
  else
    call.destroy(call.storage);
}
bool guarded_entry(const tracer::ContextActivationGeneration &generation,
                   Entry entry, void *storage,
                   const tracer::TraceContext *trace) {
  const auto &code = generation.callback_code();
  const auto &library = code->standard_library();
  tracer::ContextStdlibImage::Frame frame(
      library, code.get(), &tracer::PrivateContextCode::unwind_lookup);
  PrivateCall call{entry, nullptr, storage, trace};
  return frame.entered() && library->invoke(&invoke_private, &call) &&
         call.result;
}
bool guarded_destroy(const tracer::ContextActivationGeneration &generation,
                     Destroy destroy, void *storage) {
  const auto &code = generation.callback_code();
  const auto &library = code->standard_library();
  tracer::ContextStdlibImage::Frame frame(
      library, code.get(), &tracer::PrivateContextCode::unwind_lookup);
  PrivateCall call{nullptr, destroy, storage, nullptr};
  return frame.entered() && library->invoke(&invoke_private, &call);
}
#endif
} // namespace

void verify_context_activation() {
  // A different host GCC installation can expose headers absent from the
  // selected native library (for example, GCC 11 <span> beside GCC 8 headers).
  // Model that automatic C++ search path without modifying the host or SDK.
  struct ForeignHeaders {
    llvm::SmallString<256> path;
    ForeignHeaders() {
      require(!llvm::sys::fs::createUniqueDirectory("tracer-foreign-cxx", path),
              "cannot create foreign C++ header fixture");
      std::ofstream file(std::string(path) + "/tracer_foreign_stdlib_probe.hpp");
      file << "#error foreign standard-library header was selected\n";
      require(static_cast<bool>(file), "cannot write foreign C++ header fixture");
    }
    ~ForeignHeaders() { (void)llvm::sys::fs::remove_directories(path); }
  } foreign;
  tracer::ContextActivationRequest request;
  request.source_lineage_id[0] = 0x76;
  std::ifstream fixture(TRACER_TEST_SOURCE_DIR
                        "/tests/fixtures/shared_context_nested.cpp");
  request.source.assign(std::istreambuf_iterator<char>(fixture), {});
  require(!request.source.empty(), "activation fixture could not be read");
  request.file_name = "shared-context-activation.cc";
  request.compiler_arguments = {"-std=c++20",
                                "-fPIC",
                                "-O0",
                                "-fno-stack-protector",
                                "-I" TRACER_TEST_SOURCE_DIR "/include",
                                "-I" TRACER_TEST_SOURCE_DIR "/src",
                                "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  request.compiler_arguments.insert(request.compiler_arguments.end(),
                                    std::begin(kContextSystemArguments),
                                    std::end(kContextSystemArguments));
  // Production activation must enforce its own native-library isolation even
  // when the caller does not already disable automatic C++ include discovery.
  auto& args = request.compiler_arguments;
  args.erase(std::remove(args.begin(), args.end(), "-nostdinc++"), args.end());
  args.insert(args.end(), {"-stdlib++-isystem", std::string(foreign.path)});
  request.source += "\n#if __has_include(<tracer_foreign_stdlib_probe.hpp>)\n"
                    "#error activation exposed a foreign C++ library search path\n"
                    "#endif\n";
  tracer::ContextRegistry registry;
  const auto generation = tracer::activate_context_source(request, registry);
#if defined(__linux__) && defined(__x86_64__)
  require(static_cast<bool>(generation),
          "native source activation failed: " + generation.error);
  require(generation.generation->interceptor_class() ==
                  "demo::RequestInterceptor" &&
              generation.generation->scopes().size() == 1 &&
              generation.generation->scopes()[0]->states.size() == 1 &&
              generation.generation->object_bytes() <= 4096 &&
              generation.generation->object_alignment() <= 64,
          "activation did not freeze one complete bounded scope generation");

  tracer::ContextActivationRequest packed_request = request;
  packed_request.file_name = "packed-context-activation.cc";
  packed_request.source += "\n#pragma pack(push, 1)\n";
  tracer::ContextRegistry packed_registry;
  const auto packed =
      tracer::activate_context_source(packed_request, packed_registry);
  require(!packed,
          "packed private engine headers were admitted as native owner ABI");
  require(packed.error.find("native owner bridge") != std::string::npos &&
              packed_registry.scope_count() == 0,
          "packed private headers failed outside ABI admission: " +
              packed.error);

  CaptureSink sink;
  tracer::context_engine::ContextCapturePool pool;
  tracer::context_runtime::EventMetadata metadata;
  metadata.source_id.assign(16, 's');
  metadata.task_id.assign(16, 't');
  metadata.invocation_id.assign(16, 'i');
  metadata.probe_id.assign(16, 'p');
  metadata.capture_hook_id = 1;
  tracer::context_runtime::SharedSourceRuntime runtime(
      generation.generation->scopes(), pool, metadata, &capture_batch, &sink);
  tracer::TraceContext trace{};
  trace.call_id = 42;
  tracer::Call call(trace);
  const auto construct =
      reinterpret_cast<Entry>(generation.generation->construct_entry());
  const auto pre = reinterpret_cast<Entry>(generation.generation->pre_entry());
  const auto post =
      reinterpret_cast<Entry>(generation.generation->post_entry());
  const auto destroy =
      reinterpret_cast<Destroy>(generation.generation->destroy_entry());
  alignas(64) std::array<std::byte, 4096> object{};
  {
    tracer::context_runtime::SharedSourceRuntime::Invocation invocation(runtime,
                                                                        &call);
    require(invocation.entered() &&
                guarded_entry(*generation.generation, construct, object.data(),
                              &trace),
            "generated owner constructor did not enter its scope frame");
    require(guarded_entry(*generation.generation, pre, object.data(), &trace),
            "generated PRE entrypoint failed on the private callback module");
    tracer::ContextActivationRequest context_only_request = request;
    context_only_request.source_lineage_id[0] = 0x78;
    context_only_request.file_name = "context-only-activation.cc";
    context_only_request.source = R"CPP(
namespace emitter {
struct EventInterceptor {
  void pre(const Call& call) { call.context().event("context.only"); }
};
}
)CPP";
    tracer::ContextRegistry context_only_registry;
    const auto context_only = tracer::activate_context_source(
        context_only_request, context_only_registry);
    require(context_only && context_only.generation->scopes().empty(),
            "context-only emitter invented a state family: " +
                context_only.error);
    tracer::context_runtime::EventMetadata emitter_metadata = metadata;
    emitter_metadata.source_id.assign(16, 'e');
    emitter_metadata.capture_hook_id = 2;
    tracer::context_runtime::SharedSourceRuntime emitter_runtime(
        context_only.generation->scopes(), pool, emitter_metadata,
        &capture_batch, &sink);
    const auto emitter_construct =
        reinterpret_cast<Entry>(context_only.generation->construct_entry());
    const auto emitter_pre =
        reinterpret_cast<Entry>(context_only.generation->pre_entry());
    const auto emitter_destroy =
        reinterpret_cast<Destroy>(context_only.generation->destroy_entry());
    alignas(64) std::array<std::byte, 4096> emitter_object{};
    {
      tracer::context_execution::OriginalDispatch original_dispatch;
      require(original_dispatch.entered(),
              "nested source invocation did not enter original dispatch");
      tracer::context_runtime::SharedSourceRuntime::Invocation
          emitter_invocation(emitter_runtime, &call);
      require(emitter_invocation.entered() &&
                  guarded_entry(*context_only.generation, emitter_construct,
                                emitter_object.data(), &trace) &&
                  guarded_entry(*context_only.generation, emitter_pre,
                                emitter_object.data(), &trace),
              "context-only event emitter failed to run");
      require(sink.batches.size() == 1 &&
                  sink.batches.back().event().name() == "context.only" &&
                  sink.batches.back().event().states_size() == 1,
              "context-only emitter could not capture a foreign live state");
      require(guarded_destroy(*context_only.generation, emitter_destroy,
                              emitter_object.data()),
              "context-only destroy entrypoint failed");
    }
    trace.phase = tracer::TracePhase::Post;
    require(guarded_entry(*generation.generation, post, object.data(), &trace),
            "generated POST entrypoint failed on the private callback module");
    require(sink.batches.size() == 2,
            "generated POST did not publish its state snapshot");
    require(guarded_destroy(*generation.generation, destroy, object.data()),
            "generated destroy entrypoint failed");
  }
#else
  require(!generation && registry.scope_count() == 0,
          "non-Linux activation published an unsupported private generation");
#endif

#if defined(__linux__) && defined(__x86_64__)
  tracer::ContextActivationRequest shared_child_request = request;
  shared_child_request.source_lineage_id[0] = 0x77;
  shared_child_request.file_name = "shared-child-codec-activation.cc";
  shared_child_request.source = R"CPP(
namespace batch {
struct Scope {};
struct SharedChild { int value = 0; };
struct First : shared_state<Scope> { SharedChild child; };
struct Second : shared_state<Scope> { SharedChild child; };
struct Interceptor {
  Interceptor(State<First>, State<Second>);
  void pre(const Call&, First& first, Second& second) {
    ++first.child.value;
    ++second.child.value;
  }
};
}
)CPP";
  tracer::ContextRegistry shared_child_registry;
  const auto shared_child = tracer::activate_context_source(
      shared_child_request, shared_child_registry);
  require(
      shared_child && shared_child.generation->scopes().size() == 1 &&
          shared_child.generation->scopes()[0]->states.size() == 2,
      "two-state private owner with shared nested codec did not activate: " +
          shared_child.error);

  tracer::ContextActivationRequest generic_value_request = request;
  generic_value_request.source_lineage_id[0] = 0x7a;
  generic_value_request.file_name = "generic-payload-live-activation.cc";
  generic_value_request.source = R"CPP(
#include <string>
namespace live_payload {
struct Scope {};
struct TextScope {};
struct Emitter {
  void pre(const Call& call) {
    call.context().init_scope<Scope>(42);
    call.context().with<Scope, int>([](int& value) { ++value; });
    call.context().init_scope<TextScope>(std::string("copy"));
    call.context().with<TextScope, std::string>(
        [](std::string& value) { value += "!"; });
  }
  void post(const Call& call) { call.context().event("generic.value"); }
};
}
)CPP";
  tracer::ContextRegistry generic_value_registry;
  const auto generic_value_generation = tracer::activate_context_source(
      generic_value_request, generic_value_registry);
  require(generic_value_generation &&
              generic_value_generation.generation->scopes().size() == 2 &&
              generic_value_generation.generation->schemas().size() == 2,
          "generic private owner failed activation: " +
              generic_value_generation.error);
  CaptureSink generic_value_sink;
  tracer::context_runtime::EventMetadata generic_value_metadata = metadata;
  generic_value_metadata.capture_hook_id = 3;
  tracer::context_runtime::SharedSourceRuntime generic_value_runtime(
      generic_value_generation.generation->scopes(), pool,
      generic_value_metadata, &capture_batch, &generic_value_sink);
  const auto generic_value_construct = reinterpret_cast<Entry>(
      generic_value_generation.generation->construct_entry());
  const auto generic_value_pre =
      reinterpret_cast<Entry>(generic_value_generation.generation->pre_entry());
  const auto generic_value_post = reinterpret_cast<Entry>(
      generic_value_generation.generation->post_entry());
  const auto generic_value_destroy = reinterpret_cast<Destroy>(
      generic_value_generation.generation->destroy_entry());
  alignas(64) std::array<std::byte, 4096> generic_value_object{};
  {
    tracer::context_runtime::SharedSourceRuntime::Invocation invocation(
        generic_value_runtime, &call);
    trace.phase = tracer::TracePhase::Pre;
    require(invocation.entered() &&
                guarded_entry(*generic_value_generation.generation,
                              generic_value_construct,
                              generic_value_object.data(), &trace) &&
                guarded_entry(*generic_value_generation.generation,
                              generic_value_pre, generic_value_object.data(),
                              &trace),
            "generic init_scope<S>(42) did not reach the private owner");
    trace.phase = tracer::TracePhase::Post;
    require(guarded_entry(*generic_value_generation.generation,
                          generic_value_post, generic_value_object.data(),
                          &trace) &&
                generic_value_sink.batches.size() == 1 &&
                generic_value_sink.batches[0].event().states_size() == 2 &&
                generic_value_sink.batches[0].schemas_size() == 2,
            "generic payload event did not capture its initialized state");
    require(guarded_destroy(*generic_value_generation.generation,
                            generic_value_destroy, generic_value_object.data()),
            "generic payload interceptor failed destruction");
  }
  const auto &generic_batch = generic_value_sink.batches[0];
  std::vector<tracer::context_engine::SchemaRegistration> generic_registrations;
  generic_registrations.reserve(
      generic_value_generation.generation->schemas().size());
  std::string generic_decode_error;
  for (const auto &schema : generic_value_generation.generation->schemas()) {
    const auto state = std::find_if(
        generic_batch.event().states().begin(),
        generic_batch.event().states().end(), [&](const auto &candidate) {
          return candidate.schema_identity() == schema->revision_id;
        });
    google::protobuf::FileDescriptorSet descriptor_set;
    require(state != generic_batch.event().states().end() &&
                state->epoch_schema_id() &&
                descriptor_set.ParseFromString(schema->descriptor_set),
            "generic payload descriptor metadata was malformed");
    tracer::context_engine::SchemaRegistration registration;
    registration.id = state->epoch_schema_id();
    registration.descriptor_set = std::move(descriptor_set);
    registration.root = schema->root_type;
    registration.family = schema->family;
    registration.container_roles = schema->container_roles;
    generic_registrations.push_back(std::move(registration));
  }
  tracer::context_engine::ContextReceiver generic_receiver(
      std::make_shared<tracer::context_engine::ReceiverLedger>());
  require(generic_receiver.register_schema_set(generic_registrations,
                                               &generic_decode_error),
          "generic payload receiver rejected its schemas: " +
              generic_decode_error);
  bool decoded_integer = false, decoded_string = false;
  for (const auto &payload : generic_batch.event().states()) {
    const auto payload_bytes = ::tracer::compat::span<const std::byte>(
        reinterpret_cast<const std::byte*>(payload.payload().data()), payload.payload().size());
    const auto decoded = generic_receiver.decode(
        payload.epoch_schema_id(), payload_bytes, &generic_decode_error);
    const auto *value_field =
        decoded ? decoded->message()->GetDescriptor()->FindFieldByName("value")
                : nullptr;
    require(decoded && value_field,
            "captured generic payload could not be decoded: " +
                generic_decode_error);
    if (value_field->cpp_type() ==
        google::protobuf::FieldDescriptor::CPPTYPE_INT32) {
      decoded_integer = decoded->message()->GetReflection()->GetInt32(
                            *decoded->message(), value_field) == 43;
    } else if (value_field->cpp_type() ==
               google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
      decoded_string = decoded->message()->GetReflection()->GetString(
                           *decoded->message(), value_field) == "copy!";
    }
  }
  require(decoded_integer && decoded_string,
          "private generic int/string payloads did not roundtrip (expected "
          "43 and copy!)");

  const auto make_helper_generation = [](int initial_value, tracer::ContextRegistry& target,
                                         bool unrelated_helpers = false, const char* marker = "#7",
                                         bool cold_helper = false) {
    tracer::ContextActivationRequest helper_request;
    helper_request.source_lineage_id[0] = 0x79;
    helper_request.file_name = "owner-lifecycle-closure.cc";
    helper_request.compiler_arguments = {
        "-std=c++20",
        "-fPIC",
        "-O0",
        "-fno-stack-protector",
        "-I" TRACER_TEST_SOURCE_DIR "/include",
        "-I" TRACER_TEST_SOURCE_DIR "/src",
        "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
    helper_request.compiler_arguments.insert(
        helper_request.compiler_arguments.end(),
        std::begin(kContextSystemArguments), std::end(kContextSystemArguments));
    helper_request.source = "#include <utility>\n"
                            "namespace fingerprint { struct Scope {}; "
                            "__attribute__((noinline" +
                            std::string(cold_helper ? ", cold" : "") +
                            ")) int initialize_value() { const char marker[] = \"" + marker +
                            "\"; return marker[0] == '#' ? " + std::to_string(initial_value) +
                            " : 0; } "
                            "struct State : state<Scope> { int value; "
                            "State() : value(initialize_value()) {} }; "
                            "struct Interceptor { Interceptor(::tracer::State<State>); "
                            "void pre(const Call&, State&) {} }; }";
    if (unrelated_helpers) {
      helper_request.source +=
          "\nstd::pair<int, int> unrelated_pair_helper() { return {1, 2}; } "
          "int unrelated_callback_helper() { return 9; }\n";
    }
    return tracer::activate_context_source(helper_request, target);
  };
  tracer::ContextRegistry lifecycle_registry;
  const auto first_lifecycle = make_helper_generation(1, lifecycle_registry);
  require(first_lifecycle && first_lifecycle.generation->scopes().size() == 1,
          "owner lifecycle helper fixture failed to activate: " +
              first_lifecycle.error);
  const auto unrelated_lifecycle =
      make_helper_generation(1, lifecycle_registry, true);
  require(
      unrelated_lifecycle &&
          unrelated_lifecycle.generation->scopes()[0] ==
              first_lifecycle.generation->scopes()[0] &&
          unrelated_lifecycle.generation->scopes()[0]
                  ->states[0]
                  .lifecycle_fingerprint ==
              first_lifecycle.generation->scopes()[0]
                  ->states[0]
                  .lifecycle_fingerprint,
      "unrelated callback/template IR changed the owner closure fingerprint: " +
          unrelated_lifecycle.error);
  const auto second_lifecycle = make_helper_generation(2, lifecycle_registry);
  require(second_lifecycle &&
              second_lifecycle.generation->scopes().size() == 1 &&
              second_lifecycle.generation->scopes()[0] !=
                  first_lifecycle.generation->scopes()[0] &&
              second_lifecycle.generation->scopes()[0]
                      ->states[0]
                      .lifecycle_fingerprint !=
                  first_lifecycle.generation->scopes()[0]
                      ->states[0]
                      .lifecycle_fingerprint &&
              first_lifecycle.generation->scopes()[0]->owner_generation &&
              second_lifecycle.generation->scopes()[0]->owner_generation &&
              lifecycle_registry
                      .find_scope_generations(
                          first_lifecycle.generation->scopes()[0]
                              ->source_lineage_id,
                          "fingerprint::Scope")
                      .size() == 2,
          "changed reachable lifecycle helper reused or discarded a pinned "
          "owner generation");

  tracer::ContextRegistry literal_registry;
  const auto literal_seven = make_helper_generation(1, literal_registry);
  const auto literal_eight = make_helper_generation(1, literal_registry, false, "#8");
  require(literal_seven && literal_eight &&
              literal_seven.generation->scopes()[0]->states[0].lifecycle_fingerprint !=
                  literal_eight.generation->scopes()[0]->states[0].lifecycle_fingerprint,
          "quoted #number global data was mistaken for an LLVM attribute-group ID");

  tracer::ContextRegistry attribute_registry;
  const auto ordinary_helper = make_helper_generation(1, attribute_registry);
  const auto cold_helper = make_helper_generation(1, attribute_registry, false, "#7", true);
  require(ordinary_helper && cold_helper &&
              ordinary_helper.generation->scopes()[0]->states[0].lifecycle_fingerprint !=
                  cold_helper.generation->scopes()[0]->states[0].lifecycle_fingerprint,
          "semantic function attributes were omitted from the owner closure fingerprint");
#endif
}

void verify_context_nested_live() {
#if defined(__linux__) && defined(__x86_64__)
  tracer::ContextActivationRequest request;
  request.source_lineage_id[0] = 0x7b;
  std::ifstream fixture(TRACER_TEST_SOURCE_DIR
                        "/tests/fixtures/shared_context_nested.cpp");
  request.source.assign(std::istreambuf_iterator<char>(fixture), {});
  require(!request.source.empty(), "activation fixture could not be read");
  request.file_name = "shared-context-nested-live.cc";
  request.compiler_arguments = {"-std=c++20",
                                "-fPIC",
                                "-O0",
                                "-fno-stack-protector",
                                "-I" TRACER_TEST_SOURCE_DIR "/include",
                                "-I" TRACER_TEST_SOURCE_DIR "/src",
                                "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  request.compiler_arguments.insert(request.compiler_arguments.end(),
                                    std::begin(kContextSystemArguments),
                                    std::end(kContextSystemArguments));

  tracer::ContextRegistry registry;
  const auto generation = tracer::activate_context_source(request, registry);
  require(generation && generation.generation->scopes().size() == 1 &&
              generation.generation->schemas().size() == 1,
          "unchanged nested fixture failed native activation: " +
              generation.error);

  CaptureSink sink;
  tracer::context_engine::ContextCapturePool pool;
  tracer::context_runtime::EventMetadata metadata;
  metadata.source_id.assign(16, 'n');
  metadata.task_id.assign(16, 't');
  metadata.invocation_id.assign(16, 'i');
  metadata.probe_id.assign(16, 'p');
  metadata.capture_hook_id = 1;
  tracer::context_runtime::SharedSourceRuntime runtime(
      generation.generation->scopes(), pool, metadata, &capture_batch, &sink);
  tracer::TraceContext trace{};
  trace.call_id = 73;
  tracer::Call call(trace);
  const auto construct =
      reinterpret_cast<Entry>(generation.generation->construct_entry());
  const auto pre = reinterpret_cast<Entry>(generation.generation->pre_entry());
  const auto post =
      reinterpret_cast<Entry>(generation.generation->post_entry());
  const auto destroy =
      reinterpret_cast<Destroy>(generation.generation->destroy_entry());
  alignas(64) std::array<std::byte, 4096> object{};
  {
    tracer::context_runtime::SharedSourceRuntime::Invocation invocation(runtime,
                                                                        &call);
    trace.phase = tracer::TracePhase::Pre;
    require(
        invocation.entered() &&
            guarded_entry(*generation.generation, construct, object.data(),
                          &trace) &&
            guarded_entry(*generation.generation, pre, object.data(), &trace),
        "unchanged nested fixture failed private construct/PRE");
    {
      tracer::context_execution::OriginalDispatch original_dispatch;
      require(original_dispatch.entered(),
              "nested fixture did not enter original dispatch");
    }
    trace.phase = tracer::TracePhase::Post;
    require(
        guarded_entry(*generation.generation, post, object.data(), &trace) &&
            sink.batches.size() == 1 &&
            sink.batches[0].event().name() == "request.completed" &&
            sink.batches[0].event().states_size() == 1,
        "unchanged nested fixture failed POST capture");
    require(guarded_destroy(*generation.generation, destroy, object.data()),
            "unchanged nested fixture failed private destruction");
  }

  const auto &batch = sink.batches.front();
  const auto &wire_state = batch.event().states(0);
  const auto schema = std::find_if(
      generation.generation->schemas().begin(),
      generation.generation->schemas().end(), [&](const auto &candidate) {
        return candidate->revision_id == wire_state.schema_identity();
      });
  google::protobuf::FileDescriptorSet descriptor_set;
  require(schema != generation.generation->schemas().end() &&
              wire_state.epoch_schema_id() &&
              descriptor_set.ParseFromString((*schema)->descriptor_set),
          "nested fixture capture omitted its leased schema metadata");
  tracer::context_engine::SchemaRegistration registration;
  registration.id = wire_state.epoch_schema_id();
  registration.descriptor_set = std::move(descriptor_set);
  registration.root = (*schema)->root_type;
  registration.family = (*schema)->family;
  registration.container_roles = (*schema)->container_roles;
  tracer::context_engine::ContextReceiver receiver(
      std::make_shared<tracer::context_engine::ReceiverLedger>());
  std::string decode_error;
  require(receiver.register_schema(registration.id, registration.descriptor_set,
                                   registration.root, registration.family,
                                   &decode_error, registration.container_roles),
          "nested fixture receiver rejected its retained schema: " +
              decode_error);
  const auto payload_bytes = ::tracer::compat::span<const std::byte>(
      reinterpret_cast<const std::byte*>(wire_state.payload().data()), wire_state.payload().size());
  const auto decoded =
      receiver.decode(registration.id, payload_bytes, &decode_error);
  require(static_cast<bool>(decoded),
          "nested fixture payload failed receiver decode: " + decode_error);

  const auto *root = decoded->message();
  const auto *request_id = root->GetDescriptor()->FindFieldByName("request_id");
  const auto *items = root->GetDescriptor()->FindFieldByName("items");
  const auto *services = root->GetDescriptor()->FindFieldByName("services");
  require(request_id && items && services && items->is_repeated() &&
              services->is_repeated() &&
              root->GetReflection()->GetString(*root, request_id) ==
                  "request-42" &&
              root->GetReflection()->FieldSize(*root, items) == 1,
          "nested fixture root fields did not roundtrip");
  const auto &item = root->GetReflection()->GetRepeatedMessage(*root, items, 0);
  const auto *sku = item.GetDescriptor()->FindFieldByName("sku");
  const auto *quantity = item.GetDescriptor()->FindFieldByName("quantity");
  require(sku && quantity &&
              item.GetReflection()->GetString(item, sku) == "book" &&
              item.GetReflection()->GetUInt32(item, quantity) == 2,
          "nested fixture repeated item did not roundtrip");

  const auto &service =
      root->GetReflection()->GetRepeatedMessage(*root, services, 0);
  const auto *key = service.GetDescriptor()->FindFieldByName("key");
  const auto *value = service.GetDescriptor()->FindFieldByName("value");
  require(key && value &&
              service.GetReflection()->GetString(service, key) == "billing",
          "nested fixture map key did not roundtrip");
  const auto &stats = service.GetReflection()->GetMessage(service, value);
  const auto *calls = stats.GetDescriptor()->FindFieldByName("calls");
  const auto *durations =
      stats.GetDescriptor()->FindFieldByName("durations_ms");
  require(calls && durations && durations->is_repeated() &&
              stats.GetReflection()->GetUInt64(stats, calls) == 1 &&
              stats.GetReflection()->FieldSize(stats, durations) == 1 &&
              stats.GetReflection()->GetRepeatedDouble(stats, durations, 0) ==
                  12.5,
          "nested fixture mapped stats did not roundtrip");
#endif
}

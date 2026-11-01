#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "context_facade.hpp"
#include "context_hooks.hpp"
#include "context_lifecycle.hpp"
#include "context_test_config.hpp"
#include "tracer/callback.hpp"

namespace {
struct FacadeScope {};
struct FacadeState : tracer::shared_state<FacadeScope> {
  int value = 7;
};
struct PayloadScope {};

struct Runtime;
struct HookFrame {
  tracer::context_facade::NativeHookContext native;
  tracer::context_execution::CurrentHook current;
  HookFrame(Runtime &runtime, const void *call,
            const tracer::TraceContext *trace);
};

struct Runtime {
  int value = 19;
  int frames = 0;
  int hooks = 0;
  int hook_exits = 0;
  int access = 0;
  int access_exits = 0;
  int created = 0;
  int initialized = 0;
  int ended = 0;
  int events = 0;
  bool callback_failed = false;
  bool throw_on_begin = false;
  tracer::context_facade::Status get_status = 0;
  tracer::context_facade::Status access_status = 0;
  tracer::context_facade::Status end_status = 0;
  alignas(HookFrame) std::byte hook_buffer[sizeof(HookFrame)];
};

HookFrame::HookFrame(Runtime &runtime, const void *call,
                     const tracer::TraceContext *trace)
    : native{tracer::context_facade::NativeHookContext::kMagic,
             {nullptr, nullptr},
             call,
             trace},
      current(&native) {
  native.generation = *tracer::context_facade::generation_binding();
}

Runtime &runtime(void *opaque) { return *static_cast<Runtime *>(opaque); }
void *native_context(void *opaque) { return opaque; }
void *frame_enter(void *opaque) {
  auto &r = runtime(opaque);
  ++r.frames;
  return &r;
}
void frame_exit(void *opaque, void *) noexcept { --runtime(opaque).frames; }
void create_or_attach(void *opaque, void *, std::uint64_t) {
  ++runtime(opaque).created;
}
void *hook_begin(void *opaque, const void *call,
                 const tracer::TraceContext *trace,
                 const tracer::context_facade::RequestDescriptor *, std::size_t,
                 bool, bool) {
  auto &r = runtime(opaque);
  if (r.throw_on_begin)
    throw std::runtime_error("acquisition");
  ++r.hooks;
  auto *frame = new (r.hook_buffer) HookFrame(r, call, trace);
  return frame;
}
void hook_end(void *opaque, void *token, bool failed) noexcept {
  auto &r = runtime(opaque);
  r.callback_failed = failed;
  ++r.hook_exits;
  static_cast<HookFrame *>(token)->~HookFrame();
}
void *access_begin(void *opaque,
                   const tracer::context_facade::RequestDescriptor *,
                   std::size_t, tracer::context_facade::AccessKind,
                   tracer::context_facade::Status *status) {
  if (status)
    *status = runtime(opaque).access_status;
  if (runtime(opaque).access_status)
    return nullptr;
  ++runtime(opaque).access;
  return opaque;
}
void access_end(void *opaque, void *) noexcept {
  ++runtime(opaque).access_exits;
}
tracer::context_facade::Status get(void *opaque, std::uint64_t,
                                   void **result) noexcept {
  *result = &runtime(opaque).value;
  return runtime(opaque).get_status;
}
void init_scope(void *opaque, std::uint64_t, std::uint64_t, const void *) {
  ++runtime(opaque).initialized;
}
tracer::context_facade::Status end_scope(void *opaque, std::uint64_t) noexcept {
  const auto status = runtime(opaque).end_status;
  if (!status)
    ++runtime(opaque).ended;
  return status;
}
bool event(void *opaque, const tracer::TraceContext *, std::string_view,
           std::size_t) noexcept {
  ++runtime(opaque).events;
  return true;
}
std::uint64_t thread_id(void *) noexcept { return 42; }

const tracer::context_facade::NativeFunctionTable table{
    1,          native_context, frame_enter,  frame_exit, create_or_attach,
    hook_begin, hook_end,       access_begin, access_end, get,
    init_scope, end_scope,      event,        thread_id};
} // namespace

namespace tracer::context_facade {
template <> struct StateTraits<::FacadeState> {
  static constexpr RequestDescriptor descriptor{
      11, 13, "FacadeState", "FacadeScope", "", false, true};
};
template <> struct ScopeTraits<::FacadeScope> {
  static constexpr ScopeId id = 13;
};
template <> struct ScopeTraits<::PayloadScope> {
  static constexpr ScopeId id = 23;
};
template <> struct PayloadTraits<::PayloadScope, int> {
  static constexpr RequestDescriptor descriptor{
      17, 23, "", "PayloadScope", "int", true, false};
};
} // namespace tracer::context_facade

void verify_context_facade() {
  using namespace tracer::context_facade;
  tracer::context_execution::Scope tracking(
      tracer::context_execution::Mode::Tracking);
  if (!tracking.entered())
    throw std::runtime_error("could not enter native tracking test mode");
  Runtime r;
  bind_generation(&table, &r);
  auto &context = current_context();
  if (&context != &current_context())
    throw std::runtime_error("Context facade is not stateless");
  tracer::Call call(tracer::TraceContext{});
  if (&call.context() != &context)
    throw std::runtime_error(
        "Call::context did not return the stateless facade");

  run_direct_hook<FacadeState, PayloadAccess<PayloadScope, int>>(
      call, true, [&] {
        auto *state = request<FacadeState>();
        auto *payload = request_payload<PayloadScope, int>();
        if (state->value != 19 || payload != &r.value)
          throw std::runtime_error(
              "typed facade access returned wrong address");
        call.context().event("inside", 3);
      });
  if (r.hooks != 1 || r.hook_exits != 1 || r.callback_failed || r.events != 1)
    throw std::runtime_error("direct hook boundary or event dispatch failed");

  r.get_status = 7;
  bool native_get_status = false;
  try {
    run_direct_hook<FacadeState>(call, false,
                                 [] { (void)request<FacadeState>(); });
  } catch (const HookFailure &) {
    native_get_status = true;
  }
  r.get_status = 0;
  if (!native_get_status || !r.callback_failed)
    throw std::runtime_error(
        "native get status did not become a private error");

  bool hook_failure_propagated = false;
  try {
    run_direct_hook<FacadeState>(call, false, [&] {
      throw std::runtime_error("contained callback failure");
    });
  } catch (const HookFailure &) {
    hook_failure_propagated = true;
  }
  if (!r.callback_failed || r.hook_exits != 3 || !hook_failure_propagated)
    throw std::runtime_error(
        "callback failure did not signal the generated hook wrapper");
  r.throw_on_begin = true;
  bool acquisition_propagated = false;
  try {
    run_direct_hook<FacadeState>(call, false, [] {});
  } catch (const std::runtime_error &) {
    acquisition_propagated = true;
  }
  r.throw_on_begin = false;
  if (!acquisition_propagated)
    throw std::runtime_error("hook acquisition failure was swallowed");

  with_state<FacadeState>([](FacadeState &state) { ++state.value; });
  with_payload<PayloadScope, int>([](int &payload) { ++payload; });
  if (r.value != 21 || r.access != 2 || r.access_exits != 2)
    throw std::runtime_error("short access intervals were not released");

  r.access_status = 8;
  bool access_status = false;
  try {
    with_state<FacadeState>([](FacadeState &) {});
  } catch (const ContextError &error) {
    access_status = error.code == 8;
  }
  r.access_status = 0;
  if (!access_status)
    throw std::runtime_error("native access status was not preserved");

  r.end_status = 9;
  bool end_status = false;
  try {
    end_scope<PayloadScope>();
  } catch (const ContextError &error) {
    end_status = error.code == 9;
  }
  r.end_status = 0;
  if (!end_status)
    throw std::runtime_error("native end status was not preserved");

  {
    InvocationFrame frame(current_context());
    frame.push_create_or_attach<FacadeState>();
    // The generated C++ member-template definitions are validated below in a
    // fresh Clang TU; this native harness exercises the same table dispatch.
    table.init_scope(&r, 23, 17, &r.value);
    table.end_scope(&r, 23);
  }
  if (r.frames != 0 || r.created != 1 || r.initialized != 1 || r.ended != 1)
    throw std::runtime_error(
        "lifecycle facade did not restore transactionally");

  Runtime nested_runtime;
  {
    InvocationFrame outer(current_context());
    {
      ThreadBinding nested(&table, &nested_runtime);
      InvocationFrame inner(current_context());
    }
    outer.pop_all_and_restore();
  }
  if (r.frames != 0 || nested_runtime.frames != 0)
    throw std::runtime_error(
        "nested generation changed an outer frame binding");

  std::vector<FacadeIdentity> identities{
      {tracer::ContextDependency{
           "FacadeState", "FacadeScope", true, {}, false, {}},
       11, 13},
      {tracer::ContextDependency{"", "PayloadScope", false, {}, true, "int"},
       17, 23},
      {tracer::ContextDependency{
           "",
           "TextPayloadScope",
           false,
           {},
           true,
           "class std::basic_string<char, std::char_traits<char>, "
           "std::allocator<char>>"},
       29, 31}};
  const auto generated =
      generate_context_facade("__clang_tracer_context_fixture",
                              "__tracer_context_bind_fixture", identities);
  if (!generated.error.empty() || generated.descriptors.size() != 3 ||
      generated.definitions.find("PayloadTraits<::PayloadScope, ::int>") !=
          std::string::npos ||
      generated.definitions.find("PayloadTraits<::PayloadScope, int>") ==
          std::string::npos ||
      generated.definitions.find(
          "PayloadTraits<::TextPayloadScope, "
          "::std::basic_string<char, std::char_traits<char>, "
          "std::allocator<char>>>") == std::string::npos ||
      generated.bind_entrypoint != "__tracer_context_bind_fixture")
    throw std::runtime_error("facade trait/entrypoint generation failed: " +
                             generated.error);

  std::string source = "#include \"context_facade.hpp\"\n"
                       "#include \"tracer/callback.hpp\"\n"
                       "struct FacadeScope {}; struct FacadeState : "
                       "tracer::shared_state<FacadeScope> { int value; }; "
                       "struct PayloadScope {}; struct TextPayloadScope {};\n" +
                       generated.definitions;
  std::vector<std::string> args{"-std=c++20",
                                "-I" TRACER_TEST_SOURCE_DIR "/include",
                                "-I" TRACER_TEST_SOURCE_DIR "/src",
                                "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  args.insert(args.end(), std::begin(kContextSystemArguments),
              std::end(kContextSystemArguments));
  args.push_back("-I" + (std::filesystem::path(TRACER_TEST_RESOURCE_DIR)
                             .parent_path()
                             .parent_path()
                             .parent_path() /
                         "include")
                            .string());
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      source, args, "context-facade-generated.cc");
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("generated facade source did not compile: " +
                             source);

  const std::string live_user_source =
      "#include \"context_facade.hpp\"\n"
      "#include \"tracer/callback.hpp\"\n"
      "struct LiveScope {}; struct LiveState : tracer::shared_state<LiveScope> "
      "{ int value=0; }; "
      "struct LiveHook { LiveHook(State<LiveState>); "
      "void pre(const Call& call, LiveState& state) { ++state.value; "
      "++call.context().get<LiveState>().value; "
      "call.context().with<LiveState>([](LiveState& value) { ++value.value; "
      "}); "
      "} };";
  auto live_ast = clang::tooling::buildASTFromCodeWithArgs(
      live_user_source, args, "context-facade-live-source.cc");
  if (!live_ast || live_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("live lowered facade fixture did not parse");
  auto live_analysis =
      tracer::analyze_context_declarations(live_ast->getASTContext());
  if (!live_analysis.error.empty() || live_analysis.states.size() != 1)
    throw std::runtime_error("live facade fixture did not produce one state");
  const auto &live_state = live_analysis.states.front();
  const auto live_facade = tracer::context_facade::generate_context_facade(
      "engine", "__tracer_context_bind_live", {{live_state, 31, 37}});
  if (!live_facade.error.empty())
    throw std::runtime_error("live facade identity generation failed: " +
                             live_facade.error);
  std::string lowered_source = live_user_source + live_facade.definitions;
  auto traits_ast = clang::tooling::buildASTFromCodeWithArgs(
      lowered_source, args, "context-facade-live-traits.cc");
  if (!traits_ast || traits_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error(
        "trait definitions after user declarations failed");
  const auto hooks = tracer::generate_context_hooks(*traits_ast);
  if (!hooks.error.empty())
    throw std::runtime_error("live hook lowering failed: " + hooks.error);
  auto hook_ast = clang::tooling::buildASTFromCodeWithArgs(
      hooks.compilation_copy, args, "context-facade-live-hooks.cc");
  if (!hook_ast || hook_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("generated hook wrappers did not compile");
  const auto lifecycle = tracer::generate_context_lifecycle(*hook_ast);
  if (!lifecycle.error.empty())
    throw std::runtime_error("live lifecycle lowering failed: " +
                             lifecycle.error);
  auto complete = clang::tooling::buildASTFromCodeWithArgs(
      lifecycle.compilation_copy, args, "context-facade-live-complete.cc");
  if (!complete || complete->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error(
        "combined lowered lifecycle/hook source did not compile");
  clear_thread_generation();
}

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "context_analysis.hpp"
#include "context_binding.hpp"
#include "context_lifecycle.hpp"
#include "context_lifecycle_generated.hpp"
#include "context_owner.hpp"
#include "context_test_config.hpp"
#include "shared_context_nested_generated.hpp"
#include "tracer/compat.hpp"

void verify_context_binding();
void verify_generated_owner();
void verify_context_hooks();
void verify_context_facade();
void verify_context_runtime();
void verify_context_activation();
void verify_context_nested_live();
void verify_context_generic_lineage();
void verify_context_schema();
namespace tracer::context_engine::test {
void verify_context_capture();
}
namespace tracer::test {
void verify_context_registry();
}
void verify_context_delivery();
void verify_context_collector();
void verify_context_support_effects();
void verify_context_stdlib();

namespace {
void require(bool value, const char *message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

std::unique_ptr<clang::ASTUnit> parse(const std::string &source) {
  std::vector<std::string> arguments{"-std=c++20",
                                     "-I" TRACER_TEST_SOURCE_DIR "/include",
                                     "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  arguments.insert(arguments.end(), std::begin(kContextSystemArguments),
                   std::end(kContextSystemArguments));
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + source, arguments,
      "context-fixture.cc");
  require(ast && !ast->getDiagnostics().hasErrorOccurred(),
          "declaration fixture failed to parse");
  return ast;
}

tracer::ContextDeclarations analyze(const std::string &source) {
  const auto ast = parse(source);
  return tracer::analyze_context_declarations(ast->getASTContext());
}

void verify_lifecycle_rejections() {
  const std::string types = "struct S {}; struct D : shared_state<S> {}; ";
  for (const auto *body :
       {"struct H { H(State<D>&); void pre(const Call&) {} };",
        "struct H { H(State<D>); ~H() noexcept {} void pre(const Call&) {} };",
        "struct H { H(State<D>); H(const H&); void pre(const Call&) {} };",
        "struct H { H(State<D>); H& operator=(const H&); void pre(const Call&) "
        "{} };",
        "struct H { H(State<D>) noexcept; void pre(const Call&) {} };",
        "#define CLASS struct H { H(State<D>); void pre(const Call&) {} "
        "};\nCLASS\n"}) {
    const auto ast = parse(types + "\n" + body);
    auto &sources = ast->getSourceManager();
    const auto before = sources.getBufferData(sources.getMainFileID()).str();
    const auto rejected = tracer::generate_context_lifecycle(*ast);
    require(!rejected.error.empty() && rejected.compilation_copy.empty() &&
                rejected.classes.empty(),
            "unsupported lifecycle partially published generated source");
    require(sources.getBufferData(sources.getMainFileID()) == before,
            "lifecycle renderer changed the original compiler source buffer");
  }
  const auto ast =
      parse(types + "struct Good { Good(State<D>); void pre(const Call&) {} }; "
                    "struct Bad { Bad(State<D>); ~Bad() noexcept {} void "
                    "pre(const Call&) {} }; ");
  const auto partial = tracer::generate_context_lifecycle(*ast);
  require(!partial.error.empty() && partial.compilation_copy.empty() &&
              partial.classes.empty(),
          "failure in a later class published an earlier lifecycle rewrite");
}
} // namespace

void verify_generated_lifecycle() {
  namespace ce = tracer::context_engine;
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  engine::CurrentContext current(context);
  engine::register_scope<demo::RequestScope, demo::RequestState>();
  static_assert(!std::is_copy_constructible_v<demo::RequestInterceptor>);
  static_assert(!std::is_move_constructible_v<demo::RequestInterceptor>);
  static_assert(std::is_copy_constructible_v<demo::RequestState>);
  tracer::TraceContext raw;
  const tracer::Call call(raw);
  {
    demo::RequestInterceptor outer{State<demo::RequestState>{}};
    require(context.binding_count() == 1,
            "generated constructor did not push state");
    engine::inspect<demo::RequestState>(
        context, [&](auto &state) { outer.pre(call, state); });
    {
      demo::RequestInterceptor inner{State<demo::RequestState>{}};
      require(context.binding_count() == 2,
              "generated nested constructor did not attach");
      engine::inspect<demo::RequestState>(
          context, [&](auto &state) { inner.pre(call, state); });
    }
    require(context.binding_count() == 1,
            "generated nested destructor did not restore cursor");
    engine::inspect<demo::RequestState>(context, [&](auto &state) {
      require(state.request_id == "request-42" && state.items.size() == 2 &&
                  state.items[0].sku == "book" && state.items[1].quantity == 2,
              "unchanged example did not mutate one shared object");
    });
  }
  require(!context.binding_count() &&
              engine::LifecycleCounts<demo::RequestState>::made == 1 &&
              engine::LifecycleCounts<demo::RequestState>::destroyed == 1,
          "generated exact-example lifecycle did not finalize once");

  engine::register_scope<lifecycle_demo::Scope, lifecycle_demo::Data,
                         lifecycle_demo::Other>();
  engine::register_scope<lifecycle_demo::FailureScope,
                         lifecycle_demo::Failing>();
  {
    lifecycle_demo::A outer{State<lifecycle_demo::Data>{}};
    require(outer.tracer_context_frame_0 == 17,
            "hidden member collided with user field");
    {
      lifecycle_demo::B inner{State<lifecycle_demo::Data>{},
                              State<lifecycle_demo::Other>{}};
      require(context.binding_count() == 2,
              "same-scope dependencies did not deduplicate");
      engine::inspect<lifecycle_demo::Data>(
          context, [&](auto &state) { inner.pre(call, state); });
    }
    engine::inspect<lifecycle_demo::Data>(context, [&](auto &state) {
      require(state.value == 42,
              "generated mixed dependencies did not share state");
    });
  }
  require(!context.binding_count() && lifecycle_demo::constructed == 1 &&
              lifecycle_demo::destroyed == 1,
          "user lifecycle ran more than once for attached state");
  try {
    lifecycle_demo::Failure failed{State<lifecycle_demo::Data>{},
                                   State<lifecycle_demo::Failing>{}};
    throw std::runtime_error("expected generated constructor failure");
  } catch (const ce::Error &error) {
    require(error.code() == ce::ErrorCode::construction_failed,
            "wrong generated constructor failure");
  }
  require(!context.binding_count() && lifecycle_demo::constructed == 2 &&
              lifecycle_demo::destroyed == 2,
          "generated partial-construction cleanup leaked state or cursor");
  std::cout << "generated lifecycle ownership, nesting, deduplication and "
               "rollback passed\n";
}

void verify_owner_rendering() {
  std::ifstream file(TRACER_TEST_SOURCE_DIR
                     "/tests/fixtures/shared_context_nested.cpp");
  const std::string source{std::istreambuf_iterator<char>(file), {}};
  const auto ast = parse(source);
  const auto &sources = ast->getSourceManager();
  const auto before = sources.getBufferData(sources.getMainFileID()).str();
  const auto generated = tracer::generate_context_owner(*ast);
  require(generated.error.empty() && generated.states.size() == 1 &&
              generated.states[0].type.state_type == "demo::RequestState" &&
              ::tracer::compat::starts_with(generated.compilation_copy, before),
          "owner renderer changed the original nested example");
  require(sources.getBufferData(sources.getMainFileID()) == before,
          "owner renderer changed the original compiler buffer");
  std::vector<std::string> arguments{"-std=c++20",
                                     "-fPIC",
                                     "-O0",
                                     "-I" TRACER_TEST_SOURCE_DIR "/include",
                                     "-I" TRACER_TEST_SOURCE_DIR "/src",
                                     "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  arguments.insert(arguments.end(), std::begin(kContextSystemArguments),
                   std::end(kContextSystemArguments));
  auto compiled = tracer::compile_context_module(
      generated.compilation_copy, arguments, {"demo::RequestState"});
  if (!compiled)
    throw std::runtime_error(llvm::toString(compiled.takeError()));
  auto symbols = tracer::context_bridge_symbols(compiled->contract,
                                                {generated.states[0].construct,
                                                 generated.states[0].destroy,
                                                 generated.states[0].address});
  if (!symbols)
    throw std::runtime_error(llvm::toString(symbols.takeError()));
  auto owner = tracer::PrivateContextCode::load(
      std::move(compiled->module), compiled->contract, std::move(*symbols));
  require(
      !owner,
      "nested owner activated without its private standard-library provider");
  require(llvm::toString(owner.takeError())
                  .find("unsupported_state_support_profile") !=
              std::string::npos,
          "nested owner failed outside the support admission boundary");
  const auto bad = parse("struct S {}; struct D : shared_state<S> {}; "
                         "struct H { H(State<D>); void pre(const Call&) {} "
                         "};\n#define payload corrupt\n");
  const auto rejected = tracer::generate_context_owner(*bad);
  require(!rejected.error.empty() && rejected.states.empty() &&
              rejected.compilation_copy.empty(),
          "macro-altered owner factories partially published");
}

int main(int argc, char **argv) {
  try {
    // Keep CTest's complete aggregate while allowing native-only failures to
    // be reproduced without rerunning unrelated Clang compilation checks.
    if (argc == 2) {
      const std::string selected = argv[1];
      if (selected == "--activation-only")
        verify_context_activation();
      else if (selected == "--generic-lineage-only")
        verify_context_generic_lineage();
      else if (selected == "--nested-live-only")
        verify_context_nested_live();
      else if (selected == "--hooks-only")
        verify_context_hooks();
      else if (selected == "--schema-only")
        verify_context_schema();
      else if (selected == "--stdlib-only")
        verify_context_stdlib();
      else if (selected == "--runtime-only")
        verify_context_runtime();
      else
        throw std::runtime_error("unknown focused context check");
      return 0;
    }
    verify_context_stdlib();
    verify_context_binding();
    verify_context_hooks();
    verify_context_facade();
    verify_context_runtime();
    verify_context_activation();
    verify_context_generic_lineage();
    verify_context_support_effects();
    verify_context_schema();
    tracer::context_engine::test::verify_context_capture();
    tracer::test::verify_context_registry();
    verify_context_delivery();
    verify_context_collector();
    verify_generated_owner();
    verify_owner_rendering();
    verify_generated_lifecycle();
    verify_lifecycle_rejections();
    std::ifstream file(TRACER_TEST_SOURCE_DIR
                       "/tests/fixtures/shared_context_nested.cpp");
    require(static_cast<bool>(file), "missing unchanged acceptance source");
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    const auto nested = analyze(source);
    require(nested.requires_context && nested.error.empty() &&
                nested.classes.size() == 1,
            "unchanged nested acceptance source was not discovered");
    const auto &interceptor = nested.classes.front();
    require(interceptor.type == "demo::RequestInterceptor" &&
                interceptor.dependencies.size() == 1,
            "namespaced constructor dependency was not discovered");
    const auto &state = interceptor.dependencies.front();
    require(state.state_type == "demo::RequestState" &&
                state.scope_type == "demo::RequestScope" && state.shared,
            "state marker contract differs");
    require(interceptor.hooks.size() == 2 &&
                interceptor.hooks[0].injected.size() == 1 &&
                interceptor.hooks[1].events &&
                interceptor.hooks[1].injected == interceptor.hooks[0].injected,
            "injection/snapshot requirements were not discovered");
    require(::tracer::compat::starts_with(tracer::context_activation_error(nested),
                                          "unsupported_state_support_profile:"),
            "unvalidated state support was admitted");

    const auto ordinary =
        analyze("struct Interceptor { void pre(const Call&) {} }; ");
    require(!ordinary.requires_context &&
                tracer::context_activation_error(ordinary).empty(),
            "ordinary callback was rejected by state admission");
    const auto legacy_nonvoid = analyze(
        "struct Interceptor { long long pre(const Call&) { return 7; } };");
    require(!legacy_nonvoid.requires_context &&
                tracer::context_activation_error(legacy_nonvoid).empty(),
            "legacy non-void PRE was rejected by context admission");
    const auto context_only =
        analyze("struct Interceptor { void post(const Call& c) { "
                "c.context().event(\"inherited\"); } }; ");
    require(context_only.requires_context &&
                !tracer::context_activation_error(context_only).empty(),
            "context-only consumer bypassed activation checks");
    const auto mixed = analyze(R"CPP(
      namespace example {
      struct Scope {};
      struct Data : shared_state<Scope> { int value = 10; };
      struct Hook {
        Hook(State<Data>);
        void pre(const Call& call, Data& data) {
          data.value += 1;
          call.context().get<Data>().value += 20;
          call.context().with<Data>([](auto& value) { value.value += 11; });
        }
      };
      }
    )CPP");
    require(mixed.error.empty() && mixed.classes.size() == 1 &&
                mixed.classes[0].hooks[0].direct.size() == 1 &&
                mixed.classes[0].hooks[0].visitors ==
                    mixed.classes[0].hooks[0].direct,
            "mixed access requirements differ");
    const auto invalid =
        analyze("struct Data {}; struct Hook { Hook(State<Data>); void "
                "pre(const Call&, Data&) {} }; ");
    require(::tracer::compat::starts_with(invalid.error, "invalid_state_marker:"),
            "unmarked state was accepted");
    const auto mismatch =
        analyze("struct Scope {}; struct Data : state<Scope> {}; struct Other "
                "{}; struct Hook { "
                "Hook(State<Data>); void pre(const Call&, Other&) {} }; ");
    require(::tracer::compat::starts_with(mismatch.error, "invalid_state_injection:"),
            "missing dependency was accepted");
    const auto body =
        analyze("struct Scope {}; struct Data : state<Scope> {}; struct Hook { "
                "Hook(State<Data>) {} void pre(const Call&, Data&) {} }; ");
    require(::tracer::compat::starts_with(body.error, "invalid_state_constructor:"),
            "user constructor body was treated as generated");
    std::cout << "7 Clang context declaration/admission checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

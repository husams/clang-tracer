#include <fstream>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "context_analysis.hpp"
#include "context_hooks.hpp"
#include "context_lifecycle.hpp"
#include "context_test_config.hpp"

void verify_context_hooks() {
  std::ifstream file(TRACER_TEST_SOURCE_DIR "/tests/fixtures/context_hooks_reachable.cpp");
  const std::string fixture{std::istreambuf_iterator<char>(file), {}};
  std::vector<std::string> args{"-std=c++20", "-I" TRACER_TEST_SOURCE_DIR "/include",
      "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  args.insert(args.end(), std::begin(kContextSystemArguments),
              std::end(kContextSystemArguments));
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + fixture, args, "context-hooks.cc");
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("context hook fixture did not parse");
  auto analysis = tracer::analyze_context_declarations(ast->getASTContext());
  if (!analysis.error.empty() || analysis.classes.size() != 2)
    throw std::runtime_error("context-only and constructor-backed hooks were not planned: " +
                             analysis.error + " classes=" +
                             std::to_string(analysis.classes.size()));
  const auto class_plan = [&](const char *name) -> const tracer::ContextClassPlan& {
    for (const auto &candidate : analysis.classes)
      if (candidate.type == name) return candidate;
    throw std::runtime_error("missing expected hook class plan");
  };
  const auto &direct = class_plan("HookInterceptor").hooks.front();
  if (!direct.direct_frame || !direct.all_visible_events || !direct.events ||
      direct.direct.size() != 1 || direct.direct.front() != "HookState")
    throw std::runtime_error("transitive direct/event access plan is incomplete");
  const auto &short_hook = class_plan("ContextOnly").hooks.front();
  if (short_hook.direct_frame || short_hook.visitors.size() != 1 ||
      short_hook.visitors.front() != "HookState")
    throw std::runtime_error("context-only visitor hook was not planned");

  const std::string event_only_source =
      "struct ContextEmitter { void pre(const Call& call) { "
      "call.context().event(\"visible\"); } };";
  auto event_only_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + event_only_source, args,
      "context-event-only.cc");
  if (!event_only_ast || event_only_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("event-only hook source did not parse");
  const auto event_only_plan = tracer::analyze_context_declarations(
      event_only_ast->getASTContext());
  if (!event_only_plan.error.empty() || event_only_plan.classes.size() != 1 ||
      event_only_plan.classes.front().hooks.size() != 1 ||
      event_only_plan.classes.front().hooks.front().direct_frame ||
      !event_only_plan.classes.front().hooks.front().all_visible_events)
    throw std::runtime_error("event-only hook did not retain short snapshot planning");
  const auto event_only_lowered = tracer::generate_context_hooks(*event_only_ast);
  if (!event_only_lowered.error.empty() ||
      event_only_lowered.compilation_copy.find("run_short_hook(call, true") ==
          std::string::npos)
    throw std::runtime_error("short hook lowering omitted all-visible event planning");

  const std::string stack_source =
      "struct StackScope {}; struct StackState : shared_state<StackScope> {}; "
      "struct StackEmitter { StackEmitter(State<StackState>); "
      "void helper(const Call& c) { c.capture_stack(); } "
      "void pre(const Call& c) { helper(c); } };";
  auto stack_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + stack_source, args,
      "context-stack-capture.cc");
  if (!stack_ast || stack_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("stack capture hook source did not parse");
  const auto stack_plan = tracer::analyze_context_declarations(
      stack_ast->getASTContext());
  const auto stack_lowered = tracer::generate_context_hooks(*stack_ast);
  if (!stack_plan.error.empty() || stack_plan.classes.size() != 1 ||
      !stack_plan.classes.front().hooks.front().captures_stack ||
      !stack_plan.requires_stack_capture ||
      stack_lowered.hooks.size() != 1 ||
      !stack_lowered.hooks.front().captures_stack)
    throw std::runtime_error(
        "reachable capture_stack use was not exposed in hook metadata: error=" +
        stack_plan.error + ", classes=" +
        std::to_string(stack_plan.classes.size()) + ", hooks=" +
        (stack_plan.classes.empty() ? std::string("0") :
         std::to_string(stack_plan.classes.front().hooks.size())) +
        ", plan=" + (stack_plan.classes.empty() ||
            stack_plan.classes.front().hooks.empty() ? std::string("missing") :
            std::to_string(stack_plan.classes.front().hooks.front().captures_stack)) +
        ", generated=" + std::to_string(stack_lowered.hooks.size()) +
        (stack_lowered.hooks.empty() ? std::string() : ", generated flag=" +
            std::to_string(stack_lowered.hooks.front().captures_stack)));

  const std::string state_constructor_stack_source = R"CPP(
    struct ConstructorScope {};
    struct ConstructorState : shared_state<ConstructorScope> {
      void helper(const Call& call) { (void)call.capture_stack(); }
      explicit ConstructorState(const ScopeInit& init) {
        helper(init.root_call());
      }
    };
    struct ConstructorStackHook {
      explicit ConstructorStackHook(State<ConstructorState>);
      void pre(const Call&) {}
    };
  )CPP";
  auto state_constructor_stack_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + state_constructor_stack_source,
      args, "context-state-constructor-stack.cc");
  if (!state_constructor_stack_ast ||
      state_constructor_stack_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("state constructor stack fixture did not parse");
  const auto state_constructor_stack_plan = tracer::analyze_context_declarations(
      state_constructor_stack_ast->getASTContext());
  if (!state_constructor_stack_plan.error.empty() ||
      !state_constructor_stack_plan.requires_stack_capture ||
      state_constructor_stack_plan.classes.size() != 1 ||
      state_constructor_stack_plan.classes.front().hooks.size() != 1 ||
      state_constructor_stack_plan.classes.front().hooks.front().captures_stack)
    throw std::runtime_error(
        "state constructor stack use was not exposed analysis-wide");

  const std::string generic_constructor_stack_source = R"CPP(
    struct GenericConstructorScope {};
    struct GenericStackPayload {
      void helper(const Call& call) { (void)call.capture_stack(); }
      explicit GenericStackPayload(const Call& call) { helper(call); }
    };
    struct GenericConstructorStackHook {
      void pre(const Call& call) {
        call.context().init_scope<GenericConstructorScope>(
            GenericStackPayload(call));
      }
    };
  )CPP";
  auto generic_constructor_stack_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" +
          generic_constructor_stack_source,
      args, "context-generic-constructor-stack.cc");
  if (!generic_constructor_stack_ast ||
      generic_constructor_stack_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("generic payload stack fixture did not parse");
  const auto generic_constructor_stack_plan =
      tracer::analyze_context_declarations(
          generic_constructor_stack_ast->getASTContext());
  std::string generic_stack_diagnostic;
  for (const auto &class_plan : generic_constructor_stack_plan.classes) {
    generic_stack_diagnostic += " class=" + class_plan.type;
    for (const auto &hook : class_plan.hooks) {
      generic_stack_diagnostic += " hook=" + hook.method +
          " stack=" + std::to_string(hook.captures_stack);
      for (const auto &initializer : hook.generic_scope_initializers)
        generic_stack_diagnostic += " payload=" + initializer.payload_type +
            " scope=" + initializer.scope_type;
    }
  }
  if (!generic_constructor_stack_plan.error.empty() ||
      !generic_constructor_stack_plan.requires_stack_capture ||
      generic_constructor_stack_plan.classes.size() != 1 ||
      generic_constructor_stack_plan.classes.front().hooks.size() != 1 ||
      !generic_constructor_stack_plan.classes.front().hooks.front().captures_stack)
    throw std::runtime_error(
        "generic payload constructor stack use was not exposed analysis-wide: " +
        generic_constructor_stack_plan.error + generic_stack_diagnostic +
        " requires=" + std::to_string(
            generic_constructor_stack_plan.requires_stack_capture));

  const std::string visitor_then_event_source =
      "struct VisitorScope {}; struct VisitorState : "
      "shared_state<VisitorScope> {}; "
      "struct VisitorEmitter { void pre(const Call& call) { "
      "call.context().with<VisitorState>([](VisitorState&) {}); "
      "call.context().event(\"after visitor\"); } };";
  auto visitor_then_event_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + visitor_then_event_source,
      args, "context-visitor-then-event.cc");
  if (!visitor_then_event_ast ||
      visitor_then_event_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("visitor-then-event hook source did not parse");
  const auto visitor_then_event_plan = tracer::analyze_context_declarations(
      visitor_then_event_ast->getASTContext());
  if (!visitor_then_event_plan.error.empty() ||
      visitor_then_event_plan.classes.size() != 1 ||
      visitor_then_event_plan.classes.front().hooks.front().direct_frame ||
      visitor_then_event_plan.classes.front().hooks.front().visitors.size() != 1 ||
      !visitor_then_event_plan.classes.front().hooks.front().all_visible_events)
    throw std::runtime_error("visitor-then-event hook lost its short snapshot plan");
  const auto visitor_then_event_lowered =
      tracer::generate_context_hooks(*visitor_then_event_ast);
  if (!visitor_then_event_lowered.error.empty() ||
      visitor_then_event_lowered.compilation_copy.find(
          "run_short_hook(call, true") == std::string::npos)
    throw std::runtime_error("visitor-then-event source did not lower to a short hook");

  const auto original = ast->getSourceManager()
      .getBufferData(ast->getSourceManager().getMainFileID()).str();
  auto generated = tracer::generate_context_hooks(*ast);
  if (!generated.error.empty() || generated.compilation_copy == original ||
      generated.hooks.size() != 2 ||
      generated.compilation_copy.find("__tracer_invoke_pre") == std::string::npos ||
      generated.compilation_copy.find("run_direct_hook") == std::string::npos ||
      generated.compilation_copy.find("run_short_hook") == std::string::npos)
    throw std::runtime_error("source lowering did not emit expected hook entrypoints");
  if (ast->getSourceManager().getBufferData(ast->getSourceManager().getMainFileID()) != original)
    throw std::runtime_error("hook source lowering modified the original source buffer");

  const auto lifecycle = tracer::generate_context_lifecycle(*ast);
  if (!lifecycle.error.empty() || lifecycle.classes.size() != 2 ||
      lifecycle.compilation_copy.find("engine::InvocationFrame") ==
          std::string::npos ||
      lifecycle.compilation_copy.find("ContextOnly::ContextOnly()") ==
          std::string::npos ||
      lifecycle.compilation_copy.find("HookInterceptor::HookInterceptor(") ==
          std::string::npos ||
      lifecycle.compilation_copy.find("pop_all_and_restore()") ==
          std::string::npos ||
      lifecycle.compilation_copy.find("operator=(const HookInterceptor&) = delete") ==
          std::string::npos)
    throw std::runtime_error("context lifecycle source lowering was incomplete: " +
                             lifecycle.error);
  if (ast->getSourceManager().getBufferData(ast->getSourceManager().getMainFileID()) != original)
    throw std::runtime_error("lifecycle lowering modified the original source buffer");

  const auto reserved_hooks = tracer::generate_context_hooks(
      *ast, "__clang_tracer_context_test_0");
  const auto reserved_lifecycle = tracer::generate_context_lifecycle(
      *ast, "__clang_tracer_context_test_0");
  if (!reserved_hooks.error.empty() || !reserved_lifecycle.error.empty() ||
      reserved_hooks.facade_namespace != "__clang_tracer_context_test_0" ||
      reserved_lifecycle.facade_namespace != "__clang_tracer_context_test_0" ||
      reserved_hooks.compilation_copy.find(
          "::__clang_tracer_context_test_0::run_direct_hook") ==
          std::string::npos ||
      reserved_lifecycle.compilation_copy.find(
          "::__clang_tracer_context_test_0::InvocationFrame") ==
          std::string::npos ||
      reserved_hooks.compilation_copy.find("namespace engine { struct user_symbol {}; }") ==
          std::string::npos ||
      reserved_hooks.compilation_copy.find("void pre(const Call& call) {") ==
          std::string::npos)
    throw std::runtime_error("reserved facade lowering lost user source or did not apply");

  const auto rejected = [&](const std::string &body, const char *diagnostic) {
    const auto source = "struct S{}; struct D : shared_state<S>{}; struct H { "
        "H(State<D>); void helper(const Call& c) { c.context().event(\"x\"); } " +
        body + " };";
    auto invalid = clang::tooling::buildASTFromCodeWithArgs(
        "#include \"tracer/callback.hpp\"\n" + source, args,
        "context-hooks-rejected.cc");
    if (!invalid || invalid->getDiagnostics().hasErrorOccurred())
      throw std::runtime_error("negative context-hook fixture did not parse");
    const auto planned = tracer::analyze_context_declarations(invalid->getASTContext());
    if (planned.error.find(diagnostic) == std::string::npos)
      throw std::runtime_error("unsafe reachable hook behavior was not rejected");
  };
  rejected("void pre(const Call& c) { c.context().with<D>([](D& s) { return &s; }); }",
           "invalid_context_borrow");
  rejected("void pre(const Call& c) { c.context().with<D>([&](D& s) { helper(c); }); }",
           "invalid_context_borrow");
  rejected("void pre(const Call& c) { void (*f)() = nullptr; f(); }",
           "unsupported_context_effect");
  rejected("void pre(const Call& c) { int local = 0; static int* saved; "
           "saved = &local; }", "unsupported_context_effect");
  rejected("void pre(const Call& c) { (void)reinterpret_cast<int*>(1); }",
           "unsupported_context_effect");

  const std::string shorthand =
      "struct Scope{}; struct D : shared_state<Scope>{ int value=0; }; "
      "struct H { H(State<D> request); void pre(const Call& call) { state.value++; } };";
  auto parser_copy = tracer::prepare_context_hook_parse_copy(shorthand, args,
                                                              "shorthand.cc");
  if (!parser_copy.error.empty() || parser_copy.source.find("PLACEHOLDER_BEGIN") ==
                                        std::string::npos ||
      shorthand.find("PLACEHOLDER") != std::string::npos)
    throw std::runtime_error("shorthand speculative parser copy was not prepared");
  auto shorthand_ast = clang::tooling::buildASTFromCodeWithArgs(
      parser_copy.source, args, "shorthand-prepared.cc");
  if (!shorthand_ast || shorthand_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("prepared shorthand source did not parse cleanly");
  auto shorthand_plan = tracer::analyze_context_declarations(
      shorthand_ast->getASTContext());
  if (!shorthand_plan.error.empty())
    throw std::runtime_error("prepared shorthand source did not produce a plan: " +
                             shorthand_plan.error);
  auto lowered = tracer::generate_context_hooks(*shorthand_ast);
  if (!lowered.error.empty() ||
      lowered.compilation_copy.find("PLACEHOLDER") != std::string::npos ||
      lowered.compilation_copy.find("::engine::request<::D>()") == std::string::npos)
    throw std::runtime_error("shorthand placeholder was not lowered to guarded access: marker=" +
        std::to_string(lowered.compilation_copy.find("PLACEHOLDER")) + " request=" +
        std::to_string(lowered.compilation_copy.find("engine::request")) +
        " error=" + lowered.error);

  const std::string named_shorthand =
      "struct Scope{}; struct R : shared_state<Scope>{ int value=0; }; "
      "struct L : shared_state<Scope>{ int value=0; }; "
      "struct M { M(State<R> request, State<L> local); "
      "void helper(const Call& call) { state.request.value++; state.local.value++; } "
      "void pre(const Call& call) { helper(call); } };";
  auto named_copy = tracer::prepare_context_hook_parse_copy(
      named_shorthand, args, "named-shorthand.cc");
  if (!named_copy.error.empty())
    throw std::runtime_error("named multi-dependency shorthand was not prepared");
  auto named_ast = clang::tooling::buildASTFromCodeWithArgs(
      named_copy.source, args, "named-shorthand-prepared.cc");
  if (!named_ast || named_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("prepared multi-dependency shorthand did not parse");
  auto named_lowered = tracer::generate_context_hooks(*named_ast);
  if (!named_lowered.error.empty() ||
      named_lowered.compilation_copy.find("R& request;") == std::string::npos ||
      named_lowered.compilation_copy.find("L& local;") == std::string::npos ||
      named_lowered.compilation_copy.find("::engine::request<::R>()") ==
          std::string::npos ||
      named_lowered.compilation_copy.find("::engine::request<::L>()") ==
          std::string::npos ||
      named_lowered.compilation_copy.find("PLACEHOLDER") != std::string::npos)
    throw std::runtime_error("named request/local aliases were not lowered");

  const std::string inherited_source =
      "struct Scope {}; struct Base : shared_state<Scope> {}; "
      "struct Derived : Base { int value = 0; }; struct Other {}; "
      "struct Hook { Hook(State<Derived> state); "
      "void pre(const Call& call, Derived& value) { "
      "value.value++; } };";
  auto inherited_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + inherited_source, args,
      "inherited-state-injection.cc");
  if (!inherited_ast || inherited_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("inherited state injection fixture did not parse");
  const auto inherited = tracer::analyze_context_declarations(
      inherited_ast->getASTContext());
  const auto has_state = [&](const char *type) {
    return std::any_of(inherited.states.begin(), inherited.states.end(),
                       [&](const auto &state) { return state.state_type == type; });
  };
  if (!inherited.error.empty() || inherited.classes.empty() ||
      inherited.classes.front().hooks.front().injected.empty() ||
      !has_state("Base") || !has_state("Derived"))
    throw std::runtime_error("marked inherited injection or catalog was rejected: " +
        inherited.error + " classes=" + std::to_string(inherited.classes.size()) +
        " states=" + std::to_string(inherited.states.size()));
  const std::string unmarked_source =
      "struct Scope {}; struct Data : state<Scope> {}; struct Other {}; "
      "struct Hook { void pre(const Call& call, Other& value) {} };";
  auto unmarked_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + unmarked_source, args,
      "unmarked-state-injection.cc");
  if (!unmarked_ast || unmarked_ast->getDiagnostics().hasErrorOccurred() ||
      !tracer::analyze_context_declarations(unmarked_ast->getASTContext())
           .error.starts_with("invalid_state_injection:"))
    throw std::runtime_error("unmarked typed injection was accepted");
  const std::string unbound_marked_source =
      "struct Scope {}; struct Data : shared_state<Scope> {}; "
      "struct Hook { void pre(const Call& call, Data& value) {} };";
  auto unbound_marked_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + unbound_marked_source,
      args, "unbound-marked-state-injection.cc");
  if (!unbound_marked_ast ||
      unbound_marked_ast->getDiagnostics().hasErrorOccurred() ||
      !tracer::analyze_context_declarations(unbound_marked_ast->getASTContext())
           .error.starts_with("invalid_state_injection:"))
    throw std::runtime_error(
        "marked typed injection without a matching dependency was accepted");
  auto scalar_state_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n"
      "struct Hook { Hook(State<int>); void pre(const Call& call) {} };",
      args, "scalar-state-marker-rejected.cc");
  if (!scalar_state_ast || scalar_state_ast->getDiagnostics().hasErrorOccurred() ||
      !tracer::analyze_context_declarations(scalar_state_ast->getASTContext())
           .error.starts_with("invalid_state_marker:"))
    throw std::runtime_error("State<int> was accepted as a marked state");

  const std::string generic_payload_source = R"CPP(
    #include <string>
    struct PayloadScope {};
    template<class T> struct payload_demo { T value{}; };
    struct PayloadRecord { int field = 7; };
    struct PayloadHook {
      void pre(const Call& call) {
        call.context().init_scope<PayloadScope>(payload_demo<int>{});
        call.context().with<PayloadScope, payload_demo<int>>(
            [](payload_demo<int>& value) { ++value.value; });
        call.context().with<PayloadScope, std::string>(
            [](std::string& value) { value += "!"; });
      }
    };
    struct GenericRoot {
      void pre(const Call& call) {
        call.context().get<PayloadScope, std::string>() += "root";
        (void)call.context().get<PayloadScope, std::string>().size();
        (void)call.context().get<PayloadScope, PayloadRecord>().field;
      }
    };
    struct Unrelated {
      void use(const Call& call) {
        (void)call.context().get<PayloadScope, payload_demo<int>>();
      }
    };
  )CPP";
  auto generic_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + generic_payload_source, args,
      "generic-context-payload.cc");
  if (!generic_ast || generic_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("generic payload fixture did not parse");
  const auto generic_plan = tracer::analyze_context_declarations(
      generic_ast->getASTContext());
  if (!generic_plan.error.empty() || generic_plan.states.size() < 2)
    throw std::runtime_error("generic payload family plan was rejected: " +
                             generic_plan.error);
  bool found_init = false, found_visitor = false, found_string_root = false;
  bool found_demo_payload = false;
  std::string generic_payload_debug;
  for (const auto &access : generic_plan.states)
    if (access.generic)
      generic_payload_debug += " [" + access.scope_type + "," +
                               access.payload_type + "]";
  for (const auto &class_plan : generic_plan.classes)
    for (const auto &hook : class_plan.hooks) {
      for (const auto &access : hook.generic_scope_initializers)
        found_init |= access.scope_type == "PayloadScope" &&
                      access.payload_type.find("payload_demo<int>") !=
                          std::string::npos;
      for (const auto &access : generic_plan.states)
        found_demo_payload |= access.generic &&
            access.scope_type == "PayloadScope" &&
            access.payload_type.find("payload_demo<int>") != std::string::npos;
      found_visitor |= !hook.generic_visitors.empty();
      for (const auto &access : hook.generic_direct)
        found_string_root |= access.scope_type == "PayloadScope" &&
            access.payload_type.find("string") != std::string::npos;
    }
  if (!found_init || !found_visitor || !found_string_root || !found_demo_payload)
    throw std::runtime_error("generic init/with/root access plans are incomplete: init=" +
        std::to_string(found_init) + " with=" + std::to_string(found_visitor) +
        " string=" + std::to_string(found_string_root) + generic_payload_debug);
  const auto generic_lowered = tracer::generate_context_hooks(*generic_ast);
  const bool generic_request = generic_lowered.compilation_copy.find(
      "request_payload<") != std::string::npos;
  const bool generic_plan_entry = generic_lowered.compilation_copy.find(
      "PayloadAccess<") != std::string::npos;
  const auto generic_root_pos = generic_lowered.compilation_copy.find(
      "GenericRoot::__tracer_invoke_pre");
  const bool untouched_unrelated = generic_lowered.compilation_copy.find(
      "(void)call.context().get<PayloadScope, payload_demo<int>>()") !=
      std::string::npos;
  if (!generic_lowered.error.empty() || !generic_request ||
      !generic_plan_entry || !untouched_unrelated) {
    const auto detail = generic_root_pos == std::string::npos
        ? generic_lowered.compilation_copy.substr(0, 900)
        : generic_lowered.compilation_copy.substr(generic_root_pos, 900);
    throw std::runtime_error(
        "generic root was not lowered through payload facade: error=" +
        generic_lowered.error + ", request=" +
        std::to_string(generic_request) + ", plan=" +
        std::to_string(generic_plan_entry) + ", unrelated=" +
        std::to_string(untouched_unrelated) + ", root_pos=" +
        std::to_string(generic_root_pos) + ", source=" + detail);
  }
  const std::string record_access =
      "(*::engine::request_payload<::PayloadScope, ::PayloadRecord>()).field";
  const std::string string_suffix = ").size()";
  const auto string_end =
      generic_lowered.compilation_copy.find(string_suffix);
  const auto string_begin = string_end == std::string::npos
      ? std::string::npos
      : generic_lowered.compilation_copy.rfind(
            "(*::engine::request_payload<", string_end);
  const std::string string_access =
      string_begin == std::string::npos || string_end == std::string::npos
          ? std::string{}
          : generic_lowered.compilation_copy.substr(
                string_begin, string_end + string_suffix.size() - string_begin);
  if (generic_lowered.compilation_copy.find(record_access) ==
          std::string::npos || string_access.empty()) {
    const auto root_pos = generic_lowered.compilation_copy.find(
        "struct GenericRoot");
    throw std::runtime_error(
        "generic get member expressions were not parenthesized in generated C++: " +
        (root_pos == std::string::npos
             ? generic_lowered.compilation_copy.substr(0, 1000)
             : generic_lowered.compilation_copy.substr(root_pos, 1200)));
  }

  const std::string generic_expression_source =
      "#include <string>\n"
      "struct PayloadScope {}; struct PayloadRecord { int field; };\n"
      "namespace engine { template<class S, class P> P* request_payload(); }\n"
      "int record_access() { return " + record_access + "; }\n"
      "unsigned long string_access() { return " + string_access + "; }\n";
  auto generic_expression_ast = clang::tooling::buildASTFromCodeWithArgs(
      generic_expression_source, args, "generated-generic-get-expression.cc");
  if (!generic_expression_ast ||
      generic_expression_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error(
        "generated generic get field/size expressions did not compile");
  const std::string const_payload_source = R"CPP(
    #include <string>
    struct NumberScope {};
    struct TextScope {};
    struct ConstPayloadHook {
      void pre(const Call& call) {
        const int number = 17;
        const std::string text = "payload";
        call.context().init_scope<NumberScope>(number);
        call.context().init_scope<TextScope>(text);
      }
      void post(const Call& call) {
        (void)call.context().get<NumberScope, int>();
        (void)call.context().get<TextScope, std::string>();
      }
    };
  )CPP";
  auto const_payload_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + const_payload_source, args,
      "const-generic-context-payload.cc");
  if (!const_payload_ast || const_payload_ast->getDiagnostics().hasErrorOccurred())
    throw std::runtime_error("const generic payload fixture did not parse");
  const auto const_payload_plan = tracer::analyze_context_declarations(
      const_payload_ast->getASTContext());
  if (!const_payload_plan.error.empty())
    throw std::runtime_error("const generic payloads were rejected: " +
                             const_payload_plan.error);
  const auto find_payload = [&](const std::string &scope, bool initializer) {
    std::string result;
    for (const auto &class_plan : const_payload_plan.classes)
      for (const auto &hook : class_plan.hooks) {
        const auto &accesses = initializer ? hook.generic_scope_initializers
                                           : hook.generic_direct;
        for (const auto &access : accesses)
          if (access.scope_type == scope) result = access.payload_type;
      }
    return result;
  };
  const auto number_init = find_payload("NumberScope", true);
  const auto number_get = find_payload("NumberScope", false);
  const auto text_init = find_payload("TextScope", true);
  const auto text_get = find_payload("TextScope", false);
  const auto const_payload_lowered =
      tracer::generate_context_hooks(*const_payload_ast);
  if (number_init != "int" || number_get != "int" ||
      text_init.find("string") == std::string::npos ||
      text_init != text_get || !const_payload_lowered.error.empty() ||
      const_payload_lowered.compilation_copy.find(
          "::engine::request_payload<::NumberScope, int>()") ==
          std::string::npos ||
      const_payload_lowered.compilation_copy.find(
          "::engine::request_payload<::TextScope,") == std::string::npos)
    throw std::runtime_error(
        "const lvalue payload init/get did not share normalized facade types");
  const std::string mixed_generic_source =
      "struct DynamicScope {}; struct Payload { int value = 0; }; "
      "struct Hook { void pre(const Call& call) { "
      "call.context().init_scope<DynamicScope>(Payload{}); "
      "call.context().get<DynamicScope, Payload>().value++; } };";
  auto mixed_generic_ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n" + mixed_generic_source, args,
      "generic-init-get-rejected.cc");
  if (!mixed_generic_ast || mixed_generic_ast->getDiagnostics().hasErrorOccurred() ||
      !tracer::analyze_context_declarations(mixed_generic_ast->getASTContext())
           .error.starts_with("invalid_context_access_plan:"))
    throw std::runtime_error("direct get after dynamic init_scope was admitted");

  const auto cleanup_plan = [&](const std::string &source) {
    auto cleanup_ast = clang::tooling::buildASTFromCodeWithArgs(
        "#include \"tracer/callback.hpp\"\n" + source, args,
        "hook-cleanup-effects.cc");
    if (!cleanup_ast || cleanup_ast->getDiagnostics().hasErrorOccurred())
      throw std::runtime_error("hook cleanup fixture did not parse");
    return tracer::analyze_context_declarations(cleanup_ast->getASTContext());
  };
  const auto destructor_event = cleanup_plan(
      "struct Cleanup { const Call& call; ~Cleanup() { "
      "call.context().event(\"cleanup\"); } }; "
      "struct Hook { void pre(const Call& call) { Cleanup cleanup{call}; } };");
  if (!destructor_event.error.empty() || destructor_event.classes.empty() ||
      !destructor_event.classes.front().hooks.front().events ||
      !destructor_event.classes.front().hooks.front().all_visible_events)
    throw std::runtime_error(
        "local destructor event was absent from hook plan: error=" +
        destructor_event.error + ", classes=" +
        std::to_string(destructor_event.classes.size()) + ", hooks=" +
        (destructor_event.classes.empty() ? std::string("0") :
         std::to_string(destructor_event.classes.front().hooks.size())) +
        ", events=" + (destructor_event.classes.empty() ||
            destructor_event.classes.front().hooks.empty() ? std::string("0") :
            std::to_string(destructor_event.classes.front().hooks.front().events)) +
        ", all_visible=" + (destructor_event.classes.empty() ||
            destructor_event.classes.front().hooks.empty() ? std::string("0") :
            std::to_string(destructor_event.classes.front().hooks.front().all_visible_events)));

  const auto destructor_get = cleanup_plan(
      "struct Scope {}; struct Data : shared_state<Scope> { int value=0; }; "
      "struct Cleanup { const Call& call; ~Cleanup() { "
      "call.context().get<Data>().value++; } }; "
      "struct Hook { void pre(const Call& call) { Cleanup cleanup{call}; } };");
  if (!destructor_get.error.empty() || destructor_get.classes.empty() ||
      destructor_get.classes.front().hooks.front().direct.empty() ||
      destructor_get.classes.front().hooks.front().direct.front() != "Data")
    throw std::runtime_error("local destructor state access was absent from plan");

  const auto visitor_cleanup_event = cleanup_plan(
      "struct Scope {}; struct Data : shared_state<Scope> {}; "
      "struct Cleanup { const Call& call; ~Cleanup() { "
      "call.context().event(\"visitor cleanup\"); } }; "
      "struct Hook { void pre(const Call& call) { "
      "call.context().with<Data>([&](Data&) { Cleanup cleanup{call}; }); } };");
  if (!visitor_cleanup_event.error.starts_with("invalid_context_borrow:"))
    throw std::runtime_error("visitor temporary destructor event was admitted");

  const auto visitor_scope_init = cleanup_plan(
      "struct Scope {}; struct Data : shared_state<Scope> {}; "
      "struct Hook { void pre(const Call& call) { "
      "call.context().with<Data>([&](Data&) { "
      "call.context().init_scope<Scope>(); }); } };");
  if (!visitor_scope_init.error.starts_with("invalid_context_access_plan:") ||
      visitor_scope_init.error.find("init_scope") == std::string::npos)
    throw std::runtime_error("init_scope inside a visitor lacked an explicit rejection");

  const auto container_cleanup = cleanup_plan(
      "#include <vector>\n"
      "struct Item { const Call& call; explicit Item(const Call& c):call(c) {} "
      "~Item() { call.context().event(\"item cleanup\"); } }; "
      "struct Hook { void pre(const Call& call) { std::vector<Item> items; "
      "items.emplace_back(call); items.clear(); } };");
  if (!container_cleanup.error.empty() || container_cleanup.classes.empty() ||
      !container_cleanup.classes.front().hooks.front().all_visible_events)
    throw std::runtime_error("container element destructor event was omitted");

  const auto comparator_effect = cleanup_plan(
      "#include <set>\n"
      "struct Scope {}; struct Data : shared_state<Scope> { int value=0; }; "
      "struct Key { const Call& call; explicit Key(const Call& c):call(c) {} "
      "bool operator<(const Key& other) const { "
      "call.context().get<Data>().value++; call.context().event(\"compare\"); "
      "return false; } }; "
      "struct Hook { void pre(const Call& call) { std::set<Key> keys; "
      "keys.insert(Key{call}); keys.insert(Key{call}); } };");
  if (!comparator_effect.error.empty() || comparator_effect.classes.empty() ||
      comparator_effect.classes.front().hooks.front().direct.empty() ||
      comparator_effect.classes.front().hooks.front().direct.front() != "Data" ||
      !comparator_effect.classes.front().hooks.front().all_visible_events)
    throw std::runtime_error("std::less user operator effects were omitted");

  const auto spoofed_std = cleanup_plan(
      "namespace std { struct FakeNative { void run() { "
      "void (*function)() = nullptr; function(); } }; } "
      "struct Hook { void pre(const Call& call) { "
      "call.context().event(\"visible\"); std::FakeNative{}.run(); } };");
  if (!spoofed_std.error.starts_with("unsupported_context_effect:") ||
      spoofed_std.error.find("protected standard namespace") ==
          std::string::npos)
    throw std::runtime_error("source std namespace spoof was not rejected: " +
                             spoofed_std.error);
}

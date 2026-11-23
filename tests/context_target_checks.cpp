#include <algorithm>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "context_analysis.hpp"
#include "context_hooks.hpp"
#include "context_target.hpp"
#include "context_test_config.hpp"

// A context hook can call a function of the target process: the analysis
// classifies the call, the hook lowering rewrites it into a call through the
// target-call bridge, and the generated target side performs it.
void verify_context_target_calls() {
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
  const auto fail = [](const std::string &message) {
    throw std::runtime_error("target call: " + message);
  };
  const auto has = [](const std::string &text, const std::string &part) {
    return text.find(part) != std::string::npos;
  };

  // `prefix` starts on line 4; with the two-line `item` prefix the hook is on
  // line 8.
  const auto analyze = [&](const std::string &body, const std::string &prefix) {
    const auto source =
        "#include \"tracer/callback.hpp\"\n#include <cstdio>\n#include <string>\n" +
        prefix +
        "\nstruct S{}; struct D : shared_state<S>{ int value = 0; };\n"
        "struct H { H(State<D>);\n" + body + " };";
    auto fixture = clang::tooling::buildASTFromCodeWithArgs(
        source, args, "context-target.cc");
    if (!fixture || fixture->getDiagnostics().hasErrorOccurred())
      throw std::runtime_error("target call fixture did not parse: " + body);
    return tracer::analyze_context_declarations(fixture->getASTContext());
  };
  const auto accepted = [&](const std::string &body, const std::string &prefix) {
    const auto planned = analyze(body, prefix);
    if (!planned.error.empty() || planned.classes.size() != 1 ||
        !planned.classes.front().hooks.front().target_calls)
      fail("a supported call was not planned: " + body + " => " + planned.error);
    return planned;
  };
  const auto rejected = [&](const std::string &body, const std::string &prefix,
                            const std::string &diagnostic) {
    const auto planned = analyze(body, prefix);
    if (!has(planned.error, diagnostic))
      fail("an unsupported call was not rejected: " + body + " => " +
           planned.error);
    if (has(planned.error, "has no visible body"))
      fail("a target call was reported as a missing body: " + planned.error);
    return planned;
  };
  const std::string item =
      "struct Item { virtual ~Item(); virtual int weight() const; "
      "int plain(); static int count(); int operator+(int) const; };\n"
      "int external(int); Item* first(); int defaulted(int = 3);";
  const std::string event = "c.context().event(\"x\"); ";

  // Free functions, virtual and non-virtual methods, chained calls.
  {
    const auto planned = accepted(
        "void pre(const Call& c) { (void)external(1); " + event + "}", item);
    const auto &hook = planned.classes.front().hooks.front();
    if (hook.target_call != "external" || hook.target_call_line != 8 ||
        hook.target_call_file != "context-target.cc" || hook.direct_frame)
      fail("the first target call was not recorded");
  }
  accepted("void pre(const Call& c) { (void)first()->weight(); "
           "(void)(*first()).plain(); (void)Item::count(); " + event + "}",
           item);
  // The state is reached through a visitor after the call.
  accepted("void pre(const Call& c) { const int v = external(1); "
           "c.context().with<D>([&](D& s) { s.value = v; }); }", item);
  // An inline or static helper is compiled only into the hook's module.
  accepted("void pre(const Call& c) { (void)twice(2); " + event + "}",
           item + " inline int twice(int v) { return external(v) * 2; }");
  accepted("void pre(const Call& c) { (void)twice(2); " + event + "}",
           item + " static int twice(int v) { return external(v) * 2; }");

  // Target code never runs under a context lock.
  {
    const auto direct = rejected(
        "void pre(const Call& c) { c.context().get<D>().value = external(1); }",
        item,
        "invalid_context_access_plan: target function external cannot be "
        "called in a hook that holds a direct access frame");
    if (direct.error_line != 8 || !direct.error_column ||
        !has(direct.error, " at context-target.cc:8:"))
      fail("the direct-frame rejection has no location: " + direct.error);
  }
  rejected("void pre(const Call& c, D& injected) { injected.value = "
           "external(1); }", item,
           "invalid_context_access_plan: target function external cannot be "
           "called in a hook that holds a direct access frame");
  rejected("void pre(const Call& c) { c.context().with<D>([&](D& s) { "
           "s.value = external(1); }); }", item,
           "invalid_context_access_plan: target function external is called "
           "inside a with() visitor");

  // Calls the bridge cannot carry name the function, the reason and the place.
  {
    const auto by_value = rejected(
        "void pre(const Call& c) { take(Big{}); " + event + "}",
        item + " struct Big { int a[8]; }; void take(Big);",
        "unsupported_context_effect: call to target function take: it takes "
        "a by-value parameter of type Big (parameter 1)");
    if (by_value.error_line != 8 ||
        !has(by_value.error, " at context-target.cc:8:"))
      fail("the rejection has no location: " + by_value.error);
  }
  rejected("void pre(const Call& c) { (void)make(); " + event + "}",
           item + " struct Big { int a[8]; }; Big make();",
           "call to target function make: it returns a value of type Big");
  rejected("void pre(const Call& c) { (void)defaulted(); " + event + "}", item,
           "call to target function defaulted: argument 1 uses its default "
           "value");
  rejected("void pre(const Call& c) { (void)(*first() + 1); " + event + "}",
           item, "call to target function Item::operator+: an overloaded "
                 "operator");
  rejected("void pre(const Call& c) { (void)first()->count(); " + event + "}",
           item, "call to target function Item::count: a static member "
                 "function has to be called by its qualified name");
  rejected("void pre(const Call& c) { variadic(1, 2); " + event + "}",
           item + " void variadic(int, ...);",
           "call to target function variadic: it takes a variable argument "
           "list");
  rejected("void pre(const Call& c) { (void)EXTERNAL; " + event + "}",
           item + "\n#define EXTERNAL external(1)",
           "call to target function external: the call is not written "
           "directly in the callback source");
  rejected("void pre(const Call& c) { (void)twice(2); " + event + "}",
           item + " int twice(int v) { return external(v) * 2; }",
           "call to target function external: it is called from twice, which "
           "is neither inline nor static");
  rejected("void pre(const Call& c) { (void)twice<int>(2); " + event + "}",
           item + " template <class T> T twice(T v) { return external(v) * 2; }",
           "call to target function external: it is called from a template");

  // The target must not modify an object that lives in the hook's runtime.
  const std::string writers =
      item + " void fill(std::string& out); void read(const std::string& in); "
             "void count(int* out); void edit(Item* item);";
  rejected("void pre(const Call& c) { std::string text; fill(text); " + event + "}",
           writers, "call to target function fill: argument 1 is an object "
                    "the hook built, passed for the target to modify");
  rejected("void pre(const Call& c) { Item copy(*first()); edit(&copy); " + event + "}",
           writers, "call to target function edit: argument 1 is an object "
                    "the hook built");
  rejected("void pre(const Call& c) { Item copy(*first()); (void)copy.plain(); " +
               event + "}",
           writers, "call to target function Item::plain: a member function "
                    "that is not const is called on an object the hook built");
  accepted("void pre(const Call& c) { std::string text; read(text); int n = 0; "
           "count(&n); edit(first()); Item* p = first(); (void)p->plain(); "
           "Item& r = *first(); (void)r.plain(); " + event + "}",
           writers);

  // A function of the C library or the standard library is not a target
  // function: its declaration is in the toolchain headers.
  {
    const auto planned =
        analyze("void pre(const Call& c) { std::puts(\"x\"); " + event + "}", item);
    if (!has(planned.error, "unsupported_context_effect: ") ||
        has(planned.error, "target function"))
      fail("a C library call was treated as a target call: " + planned.error);
  }

  // Lowering: every call goes through one bridge per target function.
  const std::string lowering_source =
      "#include \"tracer/callback.hpp\"\n#include <string>\n#include <vector>\n"
      "namespace demo {\n"
      "enum class Kind : short { plain, fancy };\n"
      "struct Item {\n"
      "  virtual ~Item();\n"
      "  virtual std::string serialize() const;\n"
      "  virtual int weight() const;\n"
      "  const std::string& name() const;\n"
      "  Item* next();\n"
      "  void rename(const char* value, int count);\n"
      "};\n"
      "std::vector<Item*>& registry();\n"
      "int total(const std::vector<Item*>& items);\n"
      "Kind kind_of(const Item& item);\n"
      "double ratio(double value, bool flag, Kind kind);\n"
      "}\n"
      "struct TargetScope {};\n"
      "struct TargetPayload { std::vector<std::string> items; "
      "std::vector<int> numbers; };\n"
      "struct TargetHook {\n"
      "  void post(const Call& call) {\n"
      "    TargetPayload payload;\n"
      "    for (demo::Item* item : demo::registry()) {\n"
      "      payload.items.push_back(item->serialize());\n"
      "      payload.items.push_back(item->next()->name());\n"
      "      payload.numbers.push_back((*item).weight());\n"
      "      item->rename(\"x\", 2);\n"
      "    }\n"
      "    payload.numbers.push_back(demo::total(demo::registry()));\n"
      "    payload.numbers.push_back(static_cast<int>(\n"
      "        demo::kind_of(*demo::registry().front())));\n"
      "    (void)demo::ratio(1.5, true, demo::Kind::fancy);\n"
      "    call.context().init_scope<TargetScope>(payload);\n"
      "    call.context().event(\"target\");\n"
      "  }\n"
      "};\n";
  auto lowering_ast = clang::tooling::buildASTFromCodeWithArgs(
      lowering_source, args, "context-target-lowering.cc");
  if (!lowering_ast || lowering_ast->getDiagnostics().hasErrorOccurred())
    fail("the lowering fixture did not parse");
  const auto lowering_plan =
      tracer::analyze_context_declarations(lowering_ast->getASTContext());
  if (!lowering_plan.error.empty() || lowering_plan.classes.size() != 1 ||
      !lowering_plan.classes.front().hooks.front().target_calls)
    fail("the lowering fixture was not planned: " + lowering_plan.error);
  const auto lowered =
      tracer::generate_context_hooks(*lowering_ast, "tracer::context_facade");
  if (!lowered.error.empty()) fail("lowering failed: " + lowered.error);
  const auto &bridges = lowered.target_bridges;
  const auto bridge = [&](const std::string &name) -> const tracer::ContextTargetBridge & {
    const auto found = std::find_if(
        bridges.begin(), bridges.end(),
        [&](const auto &candidate) { return candidate.name == name; });
    if (found == bridges.end()) fail("no bridge for " + name);
    return *found;
  };
  if (bridges.size() != 9)
    fail("expected one bridge per target function, got " +
         std::to_string(bridges.size()));
  const auto &serialize = bridge("demo::Item::serialize");
  const auto &weight = bridge("demo::Item::weight");
  const auto &name = bridge("demo::Item::name");
  const auto &rename = bridge("demo::Item::rename");
  const auto &total = bridge("demo::total");
  const auto &kind_of = bridge("demo::kind_of");
  const auto &ratio = bridge("demo::ratio");
  // The virtual destructor takes the first two slots of the Itanium table.
  if (!serialize.virtual_dispatch || serialize.vtable_index != 2 ||
      serialize.result != tracer::ContextTargetValue::text ||
      !serialize.member || serialize.parameters.size() != 1 ||
      serialize.parameters.front().type != "const ::demo::Item &" ||
      serialize.parameters.front().erased != "const void*")
    fail("virtual std::string method was not described: " + serialize.key());
  if (!weight.virtual_dispatch || weight.vtable_index != 3 ||
      weight.result != tracer::ContextTargetValue::scalar ||
      weight.result_erased != "int")
    fail("virtual int method was not described");
  if (name.virtual_dispatch || name.symbol != "_ZNK4demo4Item4nameB5cxx11Ev" ||
      name.result != tracer::ContextTargetValue::address)
    fail("non-virtual reference method was not described: " + name.symbol);
  if (rename.result != tracer::ContextTargetValue::none ||
      rename.parameters.size() != 3 ||
      rename.parameters[0].type != "::demo::Item &" ||
      rename.parameters[1].erased != "const void*" ||
      rename.parameters[2].erased != "int")
    fail("void method with arguments was not described");
  if (total.member || total.symbol != "_ZN4demo5totalERKSt6vectorIPNS_4ItemESaIS2_EE" ||
      total.result_erased != "int")
    fail("free function was not described: " + total.symbol);
  if (kind_of.result != tracer::ContextTargetValue::scalar ||
      kind_of.result_erased != "short" || kind_of.result_type != "::demo::Kind")
    fail("enumeration result was not described: " + kind_of.result_type);
  if (ratio.parameters.size() != 3 || ratio.parameters[0].erased != "double" ||
      ratio.parameters[1].erased != "bool" ||
      ratio.parameters[2].erased != "short" || ratio.result_erased != "double")
    fail("scalar parameters were not described");

  const auto &copy = lowered.compilation_copy;
  if (copy.rfind("extern \"C\" bool clangtracer_target_call_0(void*", 0) != 0)
    fail("the bridge declarations do not lead the lowered source");
  for (const char *left : {"item->serialize()", "item->next()", "(*item).weight()",
                           "item->rename(", "demo::total(", "demo::kind_of(",
                           "demo::ratio(", "demo::registry()"})
    if (has(copy, left)) fail(std::string("a call was not rewritten: ") + left);
  if (!has(copy, ">(*(item)))") ||                 // item->serialize()
      !has(copy, ">(((*item)))") ||                // (*item).weight()
      !has(copy, ">(*(item), \"x\", 2)") ||        // item->rename("x", 2)
      !has(copy, ">(*(::tracer::context_facade::target_call<") || // chained
      !has(copy, ">(1.5, true, demo::Kind::fancy)"))
    fail("the rewritten calls are not the expected ones:\n" + copy);
  auto relowered = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"context_facade.hpp\"\n" + copy, args,
      "context-target-lowered.cc");
  if (!relowered || relowered->getDiagnostics().hasErrorOccurred())
    fail("the lowered source does not compile:\n" + copy);

  // The target side: virtual dispatch by slot, direct calls by linkage name,
  // an exception barrier, and a std::string that stays on the target side.
  const auto thunks = tracer::context_target_thunks(bridges);
  const auto declarations = tracer::context_target_declarations(bridges);
  for (std::size_t index = 0; index < bridges.size(); ++index)
    if (!has(thunks, "extern \"C\" bool " +
                         tracer::context_target_bridge_symbol(index) + "(void* out") ||
        !has(declarations, "extern \"C\" bool " +
                               tracer::context_target_bridge_symbol(index) + "(void*"))
      fail("a bridge function is missing");
  if (!has(thunks, "(*static_cast<const Function* const*>(a0))[2](a0)") ||
      !has(thunks, "(*static_cast<const Function* const*>(a0))[3](a0)") ||
      !has(thunks, "__asm__(\"_ZN4demo5totalERKSt6vectorIPNS_4ItemESaIS2_EE\")") ||
      !has(thunks, "new std::string(") ||
      !has(thunks, "catch (...)") ||
      !has(thunks, "extern \"C\" void clangtracer_target_release_text(void* handle) noexcept") ||
      !has(thunks, "tracer_target_call_enter()"))
    fail("the target side is not the expected one:\n" + thunks);
  auto thunk_ast = clang::tooling::buildASTFromCodeWithArgs(
      thunks, args, "context-target-thunks.cc");
  if (!thunk_ast || thunk_ast->getDiagnostics().hasErrorOccurred())
    fail("the target side does not compile:\n" + thunks);
}

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "context_support_effects.hpp"
#include "context_test_config.hpp"
#include "tracer/compat.hpp"

void verify_context_support_effects() {
  std::vector<std::string> args{
      "-std=c++20", "-I" TRACER_TEST_SOURCE_DIR "/include",
      "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  args.insert(args.end(), std::begin(kContextSystemArguments),
              std::end(kContextSystemArguments));
  const auto validate = [&](const std::string &body,
                            const std::string &state_decl =
                                "struct Data : shared_state<Scope> {};",
                            const std::string &global_declarations = "",
                            const std::vector<std::string> &extra_arguments = {}) {
    auto fixture_arguments = args;
    fixture_arguments.insert(fixture_arguments.end(), extra_arguments.begin(),
                             extra_arguments.end());
    auto ast = clang::tooling::buildASTFromCodeWithArgs(
        "#include <exception>\n#include <cstdlib>\n"
        "#include <vector>\n#include <string>\n"
        "#include \"tracer/callback.hpp\"\n"
        "struct Scope {};\n" + global_declarations + "\n" + state_decl + " "
        "struct Hook { "
        "void catch_helper() { try { throw 1; } "
        "catch (...) { catch_helper(); } } "
        "void pre(const Call& call) { call.context().get<Data>(); " + body +
            " } };",
        fixture_arguments, "context-support-effects.cc");
    if (!ast || ast->getDiagnostics().hasErrorOccurred())
      throw std::runtime_error("support-effects AST fixture did not parse");
    return tracer::validate_context_support_effects(*ast);
  };
  if (!validate("try { call.context().get<Data>(); } catch (...) {} ").empty())
    throw std::runtime_error("ordinary caught context access was rejected");
  if (!::tracer::compat::starts_with(validate("struct Big { char bytes[769]; }; throw Big{};"),
                                     "context_exception_object_too_large:")) {
    throw std::runtime_error("oversized exception was admitted");
  }
  if (!::tracer::compat::starts_with(
          validate("struct alignas(32) Wide { char value; }; throw Wide{};"),
          "context_exception_alignment_exceeded:")) {
    throw std::runtime_error("over-aligned exception was admitted");
  }
  std::string nested;
  for (unsigned i = 0; i < 17; ++i)
    nested = "try { throw 1; } catch (...) { " + nested + " }";
  if (!::tracer::compat::starts_with(validate(nested), "context_exception_capacity_exceeded:")) {
    throw std::runtime_error("transitive catch-depth reserve bound was missed");
  }
  if (!::tracer::compat::starts_with(validate("std::terminate();"),
                                     "unsupported_context_effect:")) {
    throw std::runtime_error("direct std::terminate effect was admitted");
  }
  if (!::tracer::compat::starts_with(validate("", "struct Data : shared_state<Scope> { "
                                                  "Data() noexcept { std::vector<int> values; "
                                                  "values.push_back(1); } };"),
                                     "context_noexcept_function_effect:")) {
    throw std::runtime_error("allocating noexcept state constructor was admitted");
  }
  if (!::tracer::compat::starts_with(
          validate("helper();", "struct Data : shared_state<Scope> {};",
                   "void helper() noexcept { std::string value(128, 'x'); }"),
          "context_noexcept_function_effect:")) {
    throw std::runtime_error("allocating noexcept helper was admitted");
  }
  if (!::tracer::compat::starts_with(
          validate("helper();", "struct Data : shared_state<Scope> {};",
                   "int dynamic_value() { return 1; }\n"
                   "void helper() { static int value = dynamic_value(); }"),
          "context_dynamic_local_static_unsupported:")) {
    throw std::runtime_error("dynamically initialized local static was admitted");
  }
  if (!validate("", "struct Data : shared_state<Scope> { "
                    "Data() noexcept : value(1) {} int value; };").empty())
    throw std::runtime_error("proven nonallocating noexcept constructor was rejected");
  if (!::tracer::compat::starts_with(validate("call.context().get<Data>().items.emplace_back();",
                                              "struct Element { ~Element() noexcept { "
                                              "std::string huge(100000000, 'x'); } }; "
                                              "struct Data : shared_state<Scope> { "
                                              "std::vector<Element> items; };"),
                                     "context_noexcept_destructor_effect:")) {
    throw std::runtime_error("nested payload element destructor allocation was not rejected");
  }
  const auto system_header_directory =
      std::filesystem::temp_directory_path() /
      ("clang-tracer-system-payload-" + std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(system_header_directory);
  {
    std::ofstream header(system_header_directory / "payload.hpp");
    header << "#pragma GCC system_header\n"
              "struct Element { ~Element() noexcept { "
              "std::string huge(100000000, 'x'); } };\n";
    if (!header)
      throw std::runtime_error("could not write system-header payload fixture");
  }
  const auto system_payload_error = validate(
      "call.context().get<Data>().items.emplace_back();",
      "struct Data : shared_state<Scope> { std::vector<Element> items; };",
      "#include \"payload.hpp\"\n",
      {"-isystem", system_header_directory.string()});
  std::error_code remove_error;
  std::filesystem::remove_all(system_header_directory, remove_error);
  if (!::tracer::compat::starts_with(system_payload_error, "context_noexcept_destructor_effect:")) {
    throw std::runtime_error(
        "allocating noexcept payload destructor in -isystem header escaped validation: " +
        system_payload_error);
  }
  const auto system_hash_directory =
      std::filesystem::temp_directory_path() /
      ("clang-tracer-system-hash-" + std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(system_hash_directory);
  {
    std::ofstream header(system_hash_directory / "hash.hpp");
    header << "#include <cstddef>\n#include <functional>\n#include <unordered_set>\n"
              "namespace user_hash { struct Key { int value; }; }\n"
              "namespace std { template<> struct hash<user_hash::Key> { "
              "size_t operator()(const user_hash::Key&) const noexcept "
              "{ return 0; } }; }\n";
    if (!header)
      throw std::runtime_error("could not write -isystem hash fixture");
  }
  const auto system_hash_error = validate(
      "", "struct Data : shared_state<Scope> { "
          "std::unordered_set<user_hash::Key> keys; };",
      "#include \"hash.hpp\"\n",
      {"-isystem", system_hash_directory.string()});
  std::filesystem::remove_all(system_hash_directory, remove_error);
  if (!::tracer::compat::starts_with(system_hash_error,
                                     "unsupported_context_native_declaration:")) {
    throw std::runtime_error("user std::hash specialization in a -isystem project header was "
                             "admitted: " +
                             system_hash_error);
  }
  const auto owning_recursion = validate(
      "", "struct Data : shared_state<Scope> { "
          "void helper() { (void)new int; helper(); } "
          "Data() { helper(); } };");
  if (!::tracer::compat::starts_with(owning_recursion, "context_exception_capacity_unbounded:")) {
    throw std::runtime_error("recursive owning escape was admitted: " + owning_recursion);
  }
  if (!::tracer::compat::starts_with(validate("catch_helper();"),
                                     "context_exception_capacity_unbounded:")) {
    throw std::runtime_error("recursive positive catch depth was admitted");
  }
  const auto retained_exceptions = validate(
      "try { throw 1; } catch (...) { "
      "std::vector<std::exception_ptr> held; "
      "for (int i=0; i<65; ++i) "
      "held.push_back(std::current_exception()); }");
  if (!::tracer::compat::starts_with(retained_exceptions,
                                     "context_exception_retention_unsupported:")) {
    throw std::runtime_error("retained exception_ptr reserve overflow was admitted: " +
                             retained_exceptions);
  }
  if (!::tracer::compat::starts_with(validate("", "struct Data : shared_state<Scope> { "
                                                  "~Data() noexcept { (void)new int; } };"),
                                     "context_noexcept_destructor_effect:")) {
    throw std::runtime_error("allocating noexcept state destructor was admitted");
  }
  if (!::tracer::compat::starts_with(validate("asm volatile(\"nop\");"),
                                     "unsupported_context_native_declaration:")) {
    throw std::runtime_error("user inline assembly was admitted");
  }
  if (!::tracer::compat::starts_with(
          validate("", "struct Data : shared_state<Scope> {};",
                   "extern \"C\" void foreign() __asm__(\"_Unwind_Resume\");"),
          "unsupported_context_native_declaration:")) {
    throw std::runtime_error("reserved asm-label declaration was admitted");
  }
  if (!::tracer::compat::starts_with(
          validate("", "struct Data : shared_state<Scope> {};",
                   "#if defined(__APPLE__)\n"
                   "__attribute__((section(\"__DATA,__context_guard\"))) int fake;\n"
                   "#else\n"
                   "extern \"C\" void alias_target() {}\n"
                   "extern \"C\" void fake() __attribute__((alias(\"alias_target\")));\n"
                   "#endif\n"),
          "unsupported_context_native_declaration:")) {
    throw std::runtime_error("alias or section attributed declaration was admitted");
  }
  if (validate("", "struct Data : shared_state<Scope> {};",
               "namespace std { extern void user_reserved(); }").empty())
    throw std::runtime_error("reserved std declaration was admitted");

  // Prepared adapter exemption. The fixture is assembled from pieces so the
  // region offset and size are computed rather than hard-coded.
  const std::string adapter_symbol = "adapter_target_symbol";
  const auto validate_region = [&](const std::string &before,
                                   const std::string &adapter,
                                   const std::string &after) {
    const std::string header = "struct Scope {};\n";
    const std::string source = header + before + adapter + after;
    auto ast = clang::tooling::buildASTFromCodeWithArgs(
        source, args, "context-support-effects-adapter.cc");
    if (!ast || ast->getDiagnostics().hasErrorOccurred())
      throw std::runtime_error("prepared adapter AST fixture did not parse");
    const tracer::PreparedAdapterRegion region{header.size() + before.size(),
                                               adapter.size(), adapter_symbol};
    return tracer::validate_context_support_effects(*ast, &region);
  };
  const std::string label =
      "void adapter_target(int) __asm__(\"" + adapter_symbol + "\");\n";
  const std::string rejected_prefix =
      "unsupported_context_native_declaration:";
  if (const auto error = validate_region("", label, ""); !error.empty())
    throw std::runtime_error("exact adapter label was rejected: " + error);
  if (!::tracer::compat::starts_with(validate_region(label, "", ""),
                                     rejected_prefix) ||
      !::tracer::compat::starts_with(
          validate_region("void other_target(int);\n", label,
                          "void user_target(int) __asm__(\"" + adapter_symbol +
                              "\");\n"),
          rejected_prefix))
    throw std::runtime_error("adapter label outside the region was admitted");
  if (!::tracer::compat::starts_with(
          validate_region("",
                          "void adapter_target(int) __asm__(\"other_symbol\");\n",
                          ""),
          rejected_prefix))
    throw std::runtime_error("different adapter label was admitted");
  if (!::tracer::compat::starts_with(
          validate_region("#define ADAPTER_LABEL __asm__(\"" + adapter_symbol +
                              "\")\n",
                          "void adapter_target(int) ADAPTER_LABEL;\n", ""),
          rejected_prefix))
    throw std::runtime_error("macro-expanded adapter label was admitted");
  if (!::tracer::compat::starts_with(
          validate_region("",
                          "void adapter_target(int) "
                          "__attribute__((section(\"adapter_section\")));\n",
                          ""),
          rejected_prefix))
    throw std::runtime_error("section attribute in the adapter region was admitted");
  if (!::tracer::compat::starts_with(
          validate_region("", "extern int adapter_value __asm__(\"" +
                                  adapter_symbol + "\");\n", ""),
          rejected_prefix))
    throw std::runtime_error("variable carrying the adapter label was admitted");
}

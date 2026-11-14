#pragma once

#include <string>
#include <utility>
#include <vector>

#include "context_prepared_adapter.hpp"

namespace clang { class ASTUnit; }

namespace tracer {
struct ContextHookParseCopy {
  std::string source;
  std::string error;
  // The input region translated into `source`; size 0 when none was given.
  PreparedAdapterRegion prepared_adapter;
};

// Speculative parser pass for optional state shorthand. The supplied source is
// never mutated; callers parse the returned engine-owned copy, whose marked
// placeholder bindings are replaced by generate_context_hooks().
ContextHookParseCopy prepare_context_hook_parse_copy(
    const std::string &source, const std::vector<std::string> &compiler_arguments,
    const std::string &file_name = "context-hook-input.cc",
    const PreparedAdapterRegion &prepared_adapter = {});

struct GeneratedContextHook {
  std::string type;
  std::string method;
  std::vector<std::string> direct;
  std::vector<std::string> injected;
  std::vector<std::pair<std::string, std::string>> shorthand;
  std::vector<std::pair<std::string, std::string>> generic_direct;
  std::vector<std::pair<std::string, std::string>> generic_visitors;
  std::vector<std::pair<std::string, std::string>> generic_scope_initializers;
  bool events = false;
  bool direct_frame = false;
  bool captures_stack = false;
};

struct GeneratedContextHooks {
  std::string compilation_copy;
  std::string facade_namespace;
  std::vector<GeneratedContextHook> hooks;
  std::string error;
};

// Adds private engine entrypoints to the compiler's source copy only. The
// required private facade is documented in context_hooks.cpp.
GeneratedContextHooks generate_context_hooks(clang::ASTUnit &,
                                             const std::string &facade_namespace = "engine");
} // namespace tracer

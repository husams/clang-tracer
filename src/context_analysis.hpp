#pragma once

#include <string>
#include <vector>

namespace clang {
class ASTContext;
class CXXRecordDecl;
}

namespace tracer {
struct ContextDependency {
  std::string state_type;
  std::string scope_type;
  bool shared = false;
  std::string parameter_name;
  bool generic = false;
  std::string payload_type;
};

struct ContextHookPlan {
  std::string method;
  std::vector<std::string> injected;
  std::vector<std::string> direct;
  std::vector<std::string> visitors;
  std::vector<ContextDependency> generic_direct;
  std::vector<ContextDependency> generic_visitors;
  std::vector<ContextDependency> generic_scope_initializers;
  std::vector<std::string> reachable_methods;
  bool events = false;
  // The selected probe must snapshot its entry stack before acquiring any
  // context payload guard because Call::capture_stack may perform native work.
  bool captures_stack = false;
  bool all_visible_events = false;
  bool direct_frame = false;
  bool local_scope_changes = false;
  bool initializes_scope = false;
  bool ends_scope = false;
  bool unsupported_effects = false;
  std::string unsupported_effect;
  bool invalid_access = false;
  std::string invalid_access_effect;
  bool has_reference_capture = false;
  bool uses_state_name = false;
  std::vector<std::pair<std::string, std::string>> shorthand;
  std::vector<std::pair<unsigned, unsigned>> generic_get_source_ranges;
};

struct ContextClassPlan {
  std::string type;
  std::vector<ContextDependency> dependencies;
  std::vector<ContextHookPlan> hooks;
  bool context_only = false;
};

struct ContextDeclarations {
  bool requires_context = false;
  // Probe-wide entry stack snapshot requirement, including user state and
  // generic payload construction that can execute before/among hooks.
  bool requires_stack_capture = false;
  // Every user type carrying exactly one state<Scope> or
  // shared_state<Scope> marker. The engine uses this immutable catalog to
  // register complete scope groups before any hook can execute.
  std::vector<ContextDependency> states;
  std::vector<ContextClassPlan> classes;
  std::string error;
};

ContextDeclarations analyze_context_declarations(clang::ASTContext &);

// Includes arguments for fully specialized interceptor classes, while keeping
// ordinary record names unchanged throughout planning and generated dispatch.
std::string context_class_type_name(const clang::CXXRecordDecl &);

// Declaration discovery grants no authority to execute user state. The caller
// must separately prove the exact owner/support/boundary/unwind generation.
std::string context_activation_error(const ContextDeclarations &);
} // namespace tracer

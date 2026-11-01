#pragma once

#include <string>
#include <vector>

namespace clang {
class ASTUnit;
}

namespace tracer {
struct GeneratedLifecycleClass {
  std::string type;
  std::string frame_member;
};
struct GeneratedContextLifecycle {
  std::string compilation_copy;
  std::string facade_namespace;
  std::vector<GeneratedLifecycleClass> classes;
  std::string error;
};

// Render the engine's compilation copy from observed Clang declarations. The
// private facade must provide engine::current_context() -> Context& and a
// noncopyable InvocationFrame(Context&) whose push_create_or_attach<T>()
// transactionally owns/attaches each state and whose pop_all_and_restore()
// is noexcept. It also creates the frame for context-only interceptors with
// the implicit default constructor. This grants no owner, support or unwind
// admission.
// The user's original buffers/files are never modified.
GeneratedContextLifecycle generate_context_lifecycle(
    clang::ASTUnit &, const std::string &facade_namespace = "engine");
} // namespace tracer

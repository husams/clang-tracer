#pragma once

// Target-call bridge for context hooks.
//
// A context hook runs in the sealed context runtime: its own allocator, its own
// exception storage, a closed module without process symbol lookup and without
// indirect calls. A function of the attached process runs in the process's own
// runtime. The bridge lets a hook call such a function without mixing the two:
//
//   hook (sealed runtime)                    target's runtime
//   target_call<bridge, R, P...>(args)  -->  clangtracer_target_call_N(out, erased args)
//                                              performs the call (virtual dispatch
//                                              included), catches every exception,
//                                              keeps a std::string result alive until
//                                              the hook has copied its bytes
//
// Only scalars and addresses cross the boundary. The analysis classifies the
// call, the hook lowering rewrites it, and the agent compiles the generated
// target side with its ordinary callback compiler.

#include <cstddef>
#include <string>
#include <vector>

namespace clang {
class ASTContext;
class CallExpr;
class Expr;
class FunctionDecl;
} // namespace clang

namespace tracer {

// How one value crosses the bridge.
enum class ContextTargetValue : unsigned char {
  none,    // void result
  scalar,  // arithmetic or enumeration, by value
  address, // pointer or lvalue reference
  text,    // std::string result, copied into the hook's runtime
};

struct ContextTargetParameter {
  // Spelling of the declared parameter type in the callback source.
  std::string type;
  // Type of the same value in the bridge function's C signature.
  std::string erased;
};

// One target function a hook calls. Call sites of the same function share it.
struct ContextTargetBridge {
  std::string name;   // qualified name, for diagnostics
  std::string symbol; // linkage name of the function
  // The call dispatches through the receiver's vtable at this slot instead of
  // binding to `symbol`.
  bool virtual_dispatch = false;
  unsigned vtable_index = 0;
  // The first parameter is the receiver.
  bool member = false;
  ContextTargetValue result = ContextTargetValue::none;
  std::string result_type;   // spelling in the callback source
  std::string result_erased; // scalar results only
  std::vector<ContextTargetParameter> parameters;

  // Two call sites with the same key use the same bridge function.
  std::string key() const;
};

struct ContextTargetCallSite {
  ContextTargetBridge bridge;
  // Receiver expression as written; null for a call that has no receiver.
  const clang::Expr *object = nullptr;
  bool arrow = false;
};

enum class ContextTargetCallKind : unsigned char {
  not_target,  // the callee is not a function of the target process
  supported,   // the bridge can carry this call
  unsupported, // a target function, but this call cannot be bridged
};

// True for a function that has no body in the callback and is declared outside
// the toolchain and engine headers: its definition can only be in the target.
bool is_context_target_function(clang::ASTContext &,
                                const clang::FunctionDecl *);

// Classifies one call expression. `reason` says why an unsupported call cannot
// be bridged; it does not name the callee.
ContextTargetCallKind classify_context_target_call(clang::ASTContext &,
                                                   const clang::CallExpr &,
                                                   ContextTargetCallSite &,
                                                   std::string &reason);

// Names of the generated functions.
std::string context_target_bridge_symbol(std::size_t index);
inline constexpr const char *kContextTargetReleaseText =
    "clangtracer_target_release_text";

// Declarations of the bridge functions, as the callback module and the native
// capability catalog both see them. They use builtin types only.
std::string
context_target_declarations(const std::vector<ContextTargetBridge> &);

// The call expression that replaces a target callee: the facade template with
// the bridge and the declared types as explicit arguments, without the
// argument list.
std::string context_target_callee(const ContextTargetBridge &,
                                  std::size_t index);

// One change to the callback source. `extent` is the end offset of the call
// the edit belongs to: of two edits at one offset, the one with the smaller
// extent belongs to the inner call and is applied first.
struct ContextTargetEdit {
  std::size_t offset = 0;
  std::size_t length = 0;
  std::string text;
  std::size_t extent = 0;
};

// Edits, in offsets of the main source buffer, that turn a supported call into
// a call of `callee` with the receiver as first argument. False when a part of
// the call is not written directly in the main file (a macro, an included
// file), so its text cannot be rewritten.
bool context_target_call_edits(clang::ASTContext &, const clang::CallExpr &,
                               const ContextTargetCallSite &,
                               const std::string &callee,
                               std::vector<ContextTargetEdit> &);

// Source of the target side of every bridge, to be compiled in the target's own
// runtime. It defines context_target_bridge_symbol(i) for each bridge and
// kContextTargetReleaseText, and needs tracer_target_call_enter/leave.
std::string context_target_thunks(const std::vector<ContextTargetBridge> &);

} // namespace tracer

#include "context_hooks.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>

#include "context_analysis.hpp"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"

namespace tracer {
namespace {
struct Insert { std::size_t offset; std::string text; std::size_t length = 0; };
const clang::ClassTemplateSpecializationDecl *state_token(clang::QualType type) {
  const auto *record = type.getNonReferenceType()->getAsCXXRecordDecl();
  const auto *special = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(record);
  return special && special->getSpecializedTemplate()->getQualifiedNameAsString() ==
                        "tracer::State" ? special : nullptr;
}
std::string type_name(clang::QualType type) {
  type = type.getNonReferenceType().getCanonicalType();
  if (const auto *record = type->getAsCXXRecordDecl()) {
    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
      return type.getAsString();
    return record->getQualifiedNameAsString();
  }
  return type.getCanonicalType().getAsString();
}
std::string cpp_type(const std::string &type) {
  std::string spelling = type;
  for (const auto tag : {std::string_view("class "),
                         std::string_view("struct "),
                         std::string_view("union "),
                         std::string_view("enum ")}) {
    if (::tracer::compat::starts_with(spelling, tag)) {
      spelling.erase(0, tag.size());
      break;
    }
  }
  static const std::vector<std::string> builtins = {
      "bool", "char", "signed char", "unsigned char", "wchar_t",
      "char8_t", "char16_t", "char32_t", "short", "unsigned short",
      "int", "unsigned int", "long", "unsigned long", "long long",
      "unsigned long long", "float", "double", "long double"};
  if (::tracer::compat::starts_with(spelling, "::") ||
      std::find(builtins.begin(), builtins.end(), spelling) != builtins.end()) {
    return spelling;
  }
  return "::" + spelling;
}
bool main_file_range(clang::ASTUnit &unit, clang::SourceRange range,
                     std::size_t &offset, std::size_t &length) {
  auto &sm = unit.getSourceManager();
  const auto file_range = clang::Lexer::makeFileCharRange(
      clang::CharSourceRange::getTokenRange(range), sm, unit.getLangOpts());
  if (file_range.isInvalid() || file_range.getBegin().isMacroID() ||
      file_range.getEnd().isMacroID() ||
      !sm.isWrittenInMainFile(file_range.getBegin()))
    return false;
  offset = sm.getFileOffset(file_range.getBegin());
  const auto end = sm.getFileOffset(file_range.getEnd());
  if (end < offset) return false;
  length = end - offset;
  return true;
}
bool contains_state_identifier(llvm::StringRef text) {
  for (std::size_t at = 0; (at = text.find("state", at)) != llvm::StringRef::npos; ++at) {
    const bool left = at == 0 || !(std::isalnum(static_cast<unsigned char>(text[at - 1])) || text[at - 1] == '_');
    const auto end = at + 5;
    const bool right = end == text.size() || !(std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_');
    if (left && right) return true;
  }
  return false;
}
struct PlaceholderInsert { std::size_t offset; std::string text; };
class PlaceholderDiscovery final
    : public clang::RecursiveASTVisitor<PlaceholderDiscovery> {
public:
  explicit PlaceholderDiscovery(clang::ASTUnit &unit) : unit(unit) {
    const auto source = unit.getSourceManager().getBufferData(
        unit.getSourceManager().getMainFileID());
    for (unsigned i = 0; i < 4096; ++i) {
      binding_type = "__clang_tracer_context_bindings_" + std::to_string(i);
      if (!::tracer::compat::contains(source, binding_type)) {
        break;
      }
      binding_type.clear();
    }
    if (binding_type.empty())
      error = "unsupported_context_shorthand: no collision-free placeholder "
              "binding name";
  }
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (!record->isThisDeclarationADefinition() || record->isImplicit()) return true;
    std::vector<std::pair<std::string, std::string>> dependencies;
    for (const auto *constructor : record->ctors())
      for (const auto *parameter : constructor->parameters())
        if (const auto *token = state_token(parameter->getType())) {
          const auto &args = token->getTemplateArgs();
          if (args.size() == 1 && args[0].getKind() == clang::TemplateArgument::Type)
            dependencies.emplace_back(type_name(args[0].getAsType()), parameter->getNameAsString());
        }
    if (dependencies.empty()) return true;
    auto &sm = unit.getSourceManager();
    for (const auto *method : record->methods()) {
      if (!method->hasBody()) continue;
      bool owns_state_name = false;
      for (const auto *parameter : method->parameters())
        owns_state_name |= parameter->getNameAsString() == "state";
      if (owns_state_name) continue;
      const auto body = method->getBody()->getBeginLoc();
      if (body.isInvalid() || body.isMacroID() || !sm.isWrittenInMainFile(body)) continue;
      const auto range = method->getBody()->getSourceRange();
      const auto text = clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(range), sm, unit.getLangOpts());
      if (!contains_state_identifier(text)) continue;
      const auto method_name = method->getNameAsString();
      if (std::count_if(record->methods().begin(), record->methods().end(),
                        [&](const auto *candidate) {
                          return candidate->getNameAsString() == method_name;
                        }) != 1) {
        error = "unsupported_context_shorthand: overloaded helper names cannot "
                "be disambiguated in the parser copy";
        return false;
      }
      std::string marker = "/*__TRACER_CONTEXT_PLACEHOLDER_BEGIN:" +
          context_class_type_name(*record) + ":" + method_name +
          "__*/\n#pragma clang diagnostic push\n#pragma clang diagnostic ignored \"-Wnull-dereference\"\n";
      if (dependencies.size() == 1) {
        marker += "::" + dependencies.front().first +
                  "& state = *static_cast<::" + dependencies.front().first + "*>(nullptr);\n";
      } else {
        marker += "struct " + binding_type + " {\n";
        for (const auto &dependency : dependencies) {
          if (dependency.second.empty()) {
            error = "invalid_state_shorthand: multiple dependencies require named constructor parameters";
            return false;
          }
          marker += "  ::" + dependency.first + "& " + dependency.second + ";\n";
        }
        marker += "} state{";
        for (std::size_t i = 0; i < dependencies.size(); ++i) {
          if (i) marker += ", ";
          marker += "*static_cast<::" + dependencies[i].first + "*>(nullptr)";
        }
        marker += "};\n";
      }
      marker += "#pragma clang diagnostic pop\n/*__TRACER_CONTEXT_PLACEHOLDER_END__*/\n";
      // The marker adds lines. Restore the numbering so diagnostics for the
      // rest of the body name the lines the user wrote.
      if (const auto presumed = sm.getPresumedLoc(body); presumed.isValid()) {
        marker += "#line " + std::to_string(presumed.getLine()) + " \"";
        for (const char c : std::string_view(presumed.getFilename())) {
          if (c == '"' || c == '\\') marker += '\\';
          marker += c;
        }
        marker += "\"\n";
      }
      inserts.push_back({sm.getFileOffset(body) + 1, std::move(marker)});
    }
    return true;
  }
  std::vector<PlaceholderInsert> inserts;
  std::string error;
  std::string binding_type;
private:
  clang::ASTUnit &unit;
};
std::string qualified(const clang::CXXRecordDecl &record) {
  return context_class_type_name(record);
}
std::string render(const ContextHookPlan &hook, const clang::CXXRecordDecl &r,
                   const std::string &facade) {
  const auto type = qualified(r);
  std::string out = "\nvoid " + type + "::__tracer_invoke_" + hook.method +
      "(::tracer::Call& call) {\n  ";
  if (hook.direct_frame) {
    out += "::" + facade + "::run_direct_hook<";
    bool first = true;
    for (const auto &state : hook.direct) {
      if (!first) out += ", ";
      first = false;
      out += "::" + state;
    }
    for (const auto &state : hook.visitors) {
      if (std::find(hook.direct.begin(), hook.direct.end(), state) !=
              hook.direct.end() ||
          std::find(hook.injected.begin(), hook.injected.end(), state) !=
              hook.injected.end()) continue;
      if (!first) out += ", ";
      first = false;
      out += "::" + state;
    }
    for (const auto &state : hook.injected) {
      if (std::find(hook.direct.begin(), hook.direct.end(), state) !=
          hook.direct.end()) continue;
      if (!first) out += ", ";
      first = false;
      out += "::" + state;
    }
    for (const auto &entry : hook.shorthand) {
      if (std::find(hook.direct.begin(), hook.direct.end(), entry.second) !=
              hook.direct.end() ||
          std::find(hook.injected.begin(), hook.injected.end(), entry.second) !=
              hook.injected.end()) continue;
      if (!first) out += ", ";
      first = false;
      out += "::" + entry.second;
    }
    auto payload_accesses = hook.generic_direct;
    for (const auto &access : hook.generic_visitors)
      if (std::find_if(payload_accesses.begin(), payload_accesses.end(),
                       [&](const auto &existing) {
                         return existing.scope_type == access.scope_type &&
                                existing.payload_type == access.payload_type;
                       }) == payload_accesses.end())
        payload_accesses.push_back(access);
    for (const auto &access : payload_accesses) {
      if (!first) out += ", ";
      first = false;
      out += "::" + facade + "::PayloadAccess<" +
             cpp_type(access.scope_type) + ", " +
             cpp_type(access.payload_type) + ">";
    }
    out += std::string(">(call, ") +
           (hook.all_visible_events ? "true" : "false") +
           ", [&] {\n    this->" + hook.method + "(call";
    for (const auto &state : hook.injected)
      out += ", *::" + facade + "::request<::" + state + ">()";
    out += ");\n  });\n";
  } else {
    out += "::" + facade + "::run_short_hook(call, " +
           (hook.all_visible_events ? "true" : "false") +
           ", [&] { this->" + hook.method + "(call); });\n";
  }
  out += "}\n";
  return out;
}
class HookLowering final : public clang::RecursiveASTVisitor<HookLowering> {
public:
  HookLowering(clang::ASTUnit &unit, const ContextDeclarations &analysis,
               GeneratedContextHooks &result, std::string facade)
      : unit(unit), analysis(analysis), result(result), facade(std::move(facade)) {
    const auto source = unit.getSourceManager().getBufferData(
        unit.getSourceManager().getMainFileID());
    for (unsigned i = 0; i < 4096; ++i) {
      binding_type = "__clang_tracer_context_bindings_" + std::to_string(i);
      if (!::tracer::compat::contains(source, binding_type)) {
        break;
      }
      binding_type.clear();
    }
    if (binding_type.empty())
      result.error = "unsupported_context_shorthand: no collision-free binding name";
  }
  bool TraverseCXXRecordDecl(clang::CXXRecordDecl *record) {
    const auto prior = current_class;
    if (record && record->getIdentifier()) current_class = qualified(*record);
    const bool ok = clang::RecursiveASTVisitor<HookLowering>::
        TraverseCXXRecordDecl(record);
    current_class = prior;
    return ok;
  }
  bool TraverseCXXMethodDecl(clang::CXXMethodDecl *method) {
    const auto prior = current_method;
    if (method) current_method = method->getNameAsString();
    const bool ok = clang::RecursiveASTVisitor<HookLowering>::
        TraverseCXXMethodDecl(method);
    current_method = prior;
    return ok;
  }
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (!record->isThisDeclarationADefinition() || record->isImplicit()) return true;
    const auto found = std::find_if(analysis.classes.begin(), analysis.classes.end(),
        [&](const auto &c) { return c.type == qualified(*record); });
    if (found == analysis.classes.end()) return true;
    auto &sm = unit.getSourceManager();
    const auto close = record->getBraceRange().getEnd();
    if (!record->getIdentifier() || record->isLocalClass() || record->isUnion() ||
        record->getDescribedClassTemplate() || close.isInvalid() ||
        close.isMacroID() || !sm.isWrittenInMainFile(close)) {
      result.error = "unsupported_context_hooks: hooks require a named, direct "
                     "main-file class definition";
      return false;
    }
    std::string declarations = "\nprivate:\n";
    for (const auto &hook : found->hooks) {
      declarations += "  void __tracer_invoke_" + hook.method +
                      "(::tracer::Call&);\n";
      result.hooks.push_back({found->type, hook.method, hook.direct,
                              hook.injected, hook.shorthand,
                              generic_pairs(hook.generic_direct),
                              generic_pairs(hook.generic_visitors),
                              generic_pairs(hook.generic_scope_initializers),
                              hook.events,
                              hook.direct_frame,
                              hook.captures_stack});
      definitions += render(hook, *record, facade);
      for (const auto *method : record->methods()) {
        if (method->getNameAsString() != hook.method || !method->hasBody()) continue;
        const auto body = method->getBody()->getBeginLoc();
        if (body.isInvalid() || body.isMacroID() || !sm.isWrittenInMainFile(body)) {
          result.error = "unsupported_context_hooks: hook body must be in the "
                         "main file for source lowering";
          return false;
        }
        std::string locals;
        if (hook.shorthand.size() == 1 && hook.shorthand.front().first == "state") {
          locals = "\n  auto& state = *::" + facade + "::request<::" +
                   hook.shorthand.front().second + ">();\n";
        } else if (!hook.shorthand.empty()) {
          locals = "\n  struct " + binding_type + " {\n";
          for (const auto &binding : hook.shorthand)
            locals += "    ::" + binding.second + "& " +
                      binding.first.substr(6) + ";\n";
          locals += "  } state{";
          for (std::size_t i = 0; i < hook.shorthand.size(); ++i) {
            if (i) locals += ", ";
            locals += "*::" + facade + "::request<::" + hook.shorthand[i].second + ">()";
          }
          locals += "};\n";
        }
        const auto placeholder = "/*__TRACER_CONTEXT_PLACEHOLDER_BEGIN:" +
            found->type + ":" + hook.method + "__*/";
        const auto parse_buffer = sm.getBufferData(sm.getMainFileID());
        if (!locals.empty() && !::tracer::compat::contains(parse_buffer, placeholder)) {
          const auto offset = sm.getFileOffset(body) + 1;
          insertions.push_back({offset, std::move(locals)});
        }
      }
    }
    if (!found->hooks.empty())
      insertions.push_back({sm.getFileOffset(close), std::move(declarations)});
    return true;
  }
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    const auto *method = call->getMethodDecl();
    if (!method || method->getParent()->getQualifiedNameAsString() !=
                       "tracer::Context" || method->getNameAsString() != "get")
      return true;
    const auto *args = method->getTemplateSpecializationArgs();
    if (!args || args->size() != 2 ||
        args->get(0).getKind() != clang::TemplateArgument::Type ||
        args->get(1).getKind() != clang::TemplateArgument::Type)
      return true;
    std::size_t offset = 0, length = 0;
    if (!main_file_range(unit, call->getSourceRange(), offset, length)) {
      result.error = "unsupported_generic_context_access: get expression must "
                     "be in the main source file";
      return false;
    }
    bool reachable = false;
    for (const auto &class_plan : analysis.classes)
      for (const auto &hook : class_plan.hooks)
        reachable |= (class_plan.type == current_class &&
                      std::find(hook.reachable_methods.begin(),
                                hook.reachable_methods.end(), current_method) !=
                          hook.reachable_methods.end()) ||
                     std::find(hook.generic_get_source_ranges.begin(),
                               hook.generic_get_source_ranges.end(),
                               std::pair<unsigned, unsigned>{
                                   static_cast<unsigned>(offset),
                                   static_cast<unsigned>(length)}) !=
                     hook.generic_get_source_ranges.end();
    if (!reachable) return true;
    const auto scope = type_name(args->get(0).getAsType());
    const auto payload =
        type_name(args->get(1).getAsType().getNonReferenceType());
    insertions.push_back({offset,
        "(*::" + facade + "::request_payload<" + cpp_type(scope) + ", " +
            cpp_type(payload) + ">())", length});
    return true;
  }
  std::vector<Insert> insertions;
  std::string definitions;
  const std::string &bindingType() const noexcept { return binding_type; }
private:
  clang::ASTUnit &unit;
  const ContextDeclarations &analysis;
  GeneratedContextHooks &result;
  std::string facade;
  std::string binding_type;
  std::string current_class;
  std::string current_method;
  static std::vector<std::pair<std::string, std::string>>
  generic_pairs(const std::vector<ContextDependency> &accesses) {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &access : accesses)
      result.emplace_back(access.scope_type, access.payload_type);
    return result;
  }
};
} // namespace

ContextHookParseCopy prepare_context_hook_parse_copy(
    const std::string &source,
    const std::vector<std::string> &compiler_arguments,
    const std::string &file_name,
    const PreparedAdapterRegion &prepared_adapter) {
  ContextHookParseCopy result;
  const std::string prefix = "#include \"tracer/callback.hpp\"\n";
  clang::IgnoringDiagConsumer ignore_diagnostics;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      prefix + source,
      clang_driver_arguments(compiler_arguments), file_name,
      "clang-context-preparse",
      std::make_shared<clang::PCHContainerOperations>(),
      clang::tooling::getClangStripDependencyFileAdjuster(), {},
      &ignore_diagnostics);
  if (!ast) {
    result.error = "invalid_context_parse: speculative AST recovery failed";
    return result;
  }
  PlaceholderDiscovery discovery(*ast);
  discovery.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
  if (!discovery.error.empty()) { result.error = discovery.error; return result; }
  auto &sm = ast->getSourceManager();
  result.source = sm.getBufferData(sm.getMainFileID()).str();
  // Insert offsets and the region both live in the prefixed buffer. Translate
  // the region from those original offsets before any insert is applied.
  std::size_t shift = prefix.size();
  const std::size_t region_begin = prepared_adapter.offset + prefix.size();
  const std::size_t region_end = region_begin + prepared_adapter.size;
  if (prepared_adapter.size != 0) {
    for (const auto &insert : discovery.inserts) {
      if (insert.offset > region_begin && insert.offset < region_end) {
        result.error = "invalid_context_parse: placeholder insertion inside "
                       "the prepared adapter region";
        return result;
      }
      if (insert.offset <= region_begin) shift += insert.text.size();
    }
    result.prepared_adapter = prepared_adapter;
    result.prepared_adapter.offset = prepared_adapter.offset + shift;
  }
  std::sort(discovery.inserts.begin(), discovery.inserts.end(),
      [](const auto &a, const auto &b) { return a.offset > b.offset; });
  for (const auto &insert : discovery.inserts)
    result.source.insert(insert.offset, insert.text);
  return result;
}

GeneratedContextHooks generate_context_hooks(
    clang::ASTUnit &unit, const std::string &facade_namespace) {
  GeneratedContextHooks result;
  result.facade_namespace = facade_namespace;
  const auto analysis = analyze_context_declarations(unit.getASTContext());
  if (!analysis.error.empty()) { result.error = analysis.error; return result; }
  auto &sm = unit.getSourceManager();
  if (!analysis.requires_context) {
    result.compilation_copy = sm.getBufferData(sm.getMainFileID()).str();
    return result;
  }
  HookLowering visitor(unit, analysis, result, facade_namespace);
  if (!visitor.TraverseDecl(unit.getASTContext().getTranslationUnitDecl()) ||
      !result.error.empty()) {
    result.hooks.clear();
    return result;
  }
  result.compilation_copy = sm.getBufferData(sm.getMainFileID()).str();
  std::sort(visitor.insertions.begin(), visitor.insertions.end(),
      [](const auto &a, const auto &b) { return a.offset > b.offset; });
  for (const auto &insertion : visitor.insertions) {
    if (insertion.length)
      result.compilation_copy.replace(insertion.offset, insertion.length,
                                      insertion.text);
    else
      result.compilation_copy.insert(insertion.offset, insertion.text);
  }
  result.compilation_copy += visitor.definitions;
  for (const auto &class_plan : analysis.classes) {
    const auto prefix = "/*__TRACER_CONTEXT_PLACEHOLDER_BEGIN:" +
        class_plan.type + ":";
    std::size_t search_from = 0;
    for (;;) {
      const auto begin = result.compilation_copy.find(prefix, search_from);
      if (begin == std::string::npos) break;
      const auto method_begin = begin + prefix.size();
      const auto marker_end = result.compilation_copy.find("__*/", method_begin);
      const std::string end_marker = "/*__TRACER_CONTEXT_PLACEHOLDER_END__*/";
      const auto end = marker_end == std::string::npos
          ? std::string::npos : result.compilation_copy.find(end_marker, marker_end);
      if (marker_end == std::string::npos || end == std::string::npos) {
        result.error = "invalid_context_parse: unterminated placeholder binding";
        result.compilation_copy.clear();
        result.hooks.clear();
        return result;
      }
      const auto method_name = result.compilation_copy.substr(
          method_begin, marker_end - method_begin);
      const ContextHookPlan *owner_hook = nullptr;
      for (const auto &hook : class_plan.hooks) {
        if (std::find(hook.reachable_methods.begin(),
                      hook.reachable_methods.end(), method_name) !=
            hook.reachable_methods.end()) {
          owner_hook = &hook;
          break;
        }
      }
      if (!owner_hook) {
        result.error = "invalid_state_shorthand: state binding is used outside "
                       "a reachable pre/post hook";
        result.compilation_copy.clear();
        result.hooks.clear();
        return result;
      }
      std::vector<std::pair<std::string, std::string>> aliases_plan =
          owner_hook->shorthand;
      if (aliases_plan.empty() && class_plan.dependencies.size() == 1)
        aliases_plan.emplace_back("state", class_plan.dependencies.front().state_type);
      if (aliases_plan.empty()) {
        for (const auto &dependency : class_plan.dependencies)
          aliases_plan.emplace_back("state." + dependency.parameter_name,
                                    dependency.state_type);
      }
      std::string aliases;
      if (aliases_plan.size() == 1 && aliases_plan.front().first == "state") {
        const auto &type = aliases_plan.front().second;
        aliases = "::" + type + "& state = *::" + facade_namespace +
                  "::request<::" + type + ">();\n";
      } else if (!aliases_plan.empty()) {
        aliases = "struct " + visitor.bindingType() + " {\n";
        for (const auto &binding : aliases_plan)
          aliases += "  ::" + binding.second + "& " + binding.first.substr(6) + ";\n";
        aliases += "} state{";
        for (std::size_t i = 0; i < aliases_plan.size(); ++i) {
          if (i) aliases += ", ";
          aliases += "*::" + facade_namespace + "::request<::" +
                     aliases_plan[i].second + ">()";
        }
        aliases += "};\n";
      }
      result.compilation_copy.replace(begin, end + end_marker.size() - begin, aliases);
      search_from = begin + aliases.size();
    }
  }
  return result;
}
} // namespace tracer

/*
Private native facade required by this lowering:
  template<class... T, class F>
  void run_direct_hook(Call&, bool all_visible_events, F&&);
  template<class Scope, class Payload> struct PayloadAccess;
  template<class... T> T& request();
  template<class Scope, class Payload> Payload& request_payload();
  template<class F> void run_short_hook(Call&, F&&);
  template<class Scope, class Payload, class F> void with_payload(F&&);
  template<class Scope> void init_scope();
  template<class Scope, class Payload> void init_scope(Payload&&);
  template<class Scope> void end_scope();

run_direct_hook installs CurrentHook and nonthrowing HookExit, reserves capture
resources, resolves/pins/locks the complete injected/get plan plus every visible
slot when an event is reachable, and only then invokes the callback. It catches
callback/access failures at this boundary, releases guards, commits valid ends,
and publishes complete events/diagnostics. The target call stays outside this
boundary and its catch blocks. request<T>() and request_payload<Scope,Payload>()
resolve from the already-held plan. PayloadAccess<Scope,Payload> is one complete
frame plan entry. Public Context member-template definitions may delegate to
with_payload/init_scope/end_scope; those helpers consult the installed native
CurrentHook and use the same bounded nonthrowing exit/error path.
run_short_hook installs the same context/exit without entry payload locks;
with<T>() and event() create independent intervals. Event outside an active hook,
including lifecycle/native cleanup, counts no_active_hook without throwing.
Visitors cannot leak borrowed references. Reachable indirect/opaque/native calls,
unsupported reachable effects, escaping borrows and invalid lifecycle/access
combinations must reject activation before wrapper entry.
*/

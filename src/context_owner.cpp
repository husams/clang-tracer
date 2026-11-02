#include "context_owner.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/Preprocessor.h>

#include "context_schema.hpp"
#include "tracer/compat.hpp"

namespace tracer {
namespace {
std::string canonical_payload_type(clang::QualType type) {
  type = type.getNonReferenceType().getCanonicalType();
  if (const auto *record = type->getAsCXXRecordDecl()) {
    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record)) {
      return type.getAsString();
    }
    return record->getQualifiedNameAsString();
  }
  return type.getAsString();
}

std::string canonical_payload_scope(clang::QualType type) {
  type = type.getCanonicalType();
  if (const auto *record = type->getAsCXXRecordDecl()) {
    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record)) {
      return type.getAsString();
    }
    return record->getQualifiedNameAsString();
  }
  return type.getAsString();
}

std::string source_cpp_type(std::string spelling) {
  // Canonical payload spelling is retained in family metadata; generated
  // owner templates need a valid type-id instead of Clang's diagnostic tags.
  for (const std::string_view keyword :
       {"class ", "struct ", "union ", "enum "}) {
    std::size_t offset = 0;
    while ((offset = spelling.find(keyword, offset)) != std::string::npos) {
      spelling.erase(offset, keyword.size());
    }
  }
  static const std::set<std::string> builtins = {"void",
                                                 "bool",
                                                 "char",
                                                 "signed char",
                                                 "unsigned char",
                                                 "short",
                                                 "short int",
                                                 "unsigned short",
                                                 "unsigned short int",
                                                 "int",
                                                 "unsigned",
                                                 "unsigned int",
                                                 "long",
                                                 "long int",
                                                 "unsigned long",
                                                 "unsigned long int",
                                                 "long long",
                                                 "long long int",
                                                 "unsigned long long",
                                                 "unsigned long long int",
                                                 "float",
                                                 "double",
                                                 "long double",
                                                 "wchar_t",
                                                 "char8_t",
                                                 "char16_t",
                                                 "char32_t",
                                                 "std::nullptr_t"};
  if (::tracer::compat::contains(builtins, spelling) ||
      ::tracer::compat::starts_with(spelling, "::")) {
    return spelling;
  }
  return "::" + spelling;
}

class GenericPayloadTypeFinder final
    : public clang::RecursiveASTVisitor<GenericPayloadTypeFinder> {
public:
  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    const auto *method = call->getMethodDecl();
    if (!method ||
        method->getParent()->getQualifiedNameAsString() != "tracer::Context") {
      return true;
    }
    const auto name = method->getNameAsString();
    const auto *arguments = method->getTemplateSpecializationArgs();
    clang::QualType payload;
    clang::QualType scope;
    if (name == "init_scope" && arguments && arguments->size() >= 1 &&
        arguments->get(0).getKind() == clang::TemplateArgument::Type &&
        call->getNumArgs()) {
      scope = arguments->get(0).getAsType();
      payload = call->getArg(0)->getType().getNonReferenceType();
    } else if ((name == "get" || name == "with") && arguments &&
               arguments->size() >= 2 &&
               arguments->get(0).getKind() == clang::TemplateArgument::Type &&
               arguments->get(1).getKind() == clang::TemplateArgument::Type) {
      scope = arguments->get(0).getAsType();
      payload = arguments->get(1).getAsType();
    } else {
      return true;
    }
    const auto scope_name = canonical_payload_scope(scope);
    const auto payload_name = canonical_payload_type(payload);
    if (!scope_name.empty() && !payload_name.empty()) {
      types.emplace_back(scope_name, payload_name, payload.getCanonicalType());
    }
    return true;
  }

  clang::QualType find(std::string_view scope, std::string_view payload) const {
    const auto found =
        std::find_if(types.begin(), types.end(), [&](const auto &entry) {
          return std::get<0>(entry) == scope && std::get<1>(entry) == payload;
        });
    return found == types.end() ? clang::QualType{} : std::get<2>(*found);
  }

private:
  std::vector<std::tuple<std::string, std::string, clang::QualType>> types;
};
} // namespace

GeneratedContextOwner generate_context_owner(clang::ASTUnit &unit,
                                             bool serializers) {
  GeneratedContextOwner result;
  const auto declarations = analyze_context_declarations(unit.getASTContext());
  if (unit.getDiagnostics().hasErrorOccurred() || !declarations.error.empty()) {
    result.error =
        declarations.error.empty()
            ? "invalid_owner_source: compiler diagnostics contain errors"
            : declarations.error;
    return result;
  }
  auto &manager = unit.getSourceManager();
  const auto original = manager.getBufferData(manager.getMainFileID());
  std::string prefix;
  for (std::size_t attempt = 0;; ++attempt) {
    prefix = "clangtracer_owner_" + std::to_string(attempt) + "_";
    if (!::tracer::compat::contains(original, prefix) &&
        !unit.getPreprocessor().isMacroDefined(prefix)) {
      break;
    }
  }
  auto ordered_dependencies = declarations.states;
  const auto canonical_state =
      [](const ContextDependency &dependency) -> const std::string & {
    return dependency.generic ? dependency.payload_type : dependency.state_type;
  };
  std::sort(ordered_dependencies.begin(), ordered_dependencies.end(),
            [&](const auto &left, const auto &right) {
              return std::tie(left.scope_type, left.generic,
                              canonical_state(left)) <
                     std::tie(right.scope_type, right.generic,
                              canonical_state(right));
            });
  std::map<std::string, std::uint64_t> next_initializer_by_scope;
  for (const auto &dependency : ordered_dependencies) {
    const auto found = std::find_if(
        result.states.begin(), result.states.end(), [&](const auto &state) {
          return state.type.generic == dependency.generic &&
                 (dependency.generic
                      ? state.type.scope_type == dependency.scope_type &&
                            state.type.payload_type == dependency.payload_type
                      : state.type.state_type == dependency.state_type);
        });
    if (found != result.states.end()) {
      if (found->type.scope_type != dependency.scope_type ||
          found->type.shared != dependency.shared) {
        result.error = "incompatible_owner_type: conflicting scope/policy";
        result.states.clear();
        return result;
      }
      continue;
    }
    const auto stem = prefix + std::to_string(result.states.size());
    result.states.push_back({dependency,
                             stem + "_construct",
                             stem + "_destroy",
                             stem + "_address",
                             serializers ? stem + "_serialize" : "",
                             ++next_initializer_by_scope[dependency.scope_type],
                             {},
                             {},
                             {},
                             {}});
  }
  if (result.states.empty()) {
    result.error = "invalid_owner_source: no state definitions to own";
    return result;
  }
  // ScopeInit contains only a constructor-scoped borrow of the creating Call.
  // It cannot manufacture an active-hook capability in lifecycle execution.
  std::ostringstream source;
  source << original.str() << '\n';
  if (serializers) {
    // Force generic payload specializations complete before generated codec
    // templates are parsed. Otherwise Clang can emit different parameter
    // attributes for the same codec depending on whether the source happened
    // to instantiate that specialization earlier.
    for (const auto &state : result.states) {
      if (state.type.generic) {
        source << "static_assert(sizeof("
               << source_cpp_type(state.type.payload_type)
               << ") > 0, \"generic context payload must be complete\");\n";
      }
    }
  }
  source
      << "\n#include \"context_engine.hpp\"\n#include <type_traits>\n"
      << "namespace tracer::detail { struct ScopeInitAccess {\n"
         "static ScopeInit make(const Call& call) noexcept { return "
         "ScopeInit(call); } }; }\n"
         "inline const tracer::Call& tracer::ScopeInit::root_call() const { "
         "return *root_call_; }\n"
         "namespace "
      << prefix
      << " {\n"
         "namespace ce = ::tracer::context_engine;\n"
         "template<class T, bool Generic, unsigned Identity> ce::ErrorCode "
         "construct(const ce::ScopeInit& "
         "init, std::any& payload) noexcept {\n"
         "static_assert(std::is_nothrow_destructible_v<T>, \"state "
         "destruction must be noexcept\");\n"
         "if (payload.has_value()) return "
         "ce::ErrorCode::incompatible_registration;\n"
         "try {\nif constexpr (Generic) {\n"
         "if (init.payload_source && init.payload_identity == Identity) {\n"
         "if constexpr (std::is_copy_constructible_v<T>) payload = "
         "std::make_shared<T>(*static_cast<const T*>(init.payload_source));\n"
         "else return ce::ErrorCode::construction_failed;\n"
         "} else { if constexpr (std::is_default_constructible_v<T>) payload = "
         "std::make_shared<T>();\n"
         "else return ce::ErrorCode::construction_failed; }\n"
         "} else if constexpr (std::is_constructible_v<T, const "
         "::tracer::ScopeInit&>) {\n"
         "if (!init.root_call) return ce::ErrorCode::construction_failed;\n"
         "const auto view = "
         "::tracer::detail::ScopeInitAccess::make(*static_cast<const "
         "::tracer::Call*>(init.root_call));\n"
         "payload = std::make_shared<T>(view);\n"
         "} else { static_assert(std::is_default_constructible_v<T>, \"no "
         "supported state construction plan\");\n"
         "payload = std::make_shared<T>(); }\nreturn ce::ErrorCode::ok;\n"
         "} catch (...) { return ce::ErrorCode::construction_failed; } }\n"
         "template<class T> void destroy(std::any& payload) noexcept { "
         "payload.reset(); }\n"
         "template<class T> void* address(std::any& payload) noexcept {\n"
         "auto* value = std::any_cast<std::shared_ptr<T>>(&payload); return "
         "value ? value->get() : nullptr; }\n}\n";
  for (const auto &state : result.states) {
    const auto type = state.type.generic ? state.type.payload_type
                                         : "::" + state.type.state_type;
    const auto source_type = state.type.generic ? source_cpp_type(type) : type;
    source << "::tracer::context_engine::ErrorCode " << state.construct
           << "(const ::tracer::context_engine::ScopeInit& init, std::any& "
              "payload) noexcept { return "
           << prefix << "::construct<" << source_type << ","
           << (state.type.generic ? "true" : "false") << ","
           << state.initializer_identity << ">(init, payload); }\n"
           << "void " << state.destroy << "(std::any& payload) noexcept { "
           << prefix << "::destroy<" << source_type << ">(payload); }\n"
           << "void* " << state.address
           << "(std::any& payload) noexcept { return " << prefix << "::address<"
           << source_type << ">(payload); }\n";
  }
  if (serializers) {
    source
        << "\n#include \"context_codec.hpp\"\n#include \"bounded_proto.cpp\"\n";
    std::set<std::string> emitted;
    const auto render_record =
        [&](auto &&self, clang::DeclContext *context,
            const std::string &name) -> const clang::CXXRecordDecl * {
      for (auto *decl : context->decls()) {
        if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
          if (record->isThisDeclarationADefinition() &&
              record->getQualifiedNameAsString() == name) {
            return record;
          }
        }
        if (auto *space = llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
          if (auto *found = self(self, space, name)) {
            return found;
          }
        }
      }
      return nullptr;
    };
    std::vector<const clang::CXXRecordDecl *> codec_roots;
    std::vector<clang::QualType> payload_roots;
    GenericPayloadTypeFinder payload_types;
    payload_types.TraverseDecl(unit.getASTContext().getTranslationUnitDecl());
    for (const auto &state : result.states) {
      const auto &type =
          state.type.generic ? state.type.payload_type : state.type.state_type;
      if (state.type.generic) {
        const auto payload = payload_types.find(state.type.scope_type, type);
        if (payload.isNull()) {
          result.error = "invalid_owner_source: generic payload QualType is "
                         "absent from the checked AST";
          result.states.clear();
          return result;
        }
        payload_roots.push_back(payload);
        continue;
      }
      const auto *record = render_record(
          render_record, unit.getASTContext().getTranslationUnitDecl(), type);
      if (!record || !emitted.insert(type).second) {
        continue;
      }
      codec_roots.push_back(record);
    }
    const auto codecs = context_engine::render_context_codec_sources(
        unit.getASTContext(), codec_roots, payload_roots);
    if (!codecs.error.empty()) {
      result.error = codecs.error;
      result.states.clear();
      return result;
    }
    source << codecs.source << '\n';
    for (const auto &state : result.states) {
      const auto type = state.type.generic ? state.type.payload_type
                                           : "::" + state.type.state_type;
      const auto source_type =
          state.type.generic ? source_cpp_type(type) : type;
      source
          << "bool " << state.serialize
          << "(::tracer::context_engine::BoundedProtoWriter& writer, const "
             "void* payload, ::tracer::context_engine::EncodeBudget& budget) "
             "noexcept { try { return payload && ::tracer::context_engine::";
      if (state.type.generic) {
        source << "encode_value(writer, 1, *static_cast<const " << source_type
               << "*>(payload), budget)";
      } else {
        source << "encode_state_payload(writer, *static_cast<const "
               << source_type << "*>(payload), budget)";
      }
      source << "; } catch (...) { return false; } }\n";
    }
  }
  const auto copy = source.str();
  const auto additions = copy.substr(original.size());
  // Active macros must not rewrite the engine's factory names, private
  // operations or qualified state names after the original declarations.
  for (std::size_t i = 0; i < additions.size();) {
    const auto identifier_char = [](unsigned char value) {
      return std::isalnum(value) || value == '_';
    };
    if (!std::isalpha(static_cast<unsigned char>(additions[i])) &&
        additions[i] != '_') {
      ++i;
      continue;
    }
    const auto start = i++;
    while (i < additions.size() && identifier_char(additions[i])) {
      ++i;
    }
    if (unit.getPreprocessor().isMacroDefined(
            additions.substr(start, i - start))) {
      result.error = "unsupported_owner_macro: active macro changes generated "
                     "factory source";
      result.states.clear();
      return result;
    }
  }
  result.compilation_copy = copy;
  return result;
}
} // namespace tracer

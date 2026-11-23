#include "context_target.hpp"

#include <cctype>
#include <memory>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <llvm/Support/raw_ostream.h>

#include "context_support_effects.hpp"
#include "tracer/compat.hpp"

namespace tracer {
namespace {

constexpr const char *kErasedAddress = "const void*";

// Namespaces of the runtime and names the engine generates.
bool protected_name(const std::string &name) {
  for (const auto prefix : {"std::", "__gnu_cxx::", "__cxxabiv1::", "tracer::",
                            "clangtracer_", "__tracer_", "__clang_tracer_"})
    if (::tracer::compat::starts_with(name, prefix)) return true;
  return false;
}

std::string linkage_name(clang::ASTContext &context,
                         const clang::FunctionDecl *function) {
  std::unique_ptr<clang::MangleContext> names(context.createMangleContext());
  std::string name;
  llvm::raw_string_ostream stream(name);
  if (names->shouldMangleDeclName(function))
    names->mangleName(clang::GlobalDecl(function), stream);
  else
    stream << function->getName();
  return name;
}

// The name goes into an asm label inside a string literal.
bool plain_symbol(const std::string &symbol) {
  if (symbol.empty()) return false;
  for (const unsigned char c : symbol)
    if (!std::isalnum(c) && c != '_' && c != '.' && c != '$') return false;
  return true;
}

// Spelling that names the type from any scope of the callback source; empty
// when the type has no such name.
std::string spell(clang::ASTContext &context, clang::QualType type) {
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressUnwrittenScope = false;
  auto text = clang::TypeName::getFullyQualifiedName(
      type.getCanonicalType(), context, policy, /*WithGlobalNsPrefix=*/true);
  if (text.find('(') != std::string::npos) return {};
  return text;
}

// The builtin type that carries an arithmetic or enumeration value; empty when
// the type is not one the bridge passes by value.
std::string scalar_carrier(clang::ASTContext &context, clang::QualType type) {
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isEnumeralType()) {
    const auto *enumeration =
        llvm::dyn_cast_or_null<clang::EnumDecl>(type->getAsTagDecl());
    if (!enumeration || enumeration->getIntegerType().isNull()) return {};
    type = enumeration->getIntegerType().getCanonicalType().getUnqualifiedType();
  }
  const auto *builtin = type->getAs<clang::BuiltinType>();
  if (!builtin) return {};
  const bool floating = builtin->getKind() == clang::BuiltinType::Float ||
                        builtin->getKind() == clang::BuiltinType::Double ||
                        builtin->getKind() == clang::BuiltinType::LongDouble;
  if (!floating && !builtin->isInteger()) return {};
  clang::PrintingPolicy policy(context.getLangOpts());
  return clang::QualType(builtin, 0).getAsString(policy);
}

bool object_pointee(clang::QualType pointee) {
  return !pointee->isFunctionType() && !pointee.isVolatileQualified();
}

bool standard_string(clang::ASTContext &context, clang::QualType type) {
  const auto *record = type.getCanonicalType()->getAsCXXRecordDecl();
  const auto *specialization =
      llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(record);
  if (!specialization) return false;
  const auto name =
      specialization->getSpecializedTemplate()->getQualifiedNameAsString();
  if (!::tracer::compat::starts_with(name, "std::") ||
      !::tracer::compat::ends_with(name, "basic_string") ||
      !context_source_is_trusted_support(context.getSourceManager(),
                                         specialization->getLocation()))
    return false;
  const auto &arguments = specialization->getTemplateArgs();
  if (arguments.size() != 3 ||
      arguments[0].getKind() != clang::TemplateArgument::Type ||
      arguments[1].getKind() != clang::TemplateArgument::Type ||
      arguments[2].getKind() != clang::TemplateArgument::Type)
    return false;
  const auto named = [](clang::QualType argument, const char *suffix) {
    const auto *declaration = argument->getAsCXXRecordDecl();
    const auto *instance =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            declaration);
    return instance &&
           ::tracer::compat::ends_with(
               instance->getSpecializedTemplate()->getQualifiedNameAsString(),
               suffix);
  };
  return context.hasSameType(arguments[0].getAsType(), context.CharTy) &&
         named(arguments[1].getAsType(), "char_traits") &&
         named(arguments[2].getAsType(), "allocator");
}

// Fills `parameter` for a declared parameter type; false with a reason when
// the bridge cannot pass it.
bool describe_parameter(clang::ASTContext &context, clang::QualType type,
                        ContextTargetParameter &parameter,
                        std::string &reason) {
  const auto described = [&](const char *kind) {
    reason = std::string(kind) + " of type " + type.getAsString();
    return false;
  };
  if (type->isRValueReferenceType())
    return described("it takes an rvalue reference");
  if (type->isLValueReferenceType() || type->isPointerType()) {
    if (!object_pointee(type->getPointeeType()))
      return described("it takes a function or volatile address");
    parameter.erased = kErasedAddress;
  } else if (type->isNullPtrType()) {
    parameter.erased = kErasedAddress;
  } else {
    parameter.erased = scalar_carrier(context, type);
    if (parameter.erased.empty())
      return described("it takes a by-value parameter");
    type = type.getUnqualifiedType();
  }
  parameter.type = spell(context, type);
  if (parameter.type.empty())
    return described("it has a parameter that the callback cannot name,");
  return true;
}

bool describe_result(clang::ASTContext &context, clang::QualType type,
                     ContextTargetBridge &bridge, std::string &reason) {
  const auto described = [&](const char *kind) {
    reason = std::string(kind) + " of type " + type.getAsString() +
             ", which cannot cross from the target into a context hook";
    return false;
  };
  if (type->isVoidType()) {
    bridge.result = ContextTargetValue::none;
    bridge.result_type = "void";
    return true;
  }
  if (type->isRValueReferenceType())
    return described("it returns an rvalue reference");
  if (type->isLValueReferenceType() || type->isPointerType()) {
    if (!object_pointee(type->getPointeeType()))
      return described("it returns a function or volatile address");
    bridge.result = ContextTargetValue::address;
  } else if (standard_string(context, type)) {
    bridge.result = ContextTargetValue::text;
    type = type.getUnqualifiedType();
  } else {
    bridge.result_erased = scalar_carrier(context, type);
    if (bridge.result_erased.empty())
      return described("it returns a value");
    bridge.result = ContextTargetValue::scalar;
    type = type.getUnqualifiedType();
  }
  bridge.result_type = spell(context, type);
  if (bridge.result_type.empty())
    return described("it returns a value the callback cannot name,");
  return true;
}

// True when the expression names an object that the hook itself built: a local
// or by-value parameter of class type, a part of one, or a temporary. Such an
// object owns memory of the hook's runtime.
bool hook_object(const clang::Expr *expression) {
  for (;;) {
    expression = expression->IgnoreParenImpCasts();
    if (llvm::isa<clang::MaterializeTemporaryExpr>(expression) ||
        llvm::isa<clang::CXXBindTemporaryExpr>(expression) ||
        llvm::isa<clang::CXXTemporaryObjectExpr>(expression) ||
        llvm::isa<clang::CXXConstructExpr>(expression))
      return true;
    if (const auto *address = llvm::dyn_cast<clang::UnaryOperator>(expression)) {
      if (address->getOpcode() != clang::UO_AddrOf) return false;
      expression = address->getSubExpr();
    } else if (const auto *member =
                   llvm::dyn_cast<clang::MemberExpr>(expression)) {
      if (member->isArrow() ||
          member->getMemberDecl()->getType()->isReferenceType())
        return false;
      expression = member->getBase();
    } else if (const auto *element =
                   llvm::dyn_cast<clang::ArraySubscriptExpr>(expression)) {
      if (!element->getBase()->IgnoreParenImpCasts()->getType()->isArrayType())
        return false;
      expression = element->getBase();
    } else {
      break;
    }
  }
  const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(expression);
  const auto *variable = reference
      ? llvm::dyn_cast<clang::VarDecl>(reference->getDecl()) : nullptr;
  return variable && variable->hasLocalStorage() &&
         !variable->getType()->isReferenceType() &&
         !variable->getType()->isPointerType();
}

// The target could modify, and so reallocate, a hook-built class object that
// it receives through a pointer or reference to non-const.
bool lends_hook_object(clang::QualType parameter, const clang::Expr *argument) {
  if (!parameter->isPointerType() && !parameter->isLValueReferenceType())
    return false;
  const auto pointee = parameter->getPointeeType();
  return !pointee.isConstQualified() && pointee->isRecordType() &&
         hook_object(argument);
}

std::string erased_result(const ContextTargetBridge &bridge) {
  switch (bridge.result) {
  case ContextTargetValue::none: return "void";
  case ContextTargetValue::scalar: return bridge.result_erased;
  case ContextTargetValue::address: return kErasedAddress;
  case ContextTargetValue::text: return "std::string";
  }
  return "void";
}

} // namespace

std::string ContextTargetBridge::key() const {
  std::string value = (virtual_dispatch ? "virtual " : "direct ") + symbol +
                      " -> " + result_type;
  for (const auto &parameter : parameters) value += " | " + parameter.type;
  return value;
}

bool is_context_target_function(clang::ASTContext &context,
                                const clang::FunctionDecl *function) {
  if (!function || function->hasBody() || function->getBuiltinID() != 0 ||
      function->isImplicit() || function->isDefaulted() ||
      function->isDeleted())
    return false;
  if (protected_name(function->getQualifiedNameAsString())) return false;
  const auto &sources = context.getSourceManager();
  for (const auto *declaration : function->redecls())
    if (context_source_is_trusted_support(sources, declaration->getLocation()))
      return false;
  return true;
}

ContextTargetCallKind
classify_context_target_call(clang::ASTContext &context,
                             const clang::CallExpr &call,
                             ContextTargetCallSite &site, std::string &reason) {
  const auto *callee = call.getDirectCallee();
  if (!is_context_target_function(context, callee))
    return ContextTargetCallKind::not_target;
  const auto unsupported = [&](std::string why) {
    reason = std::move(why);
    return ContextTargetCallKind::unsupported;
  };
  if (llvm::isa<clang::CXXOperatorCallExpr>(&call))
    return unsupported("an overloaded operator cannot be called through the "
                       "target-call bridge");
  if (llvm::isa<clang::CXXConstructorDecl>(callee) ||
      llvm::isa<clang::CXXDestructorDecl>(callee) ||
      llvm::isa<clang::CXXConversionDecl>(callee))
    return unsupported("a constructor, destructor or conversion function "
                       "cannot be called through the target-call bridge");
  if (callee->isVariadic())
    return unsupported("it takes a variable argument list");
  auto &bridge = site.bridge;
  bridge = {};
  site.object = nullptr;
  site.arrow = false;
  bridge.name = callee->getQualifiedNameAsString();
  bridge.symbol = linkage_name(context, callee);
  if (!plain_symbol(bridge.symbol))
    return unsupported("its linkage name cannot be bound");
  const auto *written = call.getCallee()->IgnoreParens();
  if (const auto *member_call = llvm::dyn_cast<clang::CXXMemberCallExpr>(&call)) {
    const auto *method = member_call->getMethodDecl();
    const auto *member = llvm::dyn_cast<clang::MemberExpr>(written);
    if (!method || !member || member->isImplicitAccess())
      return unsupported("its receiver is not written at the call");
    if (method->getRefQualifier() == clang::RQ_RValue || method->isVolatile())
      return unsupported("it is volatile or rvalue-reference qualified");
    site.object = member->getBase();
    site.arrow = member->isArrow();
    if (!site.arrow && !method->isConst() && !site.object->isLValue())
      return unsupported("a member function that is not const is called on a "
                         "temporary");
    const auto object_type = method->getThisType()->getPointeeType();
    if (!method->isConst() && hook_object(site.object))
      return unsupported("a member function that is not const is called on "
                         "an object the hook built; the target must not "
                         "modify an object of the hook's runtime");
    ContextTargetParameter receiver;
    receiver.type = spell(context, object_type);
    if (receiver.type.empty())
      return unsupported("the callback cannot name its class");
    receiver.type += " &";
    receiver.erased = kErasedAddress;
    bridge.member = true;
    bridge.parameters.push_back(std::move(receiver));
    if (method->isVirtual() && !member->hasQualifier()) {
      auto *tables =
          llvm::dyn_cast<clang::ItaniumVTableContext>(context.getVTableContext());
      if (!tables || !method->getParent()->isCompleteDefinition())
        return unsupported("its virtual table layout is not known");
      bridge.virtual_dispatch = true;
      bridge.vtable_index = static_cast<unsigned>(
          tables->getMethodVTableIndex(clang::GlobalDecl(method)));
    }
  } else {
    const auto *named = call.getCallee()->IgnoreParenImpCasts();
    if (llvm::isa<clang::MemberExpr>(named))
      return unsupported("a static member function has to be called by its "
                         "qualified name");
    if (!llvm::isa<clang::DeclRefExpr>(named))
      return unsupported("it is not named directly at the call");
  }
  if (call.getNumArgs() != callee->getNumParams())
    return unsupported("the call does not pass one argument per parameter");
  for (unsigned index = 0; index < call.getNumArgs(); ++index) {
    if (llvm::isa<clang::CXXDefaultArgExpr>(call.getArg(index)))
      return unsupported("argument " + std::to_string(index + 1) +
                         " uses its default value; pass every argument "
                         "explicitly");
    if (lends_hook_object(callee->getParamDecl(index)->getType(),
                          call.getArg(index)))
      return unsupported("argument " + std::to_string(index + 1) +
                         " is an object the hook built, passed for the "
                         "target to modify; the target must not modify an "
                         "object of the hook's runtime");
    ContextTargetParameter parameter;
    std::string why;
    if (!describe_parameter(context, callee->getParamDecl(index)->getType(),
                            parameter, why))
      return unsupported(why + " (parameter " + std::to_string(index + 1) +
                         "); the bridge passes arithmetic and enumeration "
                         "values, pointers and lvalue references");
    bridge.parameters.push_back(std::move(parameter));
  }
  std::string why;
  if (!describe_result(context, callee->getReturnType(), bridge, why))
    return unsupported(why);
  return ContextTargetCallKind::supported;
}

std::string context_target_bridge_symbol(std::size_t index) {
  return "clangtracer_target_call_" + std::to_string(index);
}

std::string
context_target_declarations(const std::vector<ContextTargetBridge> &bridges) {
  std::string source;
  for (std::size_t index = 0; index < bridges.size(); ++index) {
    source += "extern \"C\" bool " + context_target_bridge_symbol(index) +
              "(void*";
    for (const auto &parameter : bridges[index].parameters)
      source += ", " + parameter.erased;
    source += ") noexcept;\n";
  }
  return source;
}

std::string context_target_callee(const ContextTargetBridge &bridge,
                                  std::size_t index) {
  std::string text = "::tracer::context_facade::target_call<::" +
                     context_target_bridge_symbol(index) + ", " +
                     bridge.result_type;
  for (const auto &parameter : bridge.parameters) text += ", " + parameter.type;
  return text + ">";
}

bool context_target_call_edits(clang::ASTContext &context,
                               const clang::CallExpr &call,
                               const ContextTargetCallSite &site,
                               const std::string &callee,
                               std::vector<ContextTargetEdit> &edits) {
  const auto &sources = context.getSourceManager();
  const auto begin = [&](clang::SourceLocation location, std::size_t &offset) {
    if (location.isInvalid() || location.isMacroID() ||
        !sources.isWrittenInMainFile(location))
      return false;
    offset = sources.getFileOffset(location);
    return true;
  };
  const auto end = [&](clang::SourceLocation location, std::size_t &offset) {
    if (location.isInvalid() || location.isMacroID()) return false;
    return begin(clang::Lexer::getLocForEndOfToken(location, 0, sources,
                                                   context.getLangOpts()),
                 offset);
  };
  std::size_t extent = 0;
  if (!end(call.getEndLoc(), extent)) return false;
  if (!site.object) {
    std::size_t first = 0, last = 0;
    if (!begin(call.getCallee()->getBeginLoc(), first) ||
        !end(call.getCallee()->getEndLoc(), last) || last <= first ||
        last > extent)
      return false;
    edits.push_back({first, last - first, callee, extent});
    return true;
  }
  std::size_t object_begin = 0, object_end = 0, arguments = 0;
  if (!begin(site.object->getBeginLoc(), object_begin) ||
      !end(site.object->getEndLoc(), object_end) ||
      !begin(call.getNumArgs() ? call.getArg(0)->getBeginLoc()
                               : call.getRParenLoc(),
             arguments) ||
      object_end <= object_begin || arguments < object_end ||
      arguments >= extent)
    return false;
  edits.push_back(
      {object_begin, 0, callee + (site.arrow ? "(*(" : "(("), extent});
  edits.push_back({object_end, arguments - object_end,
                   call.getNumArgs() ? "), " : ")", extent});
  return true;
}

std::string
context_target_thunks(const std::vector<ContextTargetBridge> &bridges) {
  std::string source =
      "#include <cstddef>\n#include <string>\n"
      "extern \"C\" bool tracer_target_call_enter() noexcept;\n"
      "extern \"C\" void tracer_target_call_leave() noexcept;\n"
      "namespace clangtracer_target_detail {\n"
      "struct Text { const char* data; std::size_t size; void* handle; };\n"
      "struct Scope {\n"
      "  const bool entered = tracer_target_call_enter();\n"
      "  ~Scope() { if (entered) tracer_target_call_leave(); }\n"
      "};\n"
      "}\n"
      "extern \"C\" void " +
      std::string(kContextTargetReleaseText) +
      "(void* handle) noexcept {\n"
      "  delete static_cast<std::string*>(handle);\n"
      "}\n";
  for (std::size_t index = 0; index < bridges.size(); ++index) {
    const auto &bridge = bridges[index];
    const auto number = std::to_string(index);
    std::string erased_parameters, named_parameters, arguments;
    for (std::size_t i = 0; i < bridge.parameters.size(); ++i) {
      const auto argument = "a" + std::to_string(i);
      if (i) {
        erased_parameters += ", ";
        arguments += ", ";
      }
      erased_parameters += bridge.parameters[i].erased;
      named_parameters += ", " + bridge.parameters[i].erased + " " + argument;
      arguments += argument;
    }
    const auto result = erased_result(bridge);
    std::string invoke;
    source += "// " + bridge.name + "\n";
    if (bridge.virtual_dispatch) {
      invoke = "(*static_cast<const Function* const*>(a0))[" +
               std::to_string(bridge.vtable_index) + "](" + arguments + ")";
    } else {
      source += result + " clangtracer_target_function_" + number + "(" +
                erased_parameters + ") __asm__(\"" + bridge.symbol + "\");\n";
      invoke = "clangtracer_target_function_" + number + "(" + arguments + ")";
    }
    source += "extern \"C\" bool " + context_target_bridge_symbol(index) +
              "(void* out" + named_parameters +
              ") noexcept {\n"
              "  clangtracer_target_detail::Scope scope;\n"
              "  if (!scope.entered) return false;\n"
              "  try {\n";
    if (bridge.virtual_dispatch)
      source += "    using Function = " + result + " (*)(" + erased_parameters +
                ");\n";
    switch (bridge.result) {
    case ContextTargetValue::none:
      source += "    (void)out;\n    " + invoke + ";\n";
      break;
    case ContextTargetValue::scalar:
    case ContextTargetValue::address:
      source += "    *static_cast<" + result + "*>(out) = " + invoke + ";\n";
      break;
    case ContextTargetValue::text:
      source += "    auto* value = new std::string(" + invoke +
                ");\n"
                "    *static_cast<clangtracer_target_detail::Text*>(out) = "
                "{value->data(), value->size(), value};\n";
      break;
    }
    source += "    return true;\n"
              "  } catch (...) {\n"
              "    return false;\n"
              "  }\n"
              "}\n";
  }
  return source;
}

} // namespace tracer

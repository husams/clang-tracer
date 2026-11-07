#include "cpp_resolver.hpp"
#include "header_context.hpp"
#include "prepared_types.hpp"
#include "prepared_adapter.hpp"
#include "prepared_contract.hpp"
#include "tracer_runtime_config.hpp"

#include "../include/tracer/receiver_abi.hpp"
#include "tracer/compat.hpp"

#ifndef TRACER_CLANG_RESOURCE_DIR
#error                                                                                             \
    "clang-tracer must be built with TRACER_CLANG_RESOURCE_DIR set to its Clang resource directory"
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Basic/Version.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#if __has_include(<llvm/TargetParser/Triple.h>)
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#endif
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

namespace tracer::client {
namespace {

namespace fs = std::filesystem;
namespace api = clangtracer::v1;

constexpr std::size_t kMaxTranslationUnits = 256;
constexpr std::size_t kMaxExplicitSources = 128;
constexpr std::size_t kMaxArguments = 8;
constexpr std::size_t kMaxReceiverFields = 128;
constexpr std::size_t kMaxReceiverSourcePath = 4096;
constexpr std::size_t kMaxReceiverArguments = 512;
constexpr std::size_t kMaxReceiverArgumentBytes = 65536;

std::string trim(std::string value) {
  const auto first =
      std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
                      return std::isspace(c);
                    }).base();
  return first < last ? std::string(first, last) : std::string();
}

std::string compact(std::string_view value) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const auto c = static_cast<unsigned char>(value[i]);
    if (!std::isspace(c)) {
      result.push_back(static_cast<char>(c));
      continue;
    }
    std::size_t next = i + 1;
    while (next < value.size() && std::isspace(static_cast<unsigned char>(value[next]))) {
      ++next;
    }
    const auto is_word = [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; };
    if (!result.empty() && next < value.size() &&
        is_word(static_cast<unsigned char>(result.back())) &&
        is_word(static_cast<unsigned char>(value[next]))) {
      result.push_back(' ');
    }
    i = next - 1;
  }
  return result;
}

struct Selector {
  std::string qualified_name;
  bool has_parameters = false;
  std::vector<std::string> parameters;
  bool is_const = false;
  bool is_volatile = false;
  clang::RefQualifierKind ref_qualifier = clang::RQ_None;
};

std::vector<std::string> split_parameters(std::string_view text) {
  std::vector<std::string> result;
  if (trim(std::string(text)).empty()) {
    return result;
  }
  std::size_t start = 0;
  int angles = 0, parens = 0, brackets = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    switch (text[i]) {
    case '<':
      ++angles;
      break;
    case '>':
      if (angles > 0) {
        --angles;
      }
      break;
    case '(':
      ++parens;
      break;
    case ')':
      if (parens > 0) {
        --parens;
      }
      break;
    case '[':
      ++brackets;
      break;
    case ']':
      if (brackets > 0) {
        --brackets;
      }
      break;
    case ',':
      if (angles == 0 && parens == 0 && brackets == 0) {
        result.push_back(trim(std::string(text.substr(start, i - start))));
        start = i + 1;
      }
      break;
    default:
      break;
    }
  }
  result.push_back(trim(std::string(text.substr(start))));
  return result;
}

Selector parse_selector(const std::string& text) {
  Selector selector;
  auto open = std::string::npos;
  unsigned angles = 0, parentheses = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '(') {
      if (!angles) {
        open = i;
        break;
      }
      ++parentheses;
    } else if (ch == ')' && parentheses) {
      --parentheses;
    } else if (!parentheses && ch == '<') {
      ++angles;
    } else if (!parentheses && ch == '>' && angles) {
      --angles;
    }
  }
  if (open == std::string::npos)
    open = text.find('('); // Preserve legacy parsing for operator spellings.
  if (open == std::string::npos) {
    selector.qualified_name = trim(text);
    if (selector.qualified_name.empty()) {
      throw std::runtime_error("callable name is empty");
    }
    return selector;
  }
  const auto close = text.rfind(')');
  if (close == std::string::npos || close < open) {
    throw std::runtime_error("callable selector has an unmatched parameter list");
  }
  selector.qualified_name = trim(text.substr(0, open));
  if (selector.qualified_name.empty()) {
    throw std::runtime_error("callable name is empty");
  }
  selector.has_parameters = true;
  selector.parameters = split_parameters(std::string_view(text).substr(open + 1, close - open - 1));
  auto suffix = compact(trim(text.substr(close + 1)));
  while (!suffix.empty()) {
    suffix = trim(std::move(suffix));
    if (::tracer::compat::starts_with(suffix, "const")) {
      if (selector.is_const) {
        throw std::runtime_error("duplicate const qualifier in selector");
      }
      selector.is_const = true;
      suffix.erase(0, 5);
    } else if (::tracer::compat::starts_with(suffix, "volatile")) {
      if (selector.is_volatile) {
        throw std::runtime_error("duplicate volatile qualifier in selector");
      }
      selector.is_volatile = true;
      suffix.erase(0, 8);
    } else if (::tracer::compat::starts_with(suffix, "&&")) {
      if (selector.ref_qualifier != clang::RQ_None) {
        throw std::runtime_error("duplicate ref qualifier in selector");
      }
      selector.ref_qualifier = clang::RQ_RValue;
      suffix.erase(0, 2);
    } else if (::tracer::compat::starts_with(suffix, "&")) {
      if (selector.ref_qualifier != clang::RQ_None) {
        throw std::runtime_error("duplicate ref qualifier in selector");
      }
      selector.ref_qualifier = clang::RQ_LValue;
      suffix.erase(0, 1);
    } else {
      throw std::runtime_error("unsupported callable qualifier: " + suffix);
    }
  }
  return selector;
}

struct Candidate {
  std::string name;
  std::string symbol;
  std::string declaration;
  std::string source;
  std::string canonical_identity;
  std::vector<std::string> parameter_spellings;
  std::vector<std::string> canonical_parameter_spellings;
  bool is_const = false;
  bool is_volatile = false;
  clang::RefQualifierKind ref_qualifier = clang::RQ_None;
  api::ProbeSpec spec;
  std::string unsupported_reason;
  bool selected_by_type = false;
  bool requires_typed_adapter = false;
};

struct FrozenInvocation {
  std::string working_directory;
  std::string source_argument;
  std::vector<std::string> arguments;
  std::vector<std::string> prepared_headers;
  std::string adapter_symbol;
  bool select_template = false;
  bool pointer_layouts = true;
};

class PrepareDiagnostics final : public clang::TextDiagnosticPrinter {
public:
  explicit PrepareDiagnostics(clang::DiagnosticOptions& options)
      : clang::TextDiagnosticPrinter(llvm::errs(), options) {}

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic& diagnostic) override {
    had_error_ |= level >= clang::DiagnosticsEngine::Error;
    clang::TextDiagnosticPrinter::HandleDiagnostic(level, diagnostic);
  }

  // A ToolInvocation can continue into the frontend after a driver error.
  bool had_error() const { return had_error_; }

private:
  bool had_error_ = false;
};

bool is_receiver_field_type(clang::QualType type) {
  type = type.getCanonicalType();
  if (const auto* array = llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr())) {
    return is_receiver_field_type(array->getElementType());
  }
  if (type->isPointerType()) {
    auto pointee = type->getPointeeType().getCanonicalType();
    return pointee->isVoidType() || pointee->isBuiltinType();
  }
  return type->isBuiltinType() && type->isScalarType();
}

std::string receiver_layout_identity(const api::ReceiverContext& receiver) {
  std::string identity = "|receiver=" + receiver.record_name() +
                         "|size=" + std::to_string(receiver.size()) +
                         "|align=" + std::to_string(receiver.alignment()) +
                         "|volatile=" + (receiver.is_volatile() ? "1" : "0");
  for (const auto& field : receiver.fields()) {
    identity += "|field=" + field.name() + ":" + field.canonical_type() + ":" +
                std::to_string(field.bit_offset()) + ":" + std::to_string(field.bit_width()) + ":" +
                std::to_string(field.bit_size()) + ":" + std::to_string(field.bit_alignment()) +
                ":" + (field.is_mutable() ? "1" : "0") + ":" + field.access() + ":" +
                field.abi_identity();
  }
  return identity;
}

std::string type_spelling(clang::QualType type, const clang::PrintingPolicy& policy) {
  return type.getAsString(policy);
}

api::ValueType scalar_type(clang::QualType type, clang::ASTContext& context, bool is_result) {
  if (type->isReferenceType()) {
    throw std::runtime_error("reference parameters and results are unsupported");
  }
  type = type.getCanonicalType().getUnqualifiedType();
  if (type->isVoidType()) {
    if (is_result) {
      return api::VOID;
    }
    throw std::runtime_error("void parameter is invalid");
  }
  if (type->isPointerType()) {
    const auto pointer = type.getCanonicalType();
    const auto pointee = pointer->getPointeeType();
    if (pointer.getAddressSpace() != clang::LangAS::Default ||
        pointee.getAddressSpace() != clang::LangAS::Default || context.getTypeSize(pointer) != 64) {
      throw std::runtime_error("non-default-address-space or non-64-bit pointers are unsupported");
    }
    return api::POINTER;
  }
  if (type->isMemberPointerType() || type->isBlockPointerType()) {
    throw std::runtime_error("member and block pointers are unsupported");
  }
  if (type->isBooleanType() || type->isCharType() || type->isBitIntType()) {
    throw std::runtime_error("bool, character, and bit-precise integer types are unsupported");
  }
  if (type->isFloatingType()) {
    const auto bits = context.getTypeSize(type);
    if (bits == 32) {
      return api::FLOAT32;
    }
    if (bits == 64) {
      return api::FLOAT64;
    }
    throw std::runtime_error("floating type width is unsupported");
  }
  if (type->isIntegerType() || type->isEnumeralType()) {
    const auto bits = context.getTypeSize(type);
    const bool is_signed = type->isSignedIntegerOrEnumerationType();
    if (bits == 32 && is_signed) {
      return api::INT32;
    }
    if (bits == 64 && is_signed) {
      return api::INT64;
    }
    if (bits == 64 && !is_signed) {
      return api::UINT64;
    }
    throw std::runtime_error("integer width or signedness has no exact trace value type");
  }
  throw std::runtime_error("aggregate, vector, and other non-scalar types are unsupported");
}

class Collector : public clang::RecursiveASTVisitor<Collector> {
public:
  Collector(std::vector<Candidate>& candidates, const std::string& qualified_name,
            FrozenInvocation invocation, std::string translation_unit, std::string target_triple,
            clang::Sema *sema)
      : candidates_(candidates)
      , qualified_name_(qualified_name)
      , invocation_(std::move(invocation))
      , translation_unit_(std::move(translation_unit))
      , target_triple_(std::move(target_triple)), sema_(sema) {}

  bool VisitFunctionDecl(clang::FunctionDecl* declaration) {
    if (invocation_.select_template)
      return true;
    if (!invocation_.adapter_symbol.empty()) {
      if (!tracer::prepared::adapter_callable(*declaration, *context_, qualified_name_,
                                             invocation_.adapter_symbol))
        return true;
    } else if (declaration->getQualifiedNameAsString() != qualified_name_) {
      return true;
    }
    return collect(declaration, false);
  }

  bool VisitTypeAliasDecl(clang::TypeAliasDecl *alias) {
    if (!invocation_.select_template ||
        alias->getQualifiedNameAsString() != "tracer_prepared_selection::Selected")
      return true;
    const auto *type = llvm::dyn_cast<clang::DecltypeType>(alias->getUnderlyingType().getTypePtr());
    const auto *call = type ? llvm::dyn_cast<clang::CallExpr>(
        type->getUnderlyingExpr()->IgnoreParenImpCasts()) : nullptr;
    const auto *address = call && call->getNumArgs() == 1
        ? llvm::dyn_cast<clang::UnaryOperator>(call->getArg(0)->IgnoreParenImpCasts()) : nullptr;
    const auto *reference = address && address->getOpcode() == clang::UO_AddrOf
        ? llvm::dyn_cast<clang::DeclRefExpr>(address->getSubExpr()->IgnoreParenImpCasts()) : nullptr;
    auto *function = reference ? llvm::dyn_cast<clang::FunctionDecl>(reference->getDecl()) : nullptr;
    return function ? collect(function, true) : true;
  }

  bool collect(const clang::FunctionDecl *declaration, bool selected_by_type) {
    auto* prototype = declaration->getType()->getAs<clang::FunctionProtoType>();
    if (!prototype) {
      return true;
    }
    const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration);

    Candidate candidate;
    candidate.name = selected_by_type ? qualified_name_ : declaration->getQualifiedNameAsString();
    candidate.selected_by_type = selected_by_type;
    candidate.requires_typed_adapter = tracer::prepared::template_instance(*declaration) ||
                                       tracer::prepared::nested_static_method(method);
    candidate.is_const = method && method->isConst();
    candidate.is_volatile = method && method->isVolatile();
    candidate.ref_qualifier = method ? method->getRefQualifier() : clang::RQ_None;
    clang::PrintingPolicy policy(context_->getLangOpts());
    policy.SuppressScope = false;
    for (const auto* parameter : declaration->parameters()) {
      candidate.parameter_spellings.push_back(type_spelling(parameter->getType(), policy));
      candidate.spec.add_argument_names(parameter->getName().empty()
          ? "arg" + std::to_string(candidate.parameter_spellings.size() - 1)
          : parameter->getNameAsString());
    }
    for (unsigned i = 0; i < prototype->getNumParams(); ++i) {
      candidate.canonical_parameter_spellings.push_back(
          type_spelling(prototype->getParamType(i).getCanonicalType(), policy));
    }
    candidate.declaration =
        type_spelling(declaration->getReturnType(), policy) + " " + candidate.name + "(";
    for (std::size_t i = 0; i < candidate.parameter_spellings.size(); ++i) {
      if (i) {
        candidate.declaration += ", ";
      }
      candidate.declaration += candidate.parameter_spellings[i];
    }
    candidate.declaration += ")";
    if (candidate.is_const) {
      candidate.declaration += " const";
    }
    if (candidate.is_volatile) {
      candidate.declaration += " volatile";
    }
    if (candidate.ref_qualifier == clang::RQ_LValue) {
      candidate.declaration += " &";
    }
    if (candidate.ref_qualifier == clang::RQ_RValue) {
      candidate.declaration += " &&";
    }
    candidate.source = context_->getSourceManager().getFilename(declaration->getLocation()).str();
    candidate.canonical_identity =
        declaration->getType().getCanonicalType().getAsString(policy) + "|" +
        (method && method->isStatic() ? "static"
         : method                     ? "method"
                                      : "function") +
        "|const=" + (candidate.is_const ? "1" : "0") +
        "|volatile=" + (candidate.is_volatile ? "1" : "0") +
        "|ref=" + std::to_string(static_cast<int>(candidate.ref_qualifier));

    if (method && !method->isStatic() && !sema_) {
      set_receiver(candidate, method, policy);
      if (candidate.spec.has_receiver()) {
        candidate.canonical_identity += receiver_layout_identity(candidate.spec.receiver());
      }
    }

    try {
      if (declaration->getDeclContext()->isDependentContext() ||
          declaration->getType()->isDependentType()) {
        throw std::runtime_error("dependent callable types are unsupported");
      }
      if (!sema_ && method && method->isVirtual()) {
        throw std::runtime_error("virtual methods and their possible thunks are unsupported");
      }
      if (llvm::isa<clang::CXXConstructorDecl>(declaration) ||
          llvm::isa<clang::CXXDestructorDecl>(declaration)) {
        throw std::runtime_error("constructors and destructors are unsupported");
      }
      if (declaration->getFormalLinkage() != clang::Linkage::External) {
        throw std::runtime_error("callables with internal or non-external linkage are unsupported");
      }
      if (prototype->getCallConv() != clang::CC_C) {
        throw std::runtime_error("non-C calling convention is unsupported");
      }
      const auto function_info = prototype->getExtInfo();
      if (function_info.getHasRegParm() || function_info.getProducesResult() ||
          function_info.getNoCallerSavedRegs() || function_info.getCmseNSCall()) {
        throw std::runtime_error("ABI-changing function attributes are unsupported");
      }
      if (invocation_.adapter_symbol.empty() && declaration->hasAttr<clang::AsmLabelAttr>()) {
        throw std::runtime_error("asm-labelled declarations are unsupported");
      }
      if (prototype->isVariadic()) {
        throw std::runtime_error("variadic callables are unsupported");
      }
      if (declaration->getNumParams() > kMaxArguments) {
        throw std::runtime_error("callables with more than eight arguments are unsupported");
      }
      if (prototype->hasExtParameterInfos()) {
        for (const auto& info : prototype->getExtParameterInfos()) {
          if (info != clang::FunctionProtoType::ExtParameterInfo{}) {
            throw std::runtime_error("ABI-annotated parameters are unsupported");
          }
        }
      }
      if (method && !method->isStatic() && !method->getParent()->isCompleteDefinition()) {
        throw std::runtime_error("incomplete class method is unsupported");
      }
      candidate.spec.set_symbol(invocation_.adapter_symbol.empty()
          ? tracer::prepared::mangle(declaration, *context_) : invocation_.adapter_symbol);
      candidate.spec.mutable_signature()->set_kind(method && !method->isStatic() ? api::METHOD
                                                                                 : api::FUNCTION);
      if (sema_) {
        tracer::prepared::describe_prepared_callable(*context_, *declaration, method,
            candidate.spec, sema_, nullptr, invocation_.pointer_layouts);
        if (!invocation_.prepared_headers.empty()) {
          auto adapter = tracer::prepared::AdapterEmitter(*context_).emit(
              *declaration, invocation_.prepared_headers, invocation_.pointer_layouts);
          auto &package = *candidate.spec.mutable_prepared();
          package.set_target_triple(context_->getTargetInfo().getTriple().str());
          package.set_adapter_declarations(std::move(adapter.source));
          for (const auto &header : adapter.standard_headers)
            package.add_adapter_standard_headers(header);
        }
      } else {
        auto *signature = candidate.spec.mutable_signature();
        signature->set_result(scalar_type(declaration->getReturnType(), *context_, true));
        for (const auto *parameter : declaration->parameters())
          signature->add_arguments(scalar_type(parameter->getType(), *context_, false));
      }
      candidate.symbol = candidate.spec.symbol();
    } catch (const std::exception& error) {
      candidate.unsupported_reason = error.what();
      candidate.symbol = candidate.name + "|" + candidate.declaration + "|unsupported";
    }
    candidates_.push_back(std::move(candidate));
    return true;
  }

  void set_context(clang::ASTContext& context, clang::Sema *sema) {
    context_ = &context;
    sema_ = sema;
  }

private:
  void set_receiver(Candidate& candidate, const clang::CXXMethodDecl* method,
                    const clang::PrintingPolicy& policy) {
    auto unavailable = [&candidate](std::string reason) {
      candidate.spec.clear_receiver();
      candidate.spec.set_receiver_unavailable_reason(std::move(reason));
    };
    const auto* record = method->getParent();
    if (!record->isCompleteDefinition() || record->isUnion() || record->isDependentType() ||
        record->getDescribedClassTemplate() ||
        llvm::isa<clang::ClassTemplateSpecializationDecl>(record) || record->isDynamicClass() ||
        record->getNumBases() != 0) {
      unavailable("receiver class has an unsupported layout");
      return;
    }
    const auto fields = record->fields();
    if (std::distance(fields.begin(), fields.end()) >
        static_cast<std::ptrdiff_t>(kMaxReceiverFields)) {
      unavailable("receiver class has more than 128 fields");
      return;
    }

    const auto& source_manager = context_->getSourceManager();
    const auto main_file = source_manager.getMainFileID();
    const auto main_location = source_manager.getLocForStartOfFile(main_file);
    const auto source_path = translation_unit_.empty()
                                 ? source_manager.getFilename(main_location).str()
                                 : translation_unit_;
    if (invocation_.arguments.empty()) {
      unavailable("resolver did not capture the translation unit compiler invocation");
      return;
    }
    std::size_t total_argument_bytes = 0;
    for (const auto& argument : invocation_.arguments) {
      if (argument.size() + 1 > kMaxReceiverArgumentBytes - total_argument_bytes) {
        unavailable("translation unit invocation exceeds receiver metadata limits");
        return;
      }
      total_argument_bytes += argument.size() + 1;
    }
    if (source_path.empty() || source_path.size() >= kMaxReceiverSourcePath ||
        invocation_.working_directory.empty() || invocation_.source_argument.empty() ||
        invocation_.arguments.size() > kMaxReceiverArguments ||
        total_argument_bytes > kMaxReceiverArgumentBytes) {
      unavailable("translation unit invocation exceeds receiver metadata limits");
      return;
    }
    for (const auto* field : fields) {
      if (field->getName().empty() || !is_receiver_field_type(field->getType())) {
        unavailable("receiver class contains an unsupported field");
        return;
      }
    }

    auto* receiver = candidate.spec.mutable_receiver();
    receiver->set_translation_unit(source_path);
    receiver->set_working_directory(invocation_.working_directory);
    for (const auto& argument : invocation_.arguments) {
      receiver->add_compiler_arguments(argument);
    }
    receiver->set_record_name(record->getQualifiedNameAsString());
    receiver->set_clang_version(CLANG_VERSION_STRING);
    receiver->set_target_triple(target_triple_);
    const auto& layout = context_->getASTRecordLayout(record);
    receiver->set_size(static_cast<std::uint64_t>(layout.getSize().getQuantity()));
    receiver->set_alignment(static_cast<std::uint64_t>(layout.getAlignment().getQuantity()));
    receiver->set_is_volatile(method->isVolatile());
    unsigned index = 0;
    for (const auto* field : fields) {
      auto* resolved_field = receiver->add_fields();
      resolved_field->set_name(field->getNameAsString());
      resolved_field->set_canonical_type(field->getType().getCanonicalType().getAsString(policy));
      resolved_field->set_bit_offset(layout.getFieldOffset(index++));
      resolved_field->set_bit_width(field->isBitField() ? field->getBitWidthValue() : 0);
      resolved_field->set_bit_size(context_->getTypeSize(field->getType()));
      resolved_field->set_bit_alignment(context_->getTypeAlign(field->getType()));
      resolved_field->set_is_mutable(field->isMutable());
      resolved_field->set_abi_identity(
          tracer::receiver_abi::type_identity(field->getType(), *context_));
      switch (field->getAccess()) {
      case clang::AS_public:
        resolved_field->set_access("public");
        break;
      case clang::AS_protected:
        resolved_field->set_access("protected");
        break;
      case clang::AS_private:
        resolved_field->set_access("private");
        break;
      case clang::AS_none:
        resolved_field->set_access(record->isClass() ? "private" : "public");
        break;
      }
    }
    if (receiver->ByteSizeLong() > kMaxReceiverArgumentBytes) {
      candidate.spec.clear_receiver();
      candidate.spec.set_receiver_unavailable_reason("receiver metadata exceeds 64 KiB");
    }
  }

  clang::ASTContext* context_ = nullptr;
  std::vector<Candidate>& candidates_;
  const std::string& qualified_name_;
  FrozenInvocation invocation_;
  std::string translation_unit_;
  std::string target_triple_;
  clang::Sema *sema_;
};

class Action : public clang::ASTFrontendAction {
public:
  explicit Action(std::vector<Candidate>& candidates, std::string& target_error,
                  const std::string& qualified_name, FrozenInvocation& frozen_invocation,
                  std::string& invocation_error, bool prepared = false)
      : candidates_(candidates)
      , target_error_(target_error)
      , qualified_name_(qualified_name)
      , frozen_invocation_(frozen_invocation)
      , invocation_error_(invocation_error), prepared_(prepared) {}

  bool BeginSourceFileAction(clang::CompilerInstance& compiler) override {
    const auto& target = compiler.getTarget();
    const auto& triple = target.getTriple();
    if (triple.getArch() != llvm::Triple::x86_64 || !triple.isOSLinux() ||
        target.getPointerWidth(clang::LangAS::Default) != 64 || target.getLongWidth() != 64 ||
        (prepared_ && (!triple.isOSBinFormatELF() || !target.isLittleEndian() ||
                       !target.getCXXABI().isItaniumFamily()))) {
      target_error_ = prepared_
          ? "compilation database ABI does not match the trace target (expected Linux x86_64 "
            "ELF LP64 with the Itanium C++ ABI, found " + triple.str() + ")"
          : "compilation database must target Linux x86_64 (found " + triple.str() + ")";
      return false;
    }
    return true;
  }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler,
                                                        llvm::StringRef) override;

private:
  std::vector<Candidate>& candidates_;
  std::string& target_error_;
  const std::string& qualified_name_;
  FrozenInvocation& frozen_invocation_;
  std::string& invocation_error_;
  bool prepared_;
};

class Consumer : public clang::ASTConsumer {
public:
  Consumer(std::vector<Candidate>& candidates, const std::string& qualified_name,
           FrozenInvocation invocation, std::string translation_unit, std::string target_triple,
            clang::CompilerInstance *compiler)
      : visitor_(candidates, qualified_name, std::move(invocation), std::move(translation_unit),
                 std::move(target_triple), nullptr), compiler_(compiler) {}

  void HandleTranslationUnit(clang::ASTContext& context) override {
    visitor_.set_context(context, compiler_ ? &compiler_->getSema() : nullptr);
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  Collector visitor_;
  clang::CompilerInstance *compiler_;
};

std::unique_ptr<clang::ASTConsumer> Action::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                              llvm::StringRef) {
  auto invocation = frozen_invocation_;
  if (compiler.getFrontendOpts().Inputs.empty()) {
    invocation_error_ = "Clang action has no main translation unit input";
    return std::make_unique<Consumer>(candidates_, qualified_name_, std::move(invocation), "",
                                      compiler.getTarget().getTriple().str(), prepared_ ? &compiler : nullptr);
  }
  const auto input = compiler.getFrontendOpts().Inputs.front().getFile().str();
  auto& vfs = compiler.getFileManager().getVirtualFileSystem();
  const auto vfs_working_directory = vfs.getCurrentWorkingDirectory();
  if (!vfs_working_directory) {
    invocation_error_ =
        "cannot read Clang VFS working directory: " + vfs_working_directory.getError().message();
    return std::make_unique<Consumer>(candidates_, qualified_name_, std::move(invocation), "",
                                      compiler.getTarget().getTriple().str(), prepared_ ? &compiler : nullptr);
  }
  auto absolute_vfs_path = [&vfs](llvm::StringRef path) {
    llvm::SmallString<256> normalized(path);
    if (vfs.makeAbsolute(normalized)) {
      return std::string();
    }
    llvm::SmallString<256> real_path;
    if (!vfs.getRealPath(normalized, real_path)) {
      normalized = real_path;
    }
    llvm::sys::path::remove_dots(normalized, true);
    return std::string(normalized.str());
  };
  invocation.working_directory = absolute_vfs_path(*vfs_working_directory);
  const auto absolute_input = absolute_vfs_path(input);
  const auto absolute_adjuster_input = absolute_vfs_path(invocation.source_argument);
  if (invocation.arguments.empty() || absolute_input.empty() ||
      absolute_adjuster_input != absolute_input || invocation.working_directory.empty()) {
    invocation_error_ = "Clang action did not match a complete frozen source invocation";
  }
  invocation.source_argument = absolute_input;
  std::vector<std::string> normalized_arguments;
  normalized_arguments.reserve(invocation.arguments.size() + 1);
  if (!invocation.arguments.empty()) {
    normalized_arguments.push_back(invocation.arguments.front());
  }
  for (std::size_t i = 1; i < invocation.arguments.size(); ++i) {
    const auto& argument = invocation.arguments[i];
    const auto normalized_argument = absolute_vfs_path(argument);
    if (!normalized_argument.empty() && normalized_argument == absolute_input) {
      continue;
    }
    normalized_arguments.push_back(argument);
  }
  invocation.arguments = std::move(normalized_arguments);
  return std::make_unique<Consumer>(candidates_, qualified_name_, std::move(invocation),
                                    absolute_input, compiler.getTarget().getTriple().str(), prepared_ ? &compiler : nullptr);
}

class ActionFactory : public clang::tooling::FrontendActionFactory {
public:
  ActionFactory(std::vector<Candidate>& candidates, std::string& target_error,
                const std::string& qualified_name, FrozenInvocation& frozen_invocation,
                std::string& invocation_error, bool prepared = false)
      : candidates_(candidates)
      , target_error_(target_error)
      , qualified_name_(qualified_name)
      , frozen_invocation_(frozen_invocation)
      , invocation_error_(invocation_error), prepared_(prepared) {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<Action>(candidates_, target_error_, qualified_name_, frozen_invocation_,
                                    invocation_error_, prepared_);
  }

private:
  std::vector<Candidate>& candidates_;
  std::string& target_error_;
  const std::string& qualified_name_;
  FrozenInvocation& frozen_invocation_;
  std::string& invocation_error_;
  bool prepared_;
};

std::unique_ptr<clang::tooling::CompilationDatabase> load_database(const std::string& requested) {
  std::string error;
  if (!requested.empty()) {
    fs::path path(requested);
    if (fs::is_regular_file(path)) {
      auto db = clang::tooling::JSONCompilationDatabase::loadFromFile(
          path.string(), error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
      if (!db) {
        throw std::runtime_error("cannot load compilation database: " + error);
      }
      return db;
    }
    if (fs::is_directory(path)) {
      auto db = clang::tooling::CompilationDatabase::loadFromDirectory(path.string(), error);
      if (!db) {
        throw std::runtime_error("cannot load compilation database: " + error);
      }
      return db;
    }
    throw std::runtime_error("compilation database path does not exist: " + requested);
  }
  fs::path cwd = fs::current_path();
  const std::vector<fs::path> bases{cwd, cwd / "build", cwd.parent_path(),
                                    cwd.parent_path() / "build"};
  for (const auto& base : bases) {
    error.clear();
    auto db = clang::tooling::CompilationDatabase::loadFromDirectory(base.string(), error);
    if (db) {
      return db;
    }
  }
  throw std::runtime_error("no compile_commands.json found in the current "
                           "directory, build/, parent, or parent/build/");
}

std::string normalized_path(const std::string& path, const std::string& directory = {}) {
  fs::path p(path);
  if (p.is_relative() && !directory.empty()) {
    p = fs::path(directory) / p;
  }
  std::error_code error;
  auto canonical = fs::weakly_canonical(p, error);
  return (error ? p.lexically_normal() : canonical).string();
}

bool has_resource_dir_argument(const clang::tooling::CommandLineArguments& arguments) {
  return std::any_of(arguments.begin(), arguments.end(), [](const auto& argument) {
    return argument == "-resource-dir" || ::tracer::compat::starts_with(argument, "-resource-dir=");
  });
}

bool matches(const Candidate& candidate, const Selector& selector) {
  if (candidate.name != selector.qualified_name) {
    return false;
  }
  if (!selector.has_parameters || candidate.selected_by_type) {
    return true;
  }
  if (candidate.parameter_spellings.size() != selector.parameters.size() ||
      candidate.is_const != selector.is_const || candidate.is_volatile != selector.is_volatile ||
      candidate.ref_qualifier != selector.ref_qualifier) {
    return false;
  }
  for (std::size_t i = 0; i < selector.parameters.size(); ++i) {
    const auto requested = compact(selector.parameters[i]);
    if (compact(candidate.parameter_spellings[i]) != requested &&
        compact(candidate.canonical_parameter_spellings[i]) != requested) {
      return false;
    }
  }
  return true;
}

} // namespace

static ResolvedCallable select_cpp_candidate(std::vector<Candidate> candidates,
                                                const Selector& selector,
                                                const std::string& name) {
  std::unordered_map<std::string, Candidate> unique;
  for (auto& candidate : candidates) {
    if (!matches(candidate, selector)) {
      continue;
    }
    const auto existing = unique.find(candidate.symbol);
    if (existing == unique.end()) {
      unique.try_emplace(candidate.symbol, std::move(candidate));
    } else if (existing->second.canonical_identity != candidate.canonical_identity ||
               existing->second.unsupported_reason != candidate.unsupported_reason) {
      throw std::runtime_error("conflicting declarations resolve to the same symbol '" +
                               existing->second.spec.symbol() + "'");
    }
  }
  if (unique.empty()) {
    throw std::runtime_error("no callable matched '" + name + "'");
  }
  if (unique.size() != 1) {
    std::string message = "callable selector is ambiguous; candidates:";
    for (const auto& [_, candidate] : unique) {
      message += "\n  " + candidate.declaration;
    }
    throw std::runtime_error(message);
  }
  auto candidate = std::move(unique.begin()->second);
  if (!candidate.unsupported_reason.empty()) {
    throw std::runtime_error("callable is unsupported: " + candidate.unsupported_reason);
  }
  ResolvedCallable resolved;
  resolved.spec = std::move(candidate.spec);
  resolved.declaration = std::move(candidate.declaration);
  resolved.source = std::move(candidate.source);
  resolved.requires_typed_adapter = candidate.requires_typed_adapter;
  return resolved;
}

ResolvedCallable resolve_cpp_callable(const std::string& name,
                                      const std::string& compilation_database,
                                      const std::vector<std::string>& sources) {
  const auto selector = parse_selector(name);
  if (sources.size() > kMaxExplicitSources) {
    throw std::runtime_error("too many --source entries (maximum 128)");
  }
  auto database = load_database(compilation_database);
  std::vector<std::string> source_files;
  if (sources.empty()) {
    source_files = database->getAllFiles();
  } else {
    std::set<std::string> requested;
    for (const auto& source : sources) {
      requested.insert(normalized_path(source));
    }
    for (const auto& file : database->getAllFiles()) {
      const auto commands = database->getCompileCommands(file);
      bool found = false;
      for (const auto& command : commands) {
        if (::tracer::compat::contains(requested, normalized_path(file, command.Directory))) {
          found = true;
          break;
        }
      }
      if (found) {
        source_files.push_back(file);
      }
    }
    if (source_files.size() != requested.size()) {
      throw std::runtime_error("one or more --source files are not present in "
                               "the compilation database");
    }
  }
  std::sort(source_files.begin(), source_files.end());
  source_files.erase(std::unique(source_files.begin(), source_files.end()), source_files.end());
  if (source_files.empty()) {
    throw std::runtime_error("compilation database contains no source files");
  }
  if (source_files.size() > kMaxTranslationUnits) {
    throw std::runtime_error("compilation database has more than 256 source "
                             "files; restrict with --source");
  }

  const fs::path resource_dir(TRACER_CLANG_RESOURCE_DIR);
  bool needs_default_resource_dir = false;
  for (const auto& source : source_files) {
    const auto commands = database->getCompileCommands(source);
    if (commands.empty()) {
      throw std::runtime_error("compilation database has no command for source: " + source);
    }
    for (const auto& command : commands) {
      needs_default_resource_dir |= !has_resource_dir_argument(command.CommandLine);
    }
  }
  if (needs_default_resource_dir && !fs::is_directory(resource_dir)) {
    throw std::runtime_error("configured Clang resource directory is missing: " +
                             resource_dir.string());
  }

  std::vector<Candidate> candidates;
  std::string target_error;
  FrozenInvocation frozen_invocation;
  std::string invocation_error;
  clang::tooling::ClangTool tool(*database, source_files);
  tool.appendArgumentsAdjuster(clang::tooling::getClangStripOutputAdjuster());
  tool.appendArgumentsAdjuster(clang::tooling::getClangSyntaxOnlyAdjuster());
  tool.appendArgumentsAdjuster(
      [resource_dir = resource_dir.string()](const clang::tooling::CommandLineArguments& arguments,
                                             llvm::StringRef) {
        auto adjusted = arguments;
        if (!has_resource_dir_argument(adjusted)) {
          adjusted.push_back("-resource-dir=" + resource_dir);
        }
        return adjusted;
      });
  tool.appendArgumentsAdjuster(
      [&frozen_invocation](const clang::tooling::CommandLineArguments& arguments,
                           llvm::StringRef filename) {
        frozen_invocation.arguments = arguments;
        frozen_invocation.source_argument = filename.str();
        return arguments;
      });
  ActionFactory factory(candidates, target_error, selector.qualified_name, frozen_invocation,
                        invocation_error);
  const int result = tool.run(&factory);
  if (!target_error.empty()) {
    throw std::runtime_error(target_error);
  }
  if (!invocation_error.empty()) {
    throw std::runtime_error(invocation_error);
  }
  if (result != 0) {
    throw std::runtime_error("Clang could not parse every selected translation unit; fix "
                             "diagnostics or restrict --source");
  }

  return select_cpp_candidate(std::move(candidates), selector, name);
}

ResolvedCallable resolve_header_callable(const std::string& name,
                                         const api::ProbeSpec& headers,
                                         const PreparedHeaderCommand& command,
                                         bool pointer_layouts) {
  const auto selector = parse_selector(name);
  if (!selector.has_parameters || headers.include_headers().empty())
    throw std::runtime_error("prepared generation requires an explicit selector and header context");
  // Use the SDK fallback unless the database names a different installed
  // compiler by path or explicitly selects a toolchain.
  std::vector<std::string> arguments{"--no-default-config"};
  std::error_code path_error;
  const bool alternate_driver = fs::path(command.driver).is_absolute() &&
      fs::is_regular_file(command.driver, path_error) &&
      !fs::equivalent(command.driver, TRACER_PREPARE_BUILD_CXX, path_error) && !path_error;
  if (command.use_default_toolchain && !alternate_driver) {
    for (const auto* flag : runtime::jit_flags)
      arguments.push_back(command.clang_cl ? std::string("/clang:") + flag : flag);
  }
  arguments.insert(arguments.end(), command.arguments.begin(), command.arguments.end());
  arguments.push_back("-resource-dir=" + std::string(TRACER_CLANG_RESOURCE_DIR));
  arguments.insert(arguments.end(), {"-fsyntax-only", "-x", "c++"});
  const auto source = (fs::path(command.directory) / "clang-tracer-prepared-headers.cc").string();
  std::string code;
  for (const auto& header : headers.include_headers())
    code += "#include \"" + header + "\"\n";
  const auto angle = selector.qualified_name.find('<');
  const bool select_template = angle != std::string::npos &&
      selector.qualified_name.find('>', angle) != std::string::npos;
  if (select_template) {
    if (name.find_first_of("#;{}\\\r\n") != std::string::npos)
      throw std::invalid_argument("invalid character in prepared template selector");
    std::string parameters;
    for (const auto &parameter : selector.parameters) {
      if (!parameters.empty())
        parameters += ", ";
      parameters += parameter;
    }
    const auto type = "(" + parameters + ")";
    std::string member = type;
    if (selector.is_const) member += " const";
    if (selector.is_volatile) member += " volatile";
    if (selector.ref_qualifier == clang::RQ_LValue) member += " &";
    if (selector.ref_qualifier == clang::RQ_RValue) member += " &&";
    const auto qualified = selector.qualified_name.rfind("::", 0) == 0
        ? selector.qualified_name : "::" + selector.qualified_name;
    code += "\nnamespace tracer_prepared_selection {\n";
    if (member == type)
      code += "template<class R> void select(R (*)" + type + ");\n";
    code += "template<class R, class C> void select(R (C::*)" + member + ");\n"
            "using Selected = decltype(select(&" + qualified + "));\n}\n";
    arguments.push_back(command.clang_cl ? "/clang:-fno-access-control" : "-fno-access-control");
  }
  clang::tooling::FixedCompilationDatabase database(command.directory, arguments);
  clang::DiagnosticOptions diagnostic_options;
  PrepareDiagnostics diagnostics(diagnostic_options);
  clang::tooling::ClangTool tool(database, {source});
  tool.clearArgumentsAdjusters();
  tool.setDiagnosticConsumer(&diagnostics);
  tool.mapVirtualFile(source, code);
  std::vector<Candidate> candidates;
  std::string target_error, invocation_error;
  FrozenInvocation invocation;
  invocation.select_template = select_template;
  invocation.pointer_layouts = pointer_layouts;
  invocation.prepared_headers.assign(headers.include_headers().begin(),
                                     headers.include_headers().end());
  tool.appendArgumentsAdjuster([&](const clang::tooling::CommandLineArguments& args,
                                   llvm::StringRef filename) {
    invocation.arguments = args;
    invocation.arguments.front() = command.driver;
    invocation.source_argument = filename.str();
    return invocation.arguments;
  });
  ActionFactory factory(candidates, target_error, selector.qualified_name, invocation,
                        invocation_error, true);
  const auto result = tool.run(&factory);
  if (diagnostics.had_error())
    throw std::invalid_argument("Clang reported an error while parsing prepared headers");
  if (!target_error.empty() || !invocation_error.empty() || result != 0)
    throw std::runtime_error("cannot parse prepared headers: " + target_error + invocation_error);
  return select_cpp_candidate(std::move(candidates), selector, name);
}

void verify_prepared_adapter(const std::string &name, const api::PreparedInjector &adapter,
                             const ResolvedCallable &original, bool pointer_layouts) {
  const auto selector = parse_selector(name);
  std::vector<std::string> arguments(runtime::jit_flags.begin(), runtime::jit_flags.end());
  arguments.insert(arguments.end(), {"-std=c++20", "-x", "c++",
      "-resource-dir=" + std::string(TRACER_CLANG_RESOURCE_DIR)});
  arguments.insert(arguments.end(), adapter.adapter_abi_options().begin(),
                   adapter.adapter_abi_options().end());
  clang::DiagnosticOptions diagnostic_options;
  PrepareDiagnostics diagnostics(diagnostic_options);
  auto unit = clang::tooling::buildASTFromCodeWithArgs(
      tracer::prepared::adapter_source(adapter), arguments, "prepared-adapter.cc",
      "clang-tool", std::make_shared<clang::PCHContainerOperations>(),
      clang::tooling::getClangStripDependencyFileAdjuster(), {}, &diagnostics);
  // A compiler error must bypass the address-only pointer retry.
  if (diagnostics.had_error() || (unit && unit->getDiagnostics().hasErrorOccurred()))
    throw std::invalid_argument("Clang reported an error while compiling the prepared adapter");
  if (!unit)
    throw std::runtime_error("generated prepared adapter does not compile independently");
  std::vector<Candidate> candidates;
  FrozenInvocation invocation;
  invocation.adapter_symbol = original.spec.symbol();
  invocation.pointer_layouts = pointer_layouts;
  Collector visitor(candidates, selector.qualified_name, std::move(invocation), "", "",
                    &unit->getSema());
  visitor.set_context(unit->getASTContext(), &unit->getSema());
  visitor.TraverseDecl(unit->getASTContext().getTranslationUnitDecl());
  if (diagnostics.had_error())
    throw std::invalid_argument("Clang reported an error while compiling the prepared adapter");
  // Match by linkage because the adapter writes canonical parameter types.
  const auto candidate = std::find_if(candidates.begin(), candidates.end(), [&](const auto &item) {
    return item.symbol == original.spec.symbol() && item.unsupported_reason.empty();
  });
  if (candidate == candidates.end() ||
      candidate->spec.signature().SerializeAsString() != original.spec.signature().SerializeAsString() ||
      candidate->spec.debug_info().SerializeAsString() != original.spec.debug_info().SerializeAsString())
    throw std::runtime_error("generated prepared adapter does not reproduce the target ABI and layout");
}

} // namespace tracer::client

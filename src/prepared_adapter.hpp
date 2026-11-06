#pragma once

#include "prepared_types.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/Basic/SourceManager.h>

#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tracer::prepared {

struct AdapterDeclarations {
  std::string source;
  std::set<std::string> standard_headers;
};

class AdapterEmitter {
public:
  explicit AdapterEmitter(clang::ASTContext &ast) : ast_(ast), policy_(ast.getLangOpts()) {
    policy_.SuppressTagKeyword = true;
    policy_.FullyQualifiedName = true;
  }

  AdapterDeclarations emit(const clang::FunctionDecl &function,
                           const std::vector<std::string> &headers) {
    selected_method_ = llvm::dyn_cast<clang::CXXMethodDecl>(&function);
    // Additional inputs can provide callback-only payload declarations.
    extra_headers_.insert(std::next(headers.begin()), headers.end());
    emit_extra(*ast_.getTranslationUnitDecl());
    if (selected_method_) {
      emit_record(*selected_method_->getParent());
    } else {
      visit_signature(function);
      declarations_ += in_namespace(*function.getDeclContext(), callable(function));
    }
    return {forwards_ + declarations_, std::move(standard_headers_)};
  }

private:
  [[noreturn]] static void unsupported(const clang::NamedDecl &decl, const char *reason) {
    throw std::runtime_error("cannot prepare standalone type '" +
                             decl.getQualifiedNameAsString() + "': " + reason);
  }

  std::string in_namespace(const clang::DeclContext &context, std::string source) {
    const auto *scope = &context;
    while (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(scope)) {
      if (ns->isAnonymousNamespace())
        unsupported(*ns, "anonymous namespace");
      source = std::string(ns->isInline() ? "inline namespace " : "namespace ") +
               ns->getNameAsString() + " {\n" + source + "}\n";
      scope = ns->getParent();
    }
    if (const auto *linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(scope))
      return in_namespace(*linkage->getParent(), std::move(source));
    if (!scope->isTranslationUnit())
      throw std::runtime_error("standalone preparation requires namespace-scope types");
    return source;
  }

  std::string spelling(clang::QualType type, llvm::StringRef name = {}) {
    std::string result;
    llvm::raw_string_ostream out(result);
    type.getCanonicalType().print(out, policy_, name);
    return result;
  }

  static std::string access(clang::AccessSpecifier access) {
    return clang::getAccessSpelling(access).str();
  }

  void visit_signature(const clang::FunctionDecl &decl) {
    visit(decl.getReturnType());
    for (const auto *parameter : decl.parameters())
      visit(parameter->getType());
  }

  std::string callable(const clang::FunctionDecl &decl) {
    std::string source;
    if (decl.isExternC())
      source += "extern \"C\" ";
    if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&decl);
        method && method->isStatic())
      source += "static ";
    std::string name = decl.getNameAsString() + "(";
    for (unsigned i = 0; i < decl.getNumParams(); ++i) {
      if (i)
        name += ", ";
      name += spelling(decl.getParamDecl(i)->getType());
    }
    name += ")";
    if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&decl)) {
      if (method->isConst()) name += " const";
      if (method->isVolatile()) name += " volatile";
      if (method->getRefQualifier() == clang::RQ_LValue) name += " &";
      if (method->getRefQualifier() == clang::RQ_RValue) name += " &&";
    }
    if (decl.getType()->castAs<clang::FunctionProtoType>()->isNothrow())
      name += " noexcept";
    return source + spelling(decl.getReturnType(), name) + ";\n";
  }

  void emit_extra(const clang::DeclContext &scope) {
    if (extra_headers_.empty())
      return;
    const auto &sm = ast_.getSourceManager();
    for (const auto *decl : scope.decls()) {
      if (const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(decl);
          ns && !ns->isAnonymousNamespace()) {
        if (!sm.isInSystemHeader(ns->getLocation()))
          emit_extra(*ns);
        continue;
      }
      if (!extra_headers_.count(sm.getFilename(sm.getExpansionLoc(decl->getLocation())).str()))
        continue;
      if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
          record && record->isCompleteDefinition() && !record->isImplicit() &&
          record->getIdentifier() && !record->getDescribedClassTemplate())
        emit_record(*record);
      if (const auto *enumeration = llvm::dyn_cast<clang::EnumDecl>(decl);
          enumeration && enumeration->isCompleteDefinition() && enumeration->getIdentifier())
        emit_enum(*enumeration);
      if (const auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(decl))
        emit_alias(*alias);
    }
  }

  void emit_alias(const clang::TypedefNameDecl &alias) {
    if (alias.isInStdNamespace() || ast_.getSourceManager().isInSystemHeader(alias.getLocation()) ||
        !emitted_.insert(alias.getCanonicalDecl()).second)
      return;
    visit(alias.getUnderlyingType());
    declarations_ += in_namespace(*alias.getDeclContext(), "using " + alias.getNameAsString() +
                                  " = " + spelling(alias.getUnderlyingType()) + ";\n");
  }

  void emit_enum(const clang::EnumDecl &enumeration) {
    const auto *decl = enumeration.getDefinition();
    if (!decl || !decl->getIdentifier())
      unsupported(enumeration, "enumeration requires a named definition");
    if (!emitted_.insert(decl->getCanonicalDecl()).second)
      return;
    std::string source = decl->isScoped() ? "enum class " : "enum ";
    source += decl->getNameAsString() + " : " + spelling(decl->getIntegerType()) + " {\n";
    for (const auto *value : decl->enumerators()) {
      llvm::SmallString<32> number;
      value->getInitVal().toString(number, 10);
      source += value->getNameAsString() + " = " + number.str().str() + ",\n";
    }
    declarations_ += in_namespace(*decl->getDeclContext(), source + "};\n");
  }

  void standard_record(const clang::CXXRecordDecl &record) {
    const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&record);
    const auto name = special ? special->getSpecializedTemplate()->getName() : record.getName();
    static constexpr std::pair<std::string_view, std::string_view> headers[]{
        {"array", "array"}, {"atomic", "atomic"}, {"basic_string", "string"},
        {"basic_string_view", "string_view"}, {"bitset", "bitset"}, {"char_traits", "string"},
        {"complex", "complex"}, {"deque", "deque"}, {"duration", "chrono"},
        {"time_point", "chrono"}, {"forward_list", "forward_list"}, {"function", "functional"},
        {"greater", "functional"}, {"less", "functional"}, {"equal_to", "functional"},
        {"hash", "functional"}, {"initializer_list", "initializer_list"}, {"list", "list"},
        {"map", "map"}, {"multimap", "map"}, {"allocator", "memory"},
        {"default_delete", "memory"}, {"shared_ptr", "memory"}, {"weak_ptr", "memory"},
        {"unique_ptr", "memory"}, {"optional", "optional"}, {"queue", "queue"},
        {"priority_queue", "queue"}, {"set", "set"}, {"multiset", "set"}, {"stack", "stack"},
        {"tuple", "tuple"}, {"unordered_map", "unordered_map"},
        {"unordered_multimap", "unordered_map"}, {"unordered_set", "unordered_set"},
        {"unordered_multiset", "unordered_set"}, {"pair", "utility"}, {"ratio", "chrono"},
        {"monostate", "variant"}, {"variant", "variant"}, {"vector", "vector"}};
    for (const auto &[type, header] : headers) {
      if (name == llvm::StringRef(type.data(), type.size())) {
        standard_headers_.emplace(header);
        if (special)
          for (const auto &argument : special->getTemplateArgs().asArray())
            visit_template_arg(argument);
        return;
      }
    }
    unsupported(record, "unsupported standard library type");
  }

  void visit_template_arg(const clang::TemplateArgument &arg) {
    if (arg.getKind() == clang::TemplateArgument::Type)
      visit(arg.getAsType());
    if (arg.getKind() == clang::TemplateArgument::Pack)
      for (const auto &element : arg.pack_elements())
        visit_template_arg(element);
  }

  void visit(clang::QualType type, bool complete = true) {
    if (const auto *alias = type->getAs<clang::TypedefType>())
      emit_alias(*alias->getDecl());
    type = type.getCanonicalType();
    if (const auto *array = ast_.getAsArrayType(type)) {
      visit(array->getElementType(), complete);
    } else if (type->isPointerType()) {
      visit(type->getPointeeType(), false);
    } else if (type->isReferenceType()) {
      visit(type.getNonReferenceType(), complete);
    } else if (const auto *function = type->getAs<clang::FunctionProtoType>()) {
      visit(function->getReturnType(), false);
      for (auto parameter : function->param_types())
        visit(parameter, false);
    } else if (const auto *record = type->getAsCXXRecordDecl()) {
      if (record->isInStdNamespace())
        standard_record(*record);
      else if (complete && record->getDefinition())
        emit_record(*record->getDefinition());
      else
        forward(*record);
    } else if (const auto *enumeration = type->getAs<clang::EnumType>()) {
      emit_enum(*enumeration->getDecl());
    }
  }

  void forward(const clang::CXXRecordDecl &record) {
    if (!record.getIdentifier() || record.isDependentType() ||
        llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
      unsupported(record, "requires a named non-template declaration");
    if (forwarded_.insert(record.getCanonicalDecl()).second)
      forwards_ += in_namespace(*record.getDeclContext(),
          std::string(record.getKindName()) + " " + record.getNameAsString() + ";\n");
  }

  void emit_record(const clang::CXXRecordDecl &record) {
    if (record.isInStdNamespace()) {
      standard_record(record);
      return;
    }
    forward(record);
    if (!emitted_.insert(record.getCanonicalDecl()).second)
      return;
    if (record.isDynamicClass() || record.isUnion())
      unsupported(record, "polymorphic, virtual-base and union layouts require an explicit adapter");
    for (const auto *method : record.methods()) {
      const auto *ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
      if (method->isUserProvided() &&
          (llvm::isa<clang::CXXDestructorDecl>(method) || (ctor && ctor->isCopyOrMoveConstructor())))
        unsupported(record, "user-defined copy, move or destruction requires an explicit adapter");
    }
    const auto alignment = ast_.getASTRecordLayout(&record).getAlignment().getQuantity();
    std::string source = std::string(record.getKindName()) + " alignas(" +
                         std::to_string(alignment) + ") ";
    if (record.hasAttr<clang::PackedAttr>())
      source += "__attribute__((packed)) ";
    source += record.getNameAsString();
    bool first = true;
    for (const auto &base : record.bases()) {
      visit(base.getType());
      source += first ? " : " : ", ";
      source += access(base.getAccessSpecifier()) + " " + spelling(base.getType());
      first = false;
    }
    source += " {\n";
    for (const auto *field : record.fields()) {
      visit(field->getType());
      source += access(field->getAccess()) + ": ";
      if (field->hasAttr<clang::NoUniqueAddressAttr>())
        source += "[[no_unique_address]] ";
      if (field->hasAttr<clang::PackedAttr>())
        source += "__attribute__((packed)) ";
      if (const auto alignment = field->getMaxAlignment())
        source += "alignas(" + std::to_string(alignment / ast_.getCharWidth()) + ") ";
      if (field->isMutable())
        source += "mutable ";
      source += spelling(field->getType(), field->getName());
      if (field->isBitField())
        source += " : " + std::to_string(field->getBitWidthValue());
      source += ";\n";
    }
    for (const auto *method : record.methods()) {
      if (method->isImplicit())
        continue;
      const auto *ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
      const bool destructor = llvm::isa<clang::CXXDestructorDecl>(method);
      if ((!ctor && !destructor) ||
          (!method->isExplicitlyDefaulted() && !method->isDeletedAsWritten()))
        continue;
      source += access(method->getAccess()) + ": ";
      if (destructor)
        source += "~";
      source += record.getNameAsString() + "(";
      for (unsigned i = 0; i < method->getNumParams(); ++i) {
        if (i)
          source += ", ";
        source += spelling(method->getParamDecl(i)->getType());
      }
      source += method->isDeletedAsWritten() ? ") = delete;\n" : ") = default;\n";
    }
    if (selected_method_ && selected_method_->getParent()->getCanonicalDecl() ==
                                record.getCanonicalDecl()) {
      visit_signature(*selected_method_);
      source += "public: " + callable(*selected_method_);
    }
    declarations_ += in_namespace(*record.getDeclContext(), source + "};\n");
  }

  clang::ASTContext &ast_;
  clang::PrintingPolicy policy_;
  const clang::CXXMethodDecl *selected_method_ = nullptr;
  std::set<std::string> extra_headers_;
  std::set<std::string> standard_headers_;
  std::unordered_set<const clang::Decl *> emitted_;
  std::unordered_set<const clang::Decl *> forwarded_;
  std::string forwards_;
  std::string declarations_;
};

} // namespace tracer::prepared

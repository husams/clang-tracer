#pragma once

#include "prepared_types.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringExtras.h>

#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
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
                           const std::vector<std::string> &headers,
                           bool pointer_layouts) {
    selected_method_ = llvm::dyn_cast<clang::CXXMethodDecl>(&function);
    const bool linkage_alias = nested_static_method(selected_method_);
    if (selected_method_ && !linkage_alias)
      register_nested(*selected_method_->getParent());
    register_nested_type(function.getReturnType(), pointer_layouts);
    for (const auto *parameter : function.parameters())
      register_nested_type(parameter->getType(), pointer_layouts);
    // Additional inputs can provide callback-only payload declarations.
    extra_headers_.insert(std::next(headers.begin()), headers.end());
    emit_extra(*ast_.getTranslationUnitDecl());
    if (selected_method_ && !linkage_alias) {
      emit_record(*selected_method_->getParent());
    } else {
      visit_signature(function);
      if (linkage_alias) {
        // A static method needs only its signature and original ELF symbol.
        declarations_ += "namespace tracer_prepared_detail {\n" +
                         callable(function) + "}\n";
      } else {
        declarations_ += in_namespace(function, callable(function));
      }
    }
    // Preserve the previous ordering for ordinary namespace-scope pointees.
    if (pointer_layouts) {
      if (const auto *record = prepared_pointer_record(function.getReturnType()))
        if (!llvm::isa<clang::CXXRecordDecl>(record->getDeclContext()))
          emit_record(*record);
      for (const auto *parameter : function.parameters())
        if (const auto *record = prepared_pointer_record(parameter->getType()))
          if (!llvm::isa<clang::CXXRecordDecl>(record->getDeclContext()))
            emit_record(*record);
    }
    return {forwards_ + declarations_, std::move(standard_headers_)};
  }

private:
  [[noreturn]] static void unsupported(const clang::NamedDecl &decl, const char *reason) {
    std::string name;
    llvm::raw_string_ostream out(name);
    auto policy = decl.getASTContext().getPrintingPolicy();
    policy.FullyQualifiedName = true;
    decl.getNameForDiagnostic(out, policy, true);
    throw std::runtime_error("cannot prepare standalone type '" + name + "': " + reason);
  }

  std::string in_namespace(const clang::NamedDecl &decl, std::string source) {
    for (const auto *scope = decl.getDeclContext(); !scope->isTranslationUnit();
         scope = scope->getParent()) {
      if (llvm::isa<clang::LinkageSpecDecl>(scope))
        continue;
      const auto *ns = llvm::dyn_cast<clang::NamespaceDecl>(scope);
      if (!ns || ns->isAnonymousNamespace())
        unsupported(decl, "cannot declare the type in its original scope");
      source = std::string(ns->isInline() ? "inline namespace " : "namespace ") +
               ns->getNameAsString() + " {\n" + source + "}\n";
    }
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

  static bool custom_lifetime(const clang::CXXMethodDecl &method) {
    if (!method.isUserProvided())
      return false;
    if (const auto *ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(&method))
      return ctor->isCopyOrMoveConstructor();
    return llvm::isa<clang::CXXDestructorDecl>(method) && !method.isVirtual();
  }

  void visit_signature(const clang::FunctionDecl &decl, bool complete = true) {
    visit(decl.getReturnType(), complete);
    for (const auto *parameter : decl.parameters())
      visit(parameter->getType(), complete);
  }

  // Reserve direct signature dependencies before additional headers or another
  // signature type can cause their owning record to be emitted.
  void register_nested(const clang::NamedDecl &decl) {
    const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(decl.getDeclContext());
    if (!owner)
      return;
    nested_declarations_[owner->getCanonicalDecl()].push_back(&decl);
    if (const auto *definition = owner->getDefinition())
      register_nested(*definition);
  }

  void register_nested_type(clang::QualType type, bool pointer_layouts) {
    if (const auto *alias = type->getAs<clang::TypedefType>())
      register_nested(*alias->getDecl());
    if (type->isPointerType()) {
      if (pointer_layouts)
        if (const auto *pointee = prepared_pointer_record(type))
          register_nested(*pointee);
      return;
    }
    type = type.getNonReferenceType().getCanonicalType();
    if (const auto *record = type->getAsCXXRecordDecl()) {
      if (const auto *definition = record->getDefinition())
        register_nested(*definition);
    } else if (const auto *enumeration = type->getAs<clang::EnumType>()) {
      register_nested(*enumeration->getDecl());
    }
  }

  std::string callable(const clang::FunctionDecl &decl, bool selected = true) {
    std::string source;
    if (decl.isExternC())
      source += "extern \"C\" ";
    const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&decl);
    const bool linkage_alias = selected && nested_static_method(method);
    const auto symbol = linkage_alias || (selected && template_instance(decl))
                            ? mangle(&decl, ast_) : std::string();
    if (method && method->isStatic() && !linkage_alias)
      source += "static ";
    if (method && method->isVirtual())
      source += "virtual ";
    const bool destructor = llvm::isa<clang::CXXDestructorDecl>(decl);
    const auto *conversion = llvm::dyn_cast<clang::CXXConversionDecl>(&decl);
    std::string name;
    if (linkage_alias)
      name = "target_" + llvm::toHex(symbol, true);
    else if (destructor)
      name = "~" + method->getParent()->getNameAsString();
    else if (conversion)
      name = "operator " + spelling(conversion->getConversionType());
    else
      name = decl.getNameAsString();
    name += "(";
    for (unsigned i = 0; i < decl.getNumParams(); ++i) {
      if (i)
        name += ", ";
      name += spelling(decl.getParamDecl(i)->getType());
    }
    name += ")";
    if (method) {
      if (method->isConst()) name += " const";
      if (method->isVolatile()) name += " volatile";
      if (method->getRefQualifier() == clang::RQ_LValue) name += " &";
      if (method->getRefQualifier() == clang::RQ_RValue) name += " &&";
    }
    if (decl.getType()->castAs<clang::FunctionProtoType>()->isNothrow())
      name += " noexcept";
    source += (destructor || conversion) ? name : spelling(decl.getReturnType(), name);
    if (!symbol.empty())
      source += " __asm__(\"" + symbol + "\")";
    if (decl.isPureVirtual())
      source += " = 0";
    else if (decl.isDeletedAsWritten())
      source += " = delete";
    return source + ";\n";
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
        emitted_.count(alias.getCanonicalDecl()) || defer_to_owner(alias))
      return;
    emitted_.insert(alias.getCanonicalDecl());
    visit(alias.getUnderlyingType());
    const auto declaration = "using " + alias.getNameAsString() + " = " +
                             spelling(alias.getUnderlyingType()) + ";\n";
    place_declaration(alias, declaration);
  }

  void emit_enum(const clang::EnumDecl &enumeration) {
    const auto *decl = enumeration.getDefinition();
    if (!decl || !decl->getIdentifier())
      unsupported(enumeration, "enumeration requires a named definition");
    if (emitted_.count(decl->getCanonicalDecl()) || defer_to_owner(*decl))
      return;
    emitted_.insert(decl->getCanonicalDecl());
    std::string source = decl->isScoped() ? "enum class " : "enum ";
    source += decl->getNameAsString() + " : " + spelling(decl->getIntegerType()) + " {\n";
    for (const auto *value : decl->enumerators()) {
      llvm::SmallString<32> number;
      value->getInitVal().toString(number, 10);
      source += value->getNameAsString() + " = " + number.str().str() + ",\n";
    }
    place_declaration(*decl, source + "};\n");
  }

  void place_declaration(const clang::NamedDecl &decl, std::string source) {
    if (const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(decl.getDeclContext()))
      nested_declarations_[owner->getCanonicalDecl()].push_back("public: " + std::move(source));
    else
      declarations_ += in_namespace(decl, std::move(source));
  }

  bool defer_to_owner(const clang::NamedDecl &decl) {
    const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(decl.getDeclContext());
    if (!owner || emitting_.count(owner->getCanonicalDecl()))
      return false;
    if (emitted_.count(owner->getCanonicalDecl()))
      unsupported(decl, "nested dependency was discovered after its owner was emitted");
    nested_declarations_[owner->getCanonicalDecl()].push_back(&decl);
    ensure_nested_owner(*owner, decl);
    return true;
  }

  void ensure_nested_owner(const clang::CXXRecordDecl &owner,
                           const clang::NamedDecl &dependency) {
    if (emitted_.count(owner.getCanonicalDecl()))
      return;
    if (const auto *definition = owner.getDefinition())
      emit_record(*definition);
    else
      unsupported(dependency, "nested type owner has no complete definition");
  }

  void standard_record(const clang::CXXRecordDecl &record) {
    const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&record);
    const auto name = special ? special->getSpecializedTemplate()->getName() : record.getName();
    static constexpr std::pair<std::string_view, std::string_view> headers[]{
        {"array", "array"}, {"atomic", "atomic"}, {"basic_ostream", "ostream"},
        {"basic_ostringstream", "sstream"}, {"basic_string", "string"},
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
    if (complete)
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
    if (!record.getIdentifier() || record.isDependentType())
      unsupported(record, "requires a named concrete declaration");
    // A nested name can only be declared inside its owner. Its complete
    // declaration is queued there by emit_record instead.
    if (const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(record.getDeclContext())) {
      if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
        unsupported(record, "nested class template specializations are unsupported");
      if (!forwarded_.insert(record.getCanonicalDecl()).second)
        return;
      const auto *owner_id = owner->getCanonicalDecl();
      if (emitted_.count(owner_id) && !emitting_.count(owner_id))
        unsupported(record, "nested dependency was discovered after its owner was emitted");
      nested_declarations_[owner_id].push_back("public: " +
          std::string(record.getKindName()) + " " + record.getNameAsString() + ";\n");
      ensure_nested_owner(*owner, record);
      return;
    }
    if (!forwarded_.insert(record.getCanonicalDecl()).second)
      return;
    if (const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&record)) {
      const auto *primary = special->getSpecializedTemplate();
      if (forwarded_.insert(primary->getCanonicalDecl()).second) {
        std::string declaration = "template<";
        bool first = true;
        for (const auto *parameter : *primary->getTemplateParameters()) {
          if (!first)
            declaration += ", ";
          first = false;
          if (const auto *type = llvm::dyn_cast<clang::TemplateTypeParmDecl>(parameter)) {
            declaration += type->isParameterPack() ? "class..." : "class";
          } else if (const auto *value = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(parameter);
                     value && !value->getType()->isDependentType()) {
            visit(value->getType());
            declaration += spelling(value->getType(), value->isParameterPack() ? "..." : "");
          } else {
            unsupported(record, "cannot extract the primary template parameters");
          }
        }
        forwards_ += in_namespace(record, declaration + "> " +
            std::string(record.getKindName()) + " " + record.getNameAsString() + ";\n");
      }
      for (const auto &argument : special->getTemplateArgs().asArray())
        visit_template_arg(argument);
    }
    forwards_ += in_namespace(record,
        specialization_prefix(record) + std::string(record.getKindName()) + " " +
        record_name(record) + ";\n");
  }

  static std::string specialization_prefix(const clang::CXXRecordDecl &record) {
    return llvm::isa<clang::ClassTemplateSpecializationDecl>(record) ? "template<> " : "";
  }

  std::string record_name(const clang::CXXRecordDecl &record) {
    if (!llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
      return record.getNameAsString();
    auto policy = policy_;
    policy.SuppressDefaultTemplateArgs = false;
    std::string result;
    llvm::raw_string_ostream out(result);
    record.getNameForDiagnostic(out, policy, false);
    return result;
  }

  void emit_record(const clang::CXXRecordDecl &record) {
    if (record.isInStdNamespace()) {
      standard_record(record);
      return;
    }
    const auto *identity = record.getCanonicalDecl();
    if (emitted_.count(identity) || defer_to_owner(record))
      return;
    forward(record);
    emitted_.insert(identity);
    emitting_.insert(identity);
    if (record.isUnion())
      unsupported(record, "union layout requires an explicit adapter");
    const auto alignment = ast_.getASTRecordLayout(&record).getAlignment().getQuantity();
    std::string source = specialization_prefix(record) + std::string(record.getKindName()) + " alignas(" +
                         std::to_string(alignment) + ") ";
    if (record.hasAttr<clang::PackedAttr>())
      source += "__attribute__((packed)) ";
    source += record_name(record);
    if (record.hasAttr<clang::FinalAttr>())
      source += " final";
    bool first = true;
    for (const auto &base : record.bases()) {
      visit(base.getType());
      source += first ? " : " : ", ";
      source += access(base.getAccessSpecifier()) + (base.isVirtual() ? " virtual " : " ") +
                spelling(base.getType());
      first = false;
    }
    source += " {\n";
    const auto body = source.size();
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
    bool emitted_selected = false;
    for (const auto *method : record.methods()) {
      if (method->isImplicit())
        continue;
      if (method->isVirtual()) {
        const bool selected = selected_method_ &&
            method->getCanonicalDecl() == selected_method_->getCanonicalDecl();
        visit_signature(*method, selected);
        source += access(method->getAccess()) + ": " + callable(*method, selected);
        emitted_selected |= selected;
        continue;
      }
      const auto *ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
      const bool destructor = llvm::isa<clang::CXXDestructorDecl>(method);
      const bool custom = custom_lifetime(*method);
      if ((!ctor && !destructor) ||
          (!custom && !method->isExplicitlyDefaulted() && !method->isDeletedAsWritten()))
        continue;
      source += access(method->getAccess()) + ": ";
      if (destructor)
        source += "~";
      source += record.getNameAsString() + "(";
      // A copy or move constructor can have trailing default arguments.
      // Only its first parameter is needed to retain its special-member role.
      const auto parameters = custom && ctor ? 1U : method->getNumParams();
      for (unsigned i = 0; i < parameters; ++i) {
        if (i)
          source += ", ";
        source += spelling(method->getParamDecl(i)->getType());
      }
      source += ")";
      if (custom) {
        // Keep the call ABI without compiling application lifetime code.
        const auto *prototype = method->getType()->castAs<clang::FunctionProtoType>();
        if (!clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()) &&
            prototype->isNothrow())
          source += " noexcept";
      } else {
        source += method->isDeletedAsWritten() ? " = delete" : " = default";
      }
      source += ";\n";
    }
    if (!emitted_selected && selected_method_ &&
        selected_method_->getParent()->getCanonicalDecl() == record.getCanonicalDecl() &&
        !nested_static_method(selected_method_)) {
      visit_signature(*selected_method_);
      source += "public: " + callable(*selected_method_);
    }
    source.insert(body, take_nested_declarations(identity));
    emitting_.erase(identity);
    place_declaration(record, source + "};\n");
  }

  std::string take_nested_declarations(const clang::CXXRecordDecl *owner) {
    std::string source;
    for (auto found = nested_declarations_.find(owner); found != nested_declarations_.end();
         found = nested_declarations_.find(owner)) {
      auto pending = std::move(found->second);
      nested_declarations_.erase(found);
      for (const auto &entry : pending) {
        if (const auto *declaration = std::get_if<const clang::NamedDecl *>(&entry)) {
          if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(*declaration))
            emit_record(*record);
          else if (const auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(*declaration))
            emit_alias(*alias);
          else
            emit_enum(*llvm::cast<clang::EnumDecl>(*declaration));
        } else {
          source += std::get<std::string>(entry);
        }
      }
    }
    return source;
  }

  clang::ASTContext &ast_;
  clang::PrintingPolicy policy_;
  const clang::CXXMethodDecl *selected_method_ = nullptr;
  std::set<std::string> extra_headers_;
  std::set<std::string> standard_headers_;
  std::unordered_set<const clang::Decl *> emitted_;
  std::unordered_set<const clang::Decl *> forwarded_;
  using NestedEntry = std::variant<std::string, const clang::NamedDecl *>;
  std::unordered_map<const clang::CXXRecordDecl *, std::vector<NestedEntry>> nested_declarations_;
  std::unordered_set<const clang::Decl *> emitting_;
  std::string forwards_;
  std::string declarations_;
};

} // namespace tracer::prepared

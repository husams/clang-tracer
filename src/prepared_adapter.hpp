#pragma once

#include "prepared_types.hpp"
#include "prepared_header_discovery.hpp"
#include "record_factory.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Sema/Sema.h>
#include <llvm/ADT/StringExtras.h>

#include <algorithm>
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

class AdapterEmitter : private clang::PrintingCallbacks {
public:
  static AdapterDeclarations emit(clang::ASTContext &ast, clang::Sema &sema,
                                  const clang::HeaderSearch &toolchain_search,
                                  const clang::FunctionDecl &function,
                                  const std::vector<std::string> &headers,
                                  bool pointer_layouts,
                                  const std::vector<const clang::CXXRecordDecl *> &snapshot_types = {}) {
    std::vector<NestedRequest> late_requests;
    for (;;) {
      const auto reserved = late_requests.size();
      AdapterEmitter pass(ast, sema, toolchain_search, late_requests);
      pass.emit_declarations(function, headers, pointer_layouts, snapshot_types);
      if (!pass.first_late_dependency_)
        return {pass.forwards_ + pass.declarations_, std::move(pass.standard_headers_)};
      // Each retry must discover another dependency. Reuse the original
      // traversal so new prerequisites are emitted before their owners.
      if (late_requests.size() == reserved)
        unsupported(*pass.first_late_dependency_,
                    "nested dependency was discovered after its owner was emitted");
    }
  }

private:
  struct NestedRequest {
    const clang::NamedDecl *decl;
    bool complete;
  };

  AdapterEmitter(clang::ASTContext &ast, clang::Sema &sema,
                 const clang::HeaderSearch &toolchain_search,
                 std::vector<NestedRequest> &late_requests)
      : ast_(ast), sema_(sema), policy_(ast.getLangOpts()),
        header_discovery_(ast, toolchain_search), late_requests_(late_requests) {
    policy_.SuppressTagKeyword = true;
    policy_.FullyQualifiedName = true;
    policy_.Callbacks = this;
  }

  // The typedef that names an unnamed record is declared after the record's members, so
  // a member cannot qualify a name with it. Inside the record its members are in scope.
  bool isScopeVisible(const clang::DeclContext *scope) const override {
    const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(scope);
    return record && !record->getIdentifier() && emitting_.count(record->getCanonicalDecl());
  }

  void emit_declarations(const clang::FunctionDecl &function,
                         const std::vector<std::string> &headers,
                         bool pointer_layouts,
                         const std::vector<const clang::CXXRecordDecl *> &snapshot_types) {
    selected_method_ = llvm::dyn_cast<clang::CXXMethodDecl>(&function);
    // A union is declared where it is a member or a referent. One that the callable takes or
    // returns by value, or whose method is selected, is part of the call itself.
    const auto reject_union = [](const clang::CXXRecordDecl *record) {
      if (record && record->isUnion())
        unsupported(*record, "union layout requires an explicit adapter");
    };
    reject_union(function.getReturnType().getCanonicalType()->getAsCXXRecordDecl());
    for (const auto *parameter : function.parameters())
      reject_union(parameter->getType().getCanonicalType()->getAsCXXRecordDecl());
    if (selected_method_)
      reject_union(selected_method_->getParent());
    const bool linkage_alias = nested_static_method(selected_method_);
    if (selected_method_ && !linkage_alias)
      register_nested(*selected_method_->getParent());
    for (const auto *snapshot_type : snapshot_types)
      if (snapshot_type)
        register_nested(*snapshot_type);
    register_nested_type(function.getReturnType(), pointer_layouts);
    for (const auto *parameter : function.parameters())
      register_nested_type(parameter->getType(), pointer_layouts);
    for (const auto &request : late_requests_)
      register_nested(*request.decl, request.complete);
    // Additional inputs can provide callback-only payload declarations.
    extra_headers_.insert(std::next(headers.begin()), headers.end());
    excluded_headers_.insert(headers.begin(), headers.end());
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
    for (std::size_t i = 0; i < snapshot_types.size(); ++i) {
      const auto *snapshot_type = snapshot_types[i];
      if (!snapshot_type)
        continue;
      emit_record(*snapshot_type);
      declarations_ += "namespace tracer_prepared { using SnapshotType" +
          std::to_string(i) + " = " + spelling(record_type(ast_, snapshot_type)) + "; }\n";
    }
  }

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

  // The type of a declarator after its array extents.
  clang::QualType array_element(clang::QualType type) {
    while (const auto *array = ast_.getAsArrayType(type))
      type = array->getElementType();
    return type;
  }

  std::string spelling(clang::QualType type, llvm::StringRef name = {}) {
    type = type.getCanonicalType();
    std::string declarator = name.str();
    // Clang prints the class of a canonical member pointer type without its scope, so a
    // declarator that reaches one is spelled here: `int ::shop::Order::*name`.
    while (array_element(type)->isMemberPointerType()) {
      if (const auto *array = ast_.getAsConstantArrayType(type)) {
        declarator += "[" + std::to_string(array->getSize().getZExtValue()) + "]";
        type = array->getElementType();
        continue;
      }
      const auto *member = type->getAs<clang::MemberPointerType>();
      if (!member)
        break;
      const auto qualifiers = type.getLocalQualifiers().getAsString();
      declarator = "::" + spelling(record_type(ast_, member->getMostRecentCXXRecordDecl())) +
                   "::*" + (qualifiers.empty() ? "" : " " + qualifiers) +
                   (declarator.empty() ? "" : " " + declarator);
      type = member->getPointeeType().getCanonicalType();
      if (type->isArrayType())
        declarator = "(" + declarator + ")";
    }
    std::string result;
    llvm::raw_string_ostream out(result);
    type.print(out, policy_, declarator);
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

  std::string emit_factory_constructor(const record_factory::Recipe &recipe) {
    const auto &ctor = *recipe.constructor;
    const auto argument_name = [](unsigned index) {
      return "__tracer_ctor_arg" + std::to_string(index);
    };
    std::string source;
    if (ctor.isExplicit())
      source += "explicit ";
    source += ctor.getParent()->getNameAsString() + "(";
    for (unsigned index = 0; index < ctor.getNumParams(); ++index) {
      if (index)
        source += ", ";
      const auto type = ctor.getParamDecl(index)->getType();
      visit(type);
      source += spelling(type, argument_name(index));
    }
    source += ")";
    const auto *prototype = ctor.getType()->castAs<clang::FunctionProtoType>();
    if (!clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()) &&
        prototype->isNothrow())
      source += " noexcept";
    bool first = true;
    for (const auto &initializer : recipe.initializers) {
      source += first ? " : " : ", ";
      first = false;
      source += initializer.field ? initializer.field->getNameAsString()
                                  : spelling(initializer.base);
      source += "(";
      for (std::size_t index = 0; index < initializer.arguments.size(); ++index) {
        if (index)
          source += ", ";
        const auto &argument = initializer.arguments[index];
        const auto name = argument_name(argument.index);
        if (argument.move) {
          standard_headers_.insert("utility");
          source += "::std::move(" + name + ")";
        } else {
          source += name;
        }
      }
      source += ")";
    }
    return source + " {}\n";
  }

  // Reserve direct signature dependencies before additional headers or another
  // signature type can cause their owning record to be emitted.
  void register_nested(const clang::NamedDecl &decl, bool complete = true) {
    const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(decl.getDeclContext());
    if (!owner)
      return;
    nested_declarations_[owner->getCanonicalDecl()].push_back(NestedRequest{&decl, complete});
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

  const char *exception_suffix(const clang::FunctionDecl &decl) {
    auto *prototype = decl.getType()->castAs<clang::FunctionProtoType>();
    if (clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()))
      prototype = sema_.ResolveExceptionSpec(decl.getLocation(), prototype);
    if (!prototype || clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()) ||
        prototype->getExceptionSpecType() == clang::EST_DependentNoexcept)
      unsupported(decl, "exception specification could not be resolved");
    return prototype->isNothrow() ? " noexcept" : "";
  }

  std::string callable(const clang::FunctionDecl &decl, bool selected = true) {
    std::string source;
    if (decl.isExternC())
      source += "extern \"C\" ";
    const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&decl);
    const bool linkage_alias = selected && nested_static_method(method);
    auto symbol = linkage_alias || (selected && template_instance(decl))
                      ? mangle(&decl, ast_) : std::string();
    // A function selected by its ELF symbol is declared with that symbol as its asm label.
    if (symbol.empty() && selected && !method)
      if (const auto *label = decl.getAttr<clang::AsmLabelAttr>())
        symbol = label->getLabel().str();
    if (method && method->isStatic() && !linkage_alias)
      source += "static ";
    if (method && method->isVirtual())
      source += "virtual ";
    const bool destructor = llvm::isa<clang::CXXDestructorDecl>(decl);
    const bool constructor = llvm::isa<clang::CXXConstructorDecl>(decl);
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
    name += exception_suffix(decl);
    // Constructors and destructors have no return type to spell.
    source += (destructor || constructor || conversion) ? name : spelling(decl.getReturnType(), name);
    if (!symbol.empty())
      source += " __asm__(\"" + symbol + "\")";
    if (decl.isPureVirtual())
      source += " = 0";
    else if (decl.isDeletedAsWritten())
      source += " = delete";
    else if (destructor && decl.isExplicitlyDefaulted() && decl.isInlined())
      source += " = default";
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
          record->getIdentifier() && !record->getDescribedClassTemplate() &&
          !is_snapshot_policy_specialization(record))
        emit_record(*record);
      if (const auto *enumeration = llvm::dyn_cast<clang::EnumDecl>(decl);
          enumeration && enumeration->isCompleteDefinition() && enumeration->getIdentifier())
        emit_enum(*enumeration);
      if (const auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(decl))
        emit_alias(*alias);
    }
  }

  static bool is_snapshot_policy_specialization(const clang::CXXRecordDecl *record) {
    const auto *specialization =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
    const auto *templ = specialization ? specialization->getSpecializedTemplate() : nullptr;
    if (!templ)
      return false;
    const auto name = templ->getQualifiedNameAsString();
    return name == "tracer_snapshot::FollowPointers" || name == "tracer_snapshot::AnyPayloads";
  }

  // The name a tag has for linkage: its own, or the typedef name an unnamed tag takes.
  // Empty when the tag has neither.
  static std::string linkage_name(const clang::TagDecl &tag) {
    if (tag.getIdentifier())
      return tag.getNameAsString();
    const auto *alias = tag.getTypedefNameForAnonDecl();
    return alias ? alias->getNameAsString() : std::string();
  }

  static bool names_tag_for_linkage(const clang::TypedefNameDecl &alias) {
    const auto *tag = alias.getUnderlyingType()->getAsTagDecl();
    const auto *name = tag && !tag->getIdentifier() ? tag->getTypedefNameForAnonDecl() : nullptr;
    return name && name->getCanonicalDecl() == alias.getCanonicalDecl();
  }

  // An unnamed record has no forward declaration: the typedef that names it for linkage
  // declares it, complete, in one declaration. Without that typedef nothing outside the
  // record's own declaration can name it.
  static const clang::CXXRecordDecl &unnamed_definition(const clang::CXXRecordDecl &record) {
    const auto *definition = record.getDefinition();
    if (!definition || !record.getTypedefNameForAnonDecl())
      unsupported(record, ("an unnamed " + std::string(record.getKindName()) +
                           " without a typedef name for linkage is unsupported").c_str());
    return *definition;
  }

  void emit_alias(const clang::TypedefNameDecl &alias) {
    if (alias.isInStdNamespace() || ast_.getSourceManager().isInSystemHeader(alias.getLocation()) ||
        emitted_.count(alias.getCanonicalDecl()) || defer_to_owner(alias))
      return;
    emitted_.insert(alias.getCanonicalDecl());
    visit(alias.getUnderlyingType());
    // The typedef that names an unnamed tag for linkage is emitted with the tag's definition.
    if (names_tag_for_linkage(alias))
      return;
    const auto declaration = "using " + alias.getNameAsString() + " = " +
                             spelling(alias.getUnderlyingType()) + ";\n";
    place_declaration(alias, declaration);
  }

  void emit_enum(const clang::EnumDecl &enumeration, bool allow_anonymous = false) {
    if (const auto header = header_discovery_.headerFor(enumeration, excluded_headers_)) {
      standard_headers_.insert(*header);
      return;
    }
    if (enumeration.isInStdNamespace())
      unsupported(enumeration, "cannot resolve a public toolchain header");
    const auto *decl = enumeration.getDefinition();
    const auto name = decl ? linkage_name(*decl) : std::string();
    if (!decl || (!allow_anonymous && name.empty()))
      unsupported(enumeration, "enumeration requires a named definition");
    if (emitted_.count(decl->getCanonicalDecl()) || defer_to_owner(*decl))
      return;
    emitted_.insert(decl->getCanonicalDecl());
    // An unnamed enumeration that a typedef names is declared through that typedef.
    const bool by_typedef = !decl->getIdentifier() && !name.empty();
    std::string source = by_typedef ? "typedef " : "";
    source += decl->isScoped() ? "enum class " : "enum ";
    if (decl->getIdentifier())
      source += name + " ";
    source += ": " + spelling(decl->getIntegerType()) + " {\n";
    for (const auto *value : decl->enumerators()) {
      const auto &initial = value->getInitVal();
      llvm::SmallString<32> number;
      if (initial.isSigned() && initial.isMinSignedValue() && initial.getBitWidth() >= 64) {
        // No literal has the most negative value: `-9223372036854775808` negates a number
        // that does not fit.
        auto next = initial;
        ++next;
        next.toString(number, 10);
        number += " - 1";
      } else {
        initial.toString(number, 10);
        if (initial.isUnsigned() && initial.getActiveBits() > 63)
          number += "ULL";
      }
      source += value->getNameAsString() + " = " + number.str().str() + ",\n";
    }
    place_declaration(*decl, source + (by_typedef ? "} " + name + ";\n" : "};\n"));
  }

  void emit_constant(const clang::VarDecl &constant) {
    if (const auto header = header_discovery_.headerFor(constant, excluded_headers_)) {
      standard_headers_.insert(*header);
      return;
    }
    const auto *decl = constant.getInitializingDeclaration();
    if (!decl || !decl->isUsableInConstantExpressions(ast_) ||
        !decl->getType()->isIntegralOrEnumerationType() || !decl->getIdentifier())
      unsupported(constant, "template default requires an integral constant definition");
    const auto *value = decl->evaluateValue();
    if (!value || !value->isInt())
      unsupported(constant, "cannot evaluate template default constant");
    if (emitted_.count(decl->getCanonicalDecl()) || defer_to_owner(*decl))
      return;
    emitted_.insert(decl->getCanonicalDecl());
    const auto type = decl->getType().getUnqualifiedType();
    visit(type);
    const auto literal = print_template_arg(clang::TemplateArgument(ast_, value->getInt(), type));
    place_declaration(*decl, std::string(decl->isStaticDataMember() ? "static " : "") +
        "constexpr " + spelling(type, decl->getName()) + " = " + literal + ";\n");
  }

  void place_declaration(const clang::NamedDecl &decl, std::string source) {
    if (const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(decl.getDeclContext()))
      nested_declarations_[owner->getCanonicalDecl()].push_back(std::move(source));
    else
      declarations_ += in_namespace(decl, std::move(source));
  }

  void remember_late_nested(const clang::NamedDecl &decl, bool complete) {
    if (!first_late_dependency_)
      first_late_dependency_ = &decl;
    const auto known = std::find_if(late_requests_.begin(), late_requests_.end(),
        [&](const NestedRequest &request) {
          return request.decl->getCanonicalDecl() == decl.getCanonicalDecl() &&
                 (request.complete || !complete);
        });
    if (known == late_requests_.end())
      late_requests_.push_back({&decl, complete});
  }

  bool defer_to_owner(const clang::NamedDecl &decl) {
    const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(decl.getDeclContext());
    if (!owner || emitting_.count(owner->getCanonicalDecl()))
      return false;
    if (emitted_.count(owner->getCanonicalDecl())) {
      remember_late_nested(decl, /*complete=*/true);
      return false;
    }
    nested_declarations_[owner->getCanonicalDecl()].push_back(NestedRequest{&decl, true});
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

  bool include_toolchain_record(const clang::CXXRecordDecl &record) {
    const auto header = header_discovery_.headerFor(record, excluded_headers_);
    if (!header) {
      if (record.isInStdNamespace())
        unsupported(record, "cannot resolve a public toolchain header");
      return false;
    }
    standard_headers_.emplace(*header);
    if (const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&record))
      for (const auto &argument : special->getTemplateArgs().asArray())
        visit_template_arg(argument);
    return true;
  }

  std::string print_template_arg(const clang::TemplateArgument &arg) {
    std::string result;
    llvm::raw_string_ostream out(result);
    arg.print(policy_, out, /*IncludeType=*/true);
    return result;
  }

  void visit_default_expression(const clang::Stmt &expression) {
    if (const auto *reference = llvm::dyn_cast<clang::DeclRefExpr>(&expression)) {
      if (const auto *enumerator = llvm::dyn_cast<clang::EnumConstantDecl>(reference->getDecl()))
        emit_enum(*llvm::cast<clang::EnumDecl>(enumerator->getDeclContext()),
                  /*allow_anonymous=*/true);
      else if (const auto *constant = llvm::dyn_cast<clang::VarDecl>(reference->getDecl()))
        emit_constant(*constant);
    }
    for (const auto *child : expression.children())
      if (child)
        visit_default_expression(*child);
  }

  void visit_template_arg(const clang::TemplateArgument &arg, bool complete = true) {
    if (arg.getKind() == clang::TemplateArgument::Type) {
      visit(arg.getAsType(), complete);
    } else if (arg.getKind() == clang::TemplateArgument::Integral) {
      if (const auto *enumeration = arg.getIntegralType()->getAs<clang::EnumType>())
        emit_enum(*enumeration->getDecl(), /*allow_anonymous=*/true);
    } else if (arg.getKind() == clang::TemplateArgument::Pack) {
      for (const auto &element : arg.pack_elements())
        visit_template_arg(element, complete);
    }
  }

  std::string template_default(const clang::TemplateArgument &arg) {
    if (arg.getKind() == clang::TemplateArgument::Type) {
      const auto type = arg.getAsType();
      visit(type, /*complete=*/false);
      // Preserve dependent parameter names, such as U = T.
      return type->isDependentType() ? type.getAsString(policy_) : spelling(type);
    }
    if (arg.getKind() == clang::TemplateArgument::Expression) {
      const auto *expression = arg.getAsExpr();
      visit_default_expression(*expression);
      if (const auto *reference =
              llvm::dyn_cast<clang::DeclRefExpr>(expression->IgnoreParenImpCasts())) {
        const auto *decl = reference->getDecl();
        if (llvm::isa<clang::VarDecl, clang::EnumConstantDecl>(decl)) {
          std::string result;
          llvm::raw_string_ostream out(result);
          out << "::";
          decl->printQualifiedName(out, policy_);
          return result;
        }
      }
    }
    visit_template_arg(arg, /*complete=*/false);
    return print_template_arg(arg);
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
    } else if (const auto *member = type->getAs<clang::MemberPointerType>()) {
      // A member pointer has the same layout whether or not its class is complete.
      visit(record_type(ast_, member->getMostRecentCXXRecordDecl()), false);
      visit(member->getPointeeType(), false);
    } else if (type->isNullPtrType()) {
      standard_headers_.insert("cstddef");
    } else if (const auto *function = type->getAs<clang::FunctionProtoType>()) {
      visit(function->getReturnType(), false);
      for (auto parameter : function->param_types())
        visit(parameter, false);
    } else if (const auto *record = type->getAsCXXRecordDecl()) {
      if (include_toolchain_record(*record))
        return;
      // Only the member that declares an unnamed record can name it, and
      // emit_record declares that member in place.
      if (!record->getIdentifier())
        unnamed_definition(*record);
      if (complete && record->getDefinition())
        emit_record(*record->getDefinition());
      else
        forward(*record);
    } else if (const auto *enumeration = type->getAs<clang::EnumType>()) {
      emit_enum(*enumeration->getDecl());
    } else if (const auto *specialization = type->getAs<clang::TemplateSpecializationType>()) {
      if (const auto *primary = llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(
              specialization->getTemplateName().getAsTemplateDecl()))
        forward_template(*primary);
      for (const auto &argument : specialization->template_arguments())
        visit_template_arg(argument, /*complete=*/false);
    }
  }

  void forward_template(const clang::ClassTemplateDecl &primary) {
    if (!forwarded_.insert(primary.getCanonicalDecl()).second ||
        include_toolchain_record(*primary.getTemplatedDecl()))
      return;
    const auto *declaration = primary.getMostRecentDecl();
    const auto &record = *declaration->getTemplatedDecl();
    std::string source = "template<";
    bool first = true;
    for (const auto *parameter : *declaration->getTemplateParameters()) {
      if (!first)
        source += ", ";
      first = false;
      std::string default_value;
      const auto name = parameter->getNameAsString();
      if (const auto *type = llvm::dyn_cast<clang::TemplateTypeParmDecl>(parameter)) {
        source += std::string(type->isParameterPack() ? "class... " : "class ") + name;
        if (type->hasDefaultArgument())
          default_value = template_default(type->getDefaultArgument().getArgument());
      } else if (const auto *value = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(parameter)) {
        // A static enum constant and its parameter type can share an owner.
        // Discover the constant before the type can emit that owner.
        if (value->hasDefaultArgument())
          default_value = template_default(value->getDefaultArgument().getArgument());
        visit(value->getType(), /*complete=*/false);
        const auto parameter_name = (value->isParameterPack() ? "..." : "") + name;
        if (value->getType()->isDependentType()) {
          llvm::raw_string_ostream out(source);
          value->getType().print(out, policy_, parameter_name);
        } else {
          source += spelling(value->getType(), parameter_name);
        }
      } else {
        unsupported(record, "cannot extract the primary template parameters");
      }
      if (!default_value.empty())
        source += " = " + default_value;
    }
    declarations_ += in_namespace(record, source + "> " +
        std::string(record.getKindName()) + " " + record.getNameAsString() + ";\n");
  }

  void forward(const clang::CXXRecordDecl &record) {
    if (record.isDependentType())
      unsupported(record, "requires a named concrete declaration");
    if (!record.getIdentifier()) {
      // A use that needs only a declaration gets the definition.
      emit_record(unnamed_definition(record));
      return;
    }
    // A nested name can only be declared inside its owner. Its complete
    // declaration is queued there by emit_record instead.
    if (const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(record.getDeclContext())) {
      if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
        unsupported(record, "nested class template specializations are unsupported");
      if (!forwarded_.insert(record.getCanonicalDecl()).second)
        return;
      const auto *owner_id = owner->getCanonicalDecl();
      if (emitted_.count(owner_id) && !emitting_.count(owner_id))
        remember_late_nested(record, /*complete=*/false);
      nested_declarations_[owner_id].push_back(
          std::string(record.getKindName()) + " " + record.getNameAsString() + ";\n");
      ensure_nested_owner(*owner, record);
      return;
    }
    if (!forwarded_.insert(record.getCanonicalDecl()).second)
      return;
    if (const auto *special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&record)) {
      forward_template(*special->getSpecializedTemplate());
      // Template declarations must follow their argument dependencies.
      for (const auto &argument : special->getTemplateArgs().asArray())
        visit_template_arg(argument, /*complete=*/false);
      declarations_ += in_namespace(record, specialization_prefix(record) +
          std::string(record.getKindName()) + " " + record_name(record) + ";\n");
      for (const auto &argument : special->getTemplateArgs().asArray())
        visit_template_arg(argument);
    } else {
      forwards_ += in_namespace(record,
          std::string(record.getKindName()) + " " + record.getNameAsString() + ";\n");
    }
  }

  static bool within_unnamed_record(const clang::CXXRecordDecl &record) {
    for (const clang::DeclContext *scope = &record;; scope = scope->getParent()) {
      const auto *owner = llvm::dyn_cast<clang::CXXRecordDecl>(scope);
      if (!owner)
        return false;
      if (!owner->getIdentifier())
        return true;
    }
  }

  static std::string specialization_prefix(const clang::CXXRecordDecl &record) {
    return llvm::isa<clang::ClassTemplateSpecializationDecl>(record) ? "template<> " : "";
  }

  std::string record_name(const clang::CXXRecordDecl &record) {
    if (!llvm::isa<clang::ClassTemplateSpecializationDecl>(record))
      return record.getNameAsString();
    std::string result;
    llvm::raw_string_ostream out(result);
    // Keep the original defaulted arguments implicit in the generated
    // specialization too. Clang preserves that distinction in type names.
    record.getNameForDiagnostic(out, policy_, false);
    return result;
  }

  void emit_record(const clang::CXXRecordDecl &record) {
    if (include_toolchain_record(record))
      return;
    const auto *identity = record.getCanonicalDecl();
    if (emitted_.count(identity) || defer_to_owner(record))
      return;
    const bool unnamed = !record.getIdentifier();
    if (unnamed)
      unnamed_definition(record);
    else
      forward(record);
    if (!emitted_.insert(identity).second)
      return;
    emitting_.insert(identity);
    const auto source = record_definition(record);
    emitting_.erase(identity);
    place_declaration(record, source + (unnamed ? "} " + linkage_name(record) + ";\n" : "};\n"));
  }

  // The definition of a record up to its closing brace. The caller marks the record as
  // emitted and being emitted, and writes what follows the brace.
  std::string record_definition(const clang::CXXRecordDecl &record) {
    const auto *identity = record.getCanonicalDecl();
    const bool unnamed = !record.getIdentifier();
    const auto alignment = ast_.getASTRecordLayout(&record).getAlignment().getQuantity();
    std::string source = specialization_prefix(record) +
                         (unnamed && record.getTypedefNameForAnonDecl() ? "typedef " : "") +
                         std::string(record.getKindName()) + " alignas(" +
                         std::to_string(alignment) + ") ";
    if (record.hasAttr<clang::PackedAttr>())
      source += "__attribute__((packed)) ";
    if (unnamed)
      source.pop_back();
    else
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
    std::vector<std::pair<clang::AccessSpecifier, std::string>> members;
    const auto &sm = ast_.getSourceManager();
    const bool extra_header =
        extra_headers_.count(sm.getFilename(sm.getExpansionLoc(record.getLocation())).str()) != 0;
    for (const auto *member : record.decls()) {
      if (const auto *enumeration = llvm::dyn_cast<clang::EnumDecl>(member);
          extra_header && enumeration && enumeration->isCompleteDefinition()) {
        emit_enum(*enumeration, /*allow_anonymous=*/true);
      } else if (const auto *constant = llvm::dyn_cast<clang::VarDecl>(member);
                 constant && constant->isConstexpr() && constant->hasInit() &&
                 constant->getType()->isIntegerType() && !constant->getType()->isDependentType()) {
        // Emit referenced constants before a later template default needs them.
        // Limit new eager emission to values the adapter can print as literals.
        if (extra_header || (constant->isReferenced() &&
                             constant->isUsableInConstantExpressions(ast_) &&
                             ast_.getIntWidth(constant->getType()) <= 64))
          emit_constant(*constant);
      }
    }
    const std::vector<const clang::FieldDecl *> record_fields(record.field_begin(),
                                                              record.field_end());
    for (std::size_t index = 0; index < record_fields.size(); ++index) {
      const auto *field = record_fields[index];
      if (const auto *unnamed = declared_in_place(record, *field)) {
        std::vector<const clang::FieldDecl *> group{field};
        while (index + 1 < record_fields.size() &&
               declared_in_place(record, *record_fields[index + 1]) == unnamed)
          group.push_back(record_fields[++index]);
        members.emplace_back(field->getAccess(), emit_in_place(*unnamed, group));
        continue;
      }
      visit(field->getType());
      std::string member;
      if (field->hasAttr<clang::NoUniqueAddressAttr>())
        member += "[[no_unique_address]] ";
      if (field->hasAttr<clang::PackedAttr>())
        member += "__attribute__((packed)) ";
      if (const auto alignment = field->getMaxAlignment())
        member += "alignas(" + std::to_string(alignment / ast_.getCharWidth()) + ") ";
      if (field->isMutable())
        member += "mutable ";
      member += spelling(field->getType(), field->getName());
      if (field->isBitField())
        member += " : " + std::to_string(field->getBitWidthValue());
      members.emplace_back(field->getAccess(), member + ";\n");
    }
    bool emitted_selected = false;
    bool emitted_default_constructor = false;
    for (const auto *method : record.methods()) {
      if (method->isImplicit())
        continue;
      if (method->isVirtual()) {
        const bool selected = selected_method_ &&
            method->getCanonicalDecl() == selected_method_->getCanonicalDecl();
        visit_signature(*method, selected);
        members.emplace_back(method->getAccess(), callable(*method, selected));
        emitted_selected |= selected;
        continue;
      }
      if (method->isCopyAssignmentOperator() || method->isMoveAssignmentOperator()) {
        visit_signature(*method);
        auto declaration = callable(*method);
        if (method->isExplicitlyDefaulted() && !method->isDeletedAsWritten())
          declaration.insert(declaration.rfind(';'), " = default");
        members.emplace_back(method->getAccess(), std::move(declaration));
        continue;
      }
      const auto *ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
      const bool destructor = llvm::isa<clang::CXXDestructorDecl>(method);
      const bool custom = custom_lifetime(*method);
      if ((!ctor && !destructor) ||
          (!custom && !method->isExplicitlyDefaulted() && !method->isDeletedAsWritten()))
        continue;
      emitted_default_constructor |= ctor && ctor->getNumParams() == 0;
      std::string member = destructor ? "~" : "";
      member += record.getNameAsString() + "(";
      // A copy or move constructor can have trailing default arguments.
      // Only its first parameter is needed to retain its special-member role.
      const auto parameters = custom && ctor ? 1U : method->getNumParams();
      for (unsigned i = 0; i < parameters; ++i) {
        if (i)
          member += ", ";
        member += spelling(method->getParamDecl(i)->getType());
      }
      member += ")";
      if (custom) {
        // Keep the call ABI without compiling application lifetime code.
        member += exception_suffix(*method);
      } else {
        member += method->isDeletedAsWritten() ? " = delete" : " = default";
      }
      // This lifetime declaration already is the selected constructor or destructor.
      emitted_selected |= selected_method_ &&
          method->getCanonicalDecl() == selected_method_->getCanonicalDecl();
      members.emplace_back(method->getAccess(), member + ";\n");
    }
    if (const auto recipe = record_factory::infer(record)) {
      members.emplace_back(recipe->constructor->getAccess(), emit_factory_constructor(*recipe));
    } else if (record.hasUserDeclaredConstructor() && !emitted_default_constructor &&
               (!record.hasDefaultConstructor() || record.isDynamicClass() || record.getNumBases())) {
      // Dropping application constructor bodies must not give a layout-only
      // adapter an implicit constructor for local snapshot reconstruction.
      members.emplace_back(clang::AS_private, record.getNameAsString() + "() = delete;\n");
    }
    if (!emitted_selected && selected_method_ &&
        selected_method_->getParent()->getCanonicalDecl() == record.getCanonicalDecl() &&
        !nested_static_method(selected_method_)) {
      visit_signature(*selected_method_);
      members.emplace_back(clang::AS_public, callable(*selected_method_));
    }
    // A typedef may name an unnamed class for linkage only when the class is C-compatible,
    // and an access specifier is a member declaration that makes it otherwise. Inside such
    // a record a specifier is therefore written only where the access changes.
    const bool plain = within_unnamed_record(record);
    auto current = record.isClass() ? clang::AS_private : clang::AS_public;
    const auto add = [&](clang::AccessSpecifier wanted, const std::string &member) {
      if (!plain || wanted != current)
        source += access(wanted) + ": ";
      current = wanted;
      source += member;
    };
    for (const auto &nested : take_nested_declarations(identity))
      add(clang::AS_public, nested);
    for (const auto &[wanted, member] : members)
      add(wanted, member);
    return source;
  }

  // The unnamed record a member declares, if nothing else can name it: no identifier and no
  // typedef name for linkage. Only the declaration of such a member can define it.
  const clang::CXXRecordDecl *declared_in_place(const clang::CXXRecordDecl &owner,
                                                const clang::FieldDecl &field) {
    auto type = field.getType();
    while (const auto *array = ast_.getAsArrayType(type))
      type = array->getElementType();
    const auto *unnamed = type->getAsCXXRecordDecl();
    unnamed = unnamed ? unnamed->getDefinition() : nullptr;
    if (!unnamed || !linkage_name(*unnamed).empty())
      return nullptr;
    const auto *scope = llvm::dyn_cast<clang::CXXRecordDecl>(unnamed->getDeclContext());
    return scope && scope->getCanonicalDecl() == owner.getCanonicalDecl() ? unnamed : nullptr;
  }

  // `struct { ... } a, b[2];` for the consecutive members that share one unnamed record.
  // Its size, alignment and offsets stay those of the original, and so do the members after it.
  std::string emit_in_place(const clang::CXXRecordDecl &unnamed,
                            const std::vector<const clang::FieldDecl *> &group) {
    const auto *identity = unnamed.getCanonicalDecl();
    if (!emitted_.insert(identity).second)
      unsupported(unnamed, ("an unnamed " + std::string(unnamed.getKindName()) +
                            " shared by members that are not consecutive is unsupported").c_str());
    const auto &first = *group.front();
    const auto attributes = [&](const clang::FieldDecl &field) {
      if (field.hasAttr<clang::PackedAttr>())
        unsupported(field, "a packed member of an unnamed record type is unsupported");
      std::string result;
      if (field.hasAttr<clang::NoUniqueAddressAttr>())
        result += "[[no_unique_address]] ";
      if (const auto alignment = field.getMaxAlignment())
        result += "alignas(" + std::to_string(alignment / ast_.getCharWidth()) + ") ";
      if (field.isMutable())
        result += "mutable ";
      return result;
    };
    const auto qualifiers = [&](const clang::FieldDecl &field) {
      auto type = field.getType();
      while (const auto *array = ast_.getAsArrayType(type))
        type = array->getElementType();
      return std::string(type.isConstQualified() ? "const " : "") +
             (type.isVolatileQualified() ? "volatile " : "");
    };
    std::string declarators;
    for (const auto *field : group) {
      if (attributes(*field) != attributes(first) || qualifiers(*field) != qualifiers(first) ||
          field->getAccess() != first.getAccess())
        unsupported(*field, "members that share an unnamed record type must be declared alike");
      std::string extents;
      for (auto type = field->getType(); const auto *array = ast_.getAsArrayType(type);
           type = array->getElementType()) {
        if (const auto *constant = llvm::dyn_cast<clang::ConstantArrayType>(array))
          extents += "[" + std::to_string(constant->getSize().getZExtValue()) + "]";
        else if (llvm::isa<clang::IncompleteArrayType>(array))
          extents += "[]";
        else
          unsupported(*field, "variable-length members are unsupported");
      }
      if (!declarators.empty())
        declarators += ", ";
      declarators += field->getNameAsString() + extents;
    }
    emitting_.insert(identity);
    auto source = attributes(first) + qualifiers(first) + record_definition(unnamed) + "}";
    emitting_.erase(identity);
    return source + (declarators.empty() ? "" : " " + declarators) + ";\n";
  }

  // The declarations queued for `owner`, each one a public member.
  std::vector<std::string> take_nested_declarations(const clang::CXXRecordDecl *owner) {
    std::vector<std::string> source;
    for (auto found = nested_declarations_.find(owner); found != nested_declarations_.end();
         found = nested_declarations_.find(owner)) {
      auto pending = std::move(found->second);
      nested_declarations_.erase(found);
      for (const auto &entry : pending) {
        if (const auto *request = std::get_if<NestedRequest>(&entry)) {
          const auto *declaration = request->decl;
          if (!request->complete)
            forward(*llvm::cast<clang::CXXRecordDecl>(declaration));
          else if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
            emit_record(*record);
          else if (const auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(declaration))
            emit_alias(*alias);
          else if (const auto *constant = llvm::dyn_cast<clang::VarDecl>(declaration))
            emit_constant(*constant);
          else
            emit_enum(*llvm::cast<clang::EnumDecl>(declaration), /*allow_anonymous=*/true);
        } else {
          source.push_back(std::get<std::string>(entry));
        }
      }
    }
    return source;
  }

  clang::ASTContext &ast_;
  clang::Sema &sema_;
  clang::PrintingPolicy policy_;
  HeaderDiscovery header_discovery_;
  const clang::CXXMethodDecl *selected_method_ = nullptr;
  std::set<std::string> extra_headers_;
  std::set<std::string> excluded_headers_;
  std::set<std::string> standard_headers_;
  std::unordered_set<const clang::Decl *> emitted_;
  std::unordered_set<const clang::Decl *> forwarded_;
  using NestedEntry = std::variant<std::string, NestedRequest>;
  std::unordered_map<const clang::CXXRecordDecl *, std::vector<NestedEntry>> nested_declarations_;
  std::unordered_set<const clang::Decl *> emitting_;
  std::string forwards_;
  std::string declarations_;
  std::vector<NestedRequest> &late_requests_;
  const clang::NamedDecl *first_late_dependency_ = nullptr;
};

} // namespace tracer::prepared

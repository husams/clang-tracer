#pragma once

#include "prepared_types.hpp"
#include "prepared_header_discovery.hpp"
#include "record_factory.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/Basic/SourceManager.h>
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

class AdapterEmitter {
public:
  static AdapterDeclarations emit(clang::ASTContext &ast,
                                  const clang::HeaderSearch &toolchain_search,
                                  const clang::FunctionDecl &function,
                                  const std::vector<std::string> &headers,
                                  bool pointer_layouts,
                                  const std::vector<const clang::CXXRecordDecl *> &snapshot_types = {}) {
    std::vector<NestedRequest> late_requests;
    for (;;) {
      const auto reserved = late_requests.size();
      AdapterEmitter pass(ast, toolchain_search, late_requests);
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

  AdapterEmitter(clang::ASTContext &ast,
                 const clang::HeaderSearch &toolchain_search,
                 std::vector<NestedRequest> &late_requests)
      : ast_(ast), policy_(ast.getLangOpts()),
        header_discovery_(ast, toolchain_search), late_requests_(late_requests) {
    policy_.SuppressTagKeyword = true;
    policy_.FullyQualifiedName = true;
  }

  void emit_declarations(const clang::FunctionDecl &function,
                         const std::vector<std::string> &headers,
                         bool pointer_layouts,
                         const std::vector<const clang::CXXRecordDecl *> &snapshot_types) {
    selected_method_ = llvm::dyn_cast<clang::CXXMethodDecl>(&function);
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
          std::to_string(i) + " = " + spelling(ast_.getRecordType(snapshot_type)) + "; }\n";
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

  std::string emit_factory_constructor(const record_factory::Recipe &recipe) {
    const auto &ctor = *recipe.constructor;
    const auto argument_name = [](unsigned index) {
      return "__tracer_ctor_arg" + std::to_string(index);
    };
    std::string source = access(ctor.getAccess()) + ": ";
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
    const auto *prototype = decl.getType()->castAs<clang::FunctionProtoType>();
    if (!clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()) &&
        prototype->isNothrow())
      name += " noexcept";
    source += (destructor || conversion) ? name : spelling(decl.getReturnType(), name);
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
          !is_follow_pointers_specialization(record))
        emit_record(*record);
      if (const auto *enumeration = llvm::dyn_cast<clang::EnumDecl>(decl);
          enumeration && enumeration->isCompleteDefinition() && enumeration->getIdentifier())
        emit_enum(*enumeration);
      if (const auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(decl))
        emit_alias(*alias);
    }
  }

  static bool is_follow_pointers_specialization(const clang::CXXRecordDecl *record) {
    const auto *specialization =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
    const auto *templ = specialization ? specialization->getSpecializedTemplate() : nullptr;
    return templ &&
           templ->getQualifiedNameAsString() == "tracer_snapshot::FollowPointers";
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

  void emit_enum(const clang::EnumDecl &enumeration, bool allow_anonymous = false) {
    if (const auto header = header_discovery_.headerFor(enumeration, excluded_headers_)) {
      standard_headers_.insert(*header);
      return;
    }
    if (enumeration.isInStdNamespace())
      unsupported(enumeration, "cannot resolve a public toolchain header");
    const auto *decl = enumeration.getDefinition();
    if (!decl || (!allow_anonymous && !decl->getIdentifier()))
      unsupported(enumeration, "enumeration requires a named definition");
    if (emitted_.count(decl->getCanonicalDecl()) || defer_to_owner(*decl))
      return;
    emitted_.insert(decl->getCanonicalDecl());
    std::string source = decl->isScoped() ? "enum class " : "enum ";
    if (decl->getIdentifier())
      source += decl->getNameAsString() + " ";
    source += ": " + spelling(decl->getIntegerType()) + " {\n";
    for (const auto *value : decl->enumerators()) {
      llvm::SmallString<32> number;
      value->getInitVal().toString(number, 10);
      source += value->getNameAsString() + " = " + number.str().str() + ",\n";
    }
    place_declaration(*decl, source + "};\n");
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
      nested_declarations_[owner->getCanonicalDecl()].push_back("public: " + std::move(source));
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
    } else if (const auto *function = type->getAs<clang::FunctionProtoType>()) {
      visit(function->getReturnType(), false);
      for (auto parameter : function->param_types())
        visit(parameter, false);
    } else if (const auto *record = type->getAsCXXRecordDecl()) {
      if (include_toolchain_record(*record))
        return;
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
        remember_late_nested(record, /*complete=*/false);
      nested_declarations_[owner_id].push_back("public: " +
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
    forward(record);
    if (!emitted_.insert(identity).second)
      return;
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
    bool emitted_default_constructor = false;
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
      emitted_default_constructor |= ctor && ctor->getNumParams() == 0;
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
    if (const auto recipe = record_factory::infer(record)) {
      source += emit_factory_constructor(*recipe);
    } else if (record.hasUserDeclaredConstructor() && !emitted_default_constructor &&
               (!record.hasDefaultConstructor() || record.isDynamicClass() || record.getNumBases())) {
      // Dropping application constructor bodies must not give a layout-only
      // adapter an implicit constructor for local snapshot reconstruction.
      source += "private: " + record.getNameAsString() + "() = delete;\n";
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
          source += std::get<std::string>(entry);
        }
      }
    }
    return source;
  }

  clang::ASTContext &ast_;
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

#include <algorithm>
#include <set>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Basic/Version.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

#include "context_binding.hpp"
#include "context_paths.hpp"
#include "tracer/compat.hpp"

namespace tracer {
namespace {
llvm::Error invalid(const std::string &reason) {
  return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                 "incompatible_context_contract: " + reason);
}

bool physical_standard_library_record(clang::ASTContext &context,
                                      const clang::CXXRecordDecl *record) {
  if (!record || record->getQualifiedNameAsString().rfind("std::", 0) != 0)
    return false;
  const auto location =
      context.getSourceManager().getSpellingLoc(record->getLocation());
  const auto filename = context.getSourceManager().getFilename(location);
  if (filename.empty())
    return false;
  llvm::SmallString<256> physical;
  if (llvm::sys::fs::real_path(filename, physical) ||
      llvm::StringRef(physical).find("/include/c++/") == llvm::StringRef::npos)
    return false;
  llvm::StringRef roots(TRACER_CONTEXT_SYSTEM_ROOTS);
  while (!roots.empty()) {
    const auto pair = roots.split('|');
    roots = pair.second;
    if (pair.first.empty())
      continue;
    llvm::SmallString<256> root;
    if (!llvm::sys::fs::real_path(pair.first, root) &&
        ::tracer::compat::starts_with(llvm::StringRef(physical),
                                      llvm::StringRef(root).str() + "/")) {
      return true;
    }
  }
  return false;
}

bool builtin_va_list_record(clang::ASTContext &context,
                            const clang::CXXRecordDecl *record) {
  clang::QualType type =
      context.getCanonicalType(context.getBuiltinVaListType())
          .getUnqualifiedType();
  if (const auto *array = context.getAsArrayType(type))
    type = context.getCanonicalType(array->getElementType());
  while (type->isPointerType() || type->isReferenceType())
    type = context.getCanonicalType(type->getPointeeType());
  const auto *builtin_record = type->getAsCXXRecordDecl();
  return builtin_record &&
         builtin_record->getCanonicalDecl() == record->getCanonicalDecl();
}

class Layouts final {
public:
  explicit Layouts(clang::ASTContext &context)
      : context_(context), policy_(context.getLangOpts()) {
    policy_.FullyQualifiedName = true;
    policy_.PrintAsCanonical = true;
  }

  llvm::Error add(clang::QualType type, unsigned depth = 0,
                  bool opaque_borrow = false, bool include_scalar_abi = false) {
    if (depth > 32 || entries_.size() > 1024) {
      return invalid("recursive type layout budget exceeded");
    }
    type = context_.getCanonicalType(type).getUnqualifiedType();
    if (const auto *array = context_.getAsArrayType(type)) {
      return add(array->getElementType(), depth + 1, false, include_scalar_abi);
    }
    const auto *record = type->getAsCXXRecordDecl();
    if (!record) {
      if (!include_scalar_abi) {
        return llvm::Error::success();
      }
      if (!type->isBuiltinType() && !type->isEnumeralType() &&
          !type->isPointerType() && !type->isMemberPointerType()) {
        return llvm::Error::success();
      }
      const auto name = type.getAsString(policy_);
      if (!seen_.insert(name).second) {
        return llvm::Error::success();
      }
      ContextTypeLayout entry;
      entry.name = name;
      entry.bytes = context_.getTypeSizeInChars(type).getQuantity();
      entry.alignment = context_.getTypeAlignInChars(type).getQuantity();
      if (const auto *builtin = type->getAs<clang::BuiltinType>()) {
        entry.definition =
            "builtin:" +
            std::to_string(static_cast<unsigned>(builtin->getKind()));
      } else if (const auto *enumeration = type->getAs<clang::EnumType>()) {
        entry.definition = "enum:" + enumeration->getDecl()
                                         ->getIntegerType()
                                         .getCanonicalType()
                                         .getAsString(policy_);
      } else {
        entry.definition = "pointer";
      }
      bytes_ += entry.definition.size() + entry.name.size();
      if (bytes_ > 4 * 1024 * 1024) {
        return invalid("type definition budget exceeded");
      }
      entries_.push_back(std::move(entry));
      return llvm::Error::success();
    }
    const auto name = type.getAsString(policy_);
    if (!seen_.insert(name).second) {
      return llvm::Error::success();
    }
    record = record->getDefinition();
    if (!record && opaque_borrow) {
      entries_.push_back({name, "<opaque borrowed record>", 0, 0});
      return llvm::Error::success();
    }
    if (!record || record->isDependentType() ||
        !record->isCompleteDefinition() || record->isInvalidDecl()) {
      return invalid("incomplete record " + name);
    }
    ContextTypeLayout entry;
    entry.name = name;
    const auto &layout = context_.getASTRecordLayout(record);
    entry.bytes = layout.getSize().getQuantity();
    entry.alignment = layout.getAlignment().getQuantity();
    llvm::raw_string_ostream definition(entry.definition);
    const bool opaque_standard =
        physical_standard_library_record(context_, record);
    definition << "record:" << static_cast<unsigned>(record->getTagKind())
               << ";size:" << layout.getSize().getQuantity()
               << ";align:" << layout.getAlignment().getQuantity()
               << ";nonvirtual:" << layout.getNonVirtualSize().getQuantity()
               << ':' << layout.getNonVirtualAlignment().getQuantity()
               << ";polymorphic:" << record->isPolymorphic()
               << ";abstract:" << record->isAbstract()
               << ";dynamic:" << record->isDynamicClass()
               << ";virtual-bases:" << record->getNumVBases();
    if (!opaque_standard) {
      bool invalid_range = false;
      const auto source_text = clang::Lexer::getSourceText(
          clang::CharSourceRange::getTokenRange(record->getSourceRange()),
          context_.getSourceManager(), context_.getLangOpts(), &invalid_range);
      if ((invalid_range || source_text.empty()) &&
          !builtin_va_list_record(context_, record)) {
        return invalid("record source definition cannot be captured for " +
                       name);
      }
      if (!invalid_range && !source_text.empty())
        definition << ";source:" << source_text.size() << ':' << source_text;
      else
        definition << ";builtin-va-list-record";
    }
    unsigned index = 0;
    if (!opaque_standard) {
      for (const auto *field : record->fields()) {
        const auto field_name = field->getNameAsString();
        const auto field_type =
            field->getType().getCanonicalType().getAsString(policy_);
        definition << ";field:" << layout.getFieldOffset(index++) << ':'
                   << field_name.size() << ':' << field_name << ':'
                   << field_type.size() << ':' << field_type << ':'
                   << static_cast<unsigned>(field->getAccess()) << ':'
                   << field->isMutable() << ':' << field->isBitField();
        if (field->isBitField()) {
          definition << ':' << field->getBitWidthValue();
        }
        if (auto error = add(field->getType(), depth + 1)) {
          return error;
        }
      }
      for (const auto &base : record->bases()) {
        const auto *decl = base.getType()->getAsCXXRecordDecl();
        if (!decl) {
          return invalid("unresolved base of " + name);
        }
        const auto base_type =
            base.getType().getCanonicalType().getAsString(policy_);
        definition << ";base:" << base_type.size() << ':' << base_type << ':'
                   << static_cast<unsigned>(base.getAccessSpecifier()) << ':'
                   << base.isVirtual() << ':'
                   << (base.isVirtual() ? layout.getVBaseClassOffset(decl)
                                        : layout.getBaseClassOffset(decl))
                          .getQuantity();
        if (auto error = add(base.getType(), depth + 1)) {
          return error;
        }
      }
    }
    if (const auto *special =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      for (const auto &argument : special->getTemplateArgs().asArray()) {
        if (argument.getKind() == clang::TemplateArgument::Type) {
          const auto *record = argument.getAsType()->getAsCXXRecordDecl();
          // A standard allocator/node policy can name an uninstantiated
          // implementation detail without embedding its layout in this value.
          // User element records remain mandatory; emitted support operations
          // still require their own complete bridge/layout admission.
          if (record && !record->hasDefinition() &&
              physical_standard_library_record(context_, record)) {
            continue;
          }
          if (auto error = add(argument.getAsType(), depth + 1, false, true)) {
            return error;
          }
        }
      }
    }
    bytes_ += entry.definition.size();
    if (bytes_ > 4 * 1024 * 1024) {
      return invalid("type definition budget exceeded");
    }
    entries_.push_back(std::move(entry));
    return llvm::Error::success();
  }

  std::vector<ContextTypeLayout> take() {
    std::sort(entries_.begin(), entries_.end(),
              [](const auto &a, const auto &b) { return a.name < b.name; });
    return std::move(entries_);
  }

private:
  clang::ASTContext &context_;
  clang::PrintingPolicy policy_;
  std::set<std::string> seen_;
  std::vector<ContextTypeLayout> entries_;
  std::size_t bytes_ = 0;
};

class Bridges final : public clang::RecursiveASTVisitor<Bridges> {
public:
  Bridges(clang::ASTContext &context, ContextCompilationContract &contract,
          std::string_view owned_input_marker)
      : context_(context), contract_(contract),
        names_(context.createMangleContext()) {
    if (owned_input_marker.empty()) {
      return;
    }
    struct MarkerFinder final : clang::RecursiveASTVisitor<MarkerFinder> {
      explicit MarkerFinder(std::string_view requested) : name(requested) {}
      bool VisitTypeAliasDecl(clang::TypeAliasDecl *alias) {
        if (alias->getName().str() == name) {
          location = alias->getLocation();
        }
        return true;
      }
      std::string_view name;
      clang::SourceLocation location;
    } finder(owned_input_marker);
    finder.TraverseDecl(context.getTranslationUnitDecl());
    if (finder.location.isValid()) {
      auto &source_manager = context.getSourceManager();
      owned_input_file_ = source_manager.getFileID(
          source_manager.getExpansionLoc(finder.location));
    } else {
      error_ = "owned input marker was not found in the parsed buffer";
    }
  }

  bool VisitFunctionDecl(clang::FunctionDecl *function) {
    auto &source_manager = context_.getSourceManager();
    const auto location =
        source_manager.getSpellingLoc(function->getLocation());
    const auto expansion_location =
        source_manager.getExpansionLoc(function->getLocation());
    const bool emitted_owned_declaration =
        !owned_input_file_.isInvalid() &&
        source_manager.getFileID(expansion_location) == owned_input_file_;
    const bool emitted_main_declaration =
        emitted_owned_declaration ||
        source_manager.isWrittenInMainFile(location);
    if (llvm::isa<clang::CXXMethodDecl>(function) ||
        function->isDependentContext() ||
        function->getType()->isDependentType() || !function->getIdentifier() ||
        (!function->isUsed() && !emitted_main_declaration)) {
      return true;
    }
    if (contract_.bridges.size() == 4096) {
      error_ = "bridge declaration budget exceeded";
      return false;
    }
    ContextCompilationContract::Bridge bridge;
    bridge.source_name = function->getQualifiedNameAsString();
    llvm::raw_string_ostream symbol(bridge.symbol);
    if (names_->shouldMangleDeclName(function)) {
      names_->mangleName(function, symbol);
    } else {
      symbol << function->getName();
    }
    if (!seen_.insert(bridge.symbol).second) {
      return true;
    }
    clang::PrintingPolicy policy(context_.getLangOpts());
    policy.FullyQualifiedName = true;
    policy.PrintAsCanonical = true;
    bridge.type = function->getType().getCanonicalType().getAsString(policy);
    Layouts layouts(context_);
    const auto add_type = [&](clang::QualType type) -> bool {
      bool borrowed = false;
      for (unsigned depth = 0; type->isPointerType() || type->isReferenceType();
           ++depth) {
        if (depth == 32) {
          error_ = "bridge pointee depth exceeded";
          return false;
        }
        type = type->getPointeeType();
        borrowed = true;
      }
      if (auto error = layouts.add(type, 0, borrowed)) {
        error_ = llvm::toString(std::move(error));
        return false;
      }
      return true;
    };
    if (!add_type(function->getReturnType())) {
      return false;
    }
    for (const auto *parameter : function->parameters()) {
      if (!add_type(parameter->getType())) {
        return false;
      }
    }
    bridge.records = layouts.take();
    contract_.bridges.push_back(std::move(bridge));
    return true;
  }

  std::string error_;

private:
  clang::ASTContext &context_;
  ContextCompilationContract &contract_;
  std::unique_ptr<clang::MangleContext> names_;
  clang::FileID owned_input_file_;
  std::set<std::string> seen_;
};

class StandardDefinitions final
    : public clang::RecursiveASTVisitor<StandardDefinitions> {
public:
  StandardDefinitions(clang::ASTContext &context, std::vector<std::string> &out)
      : context_(context), out_(out), names_(context.createMangleContext()) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitFunctionDecl(clang::FunctionDecl *function) {
    if (!function->doesThisDeclarationHaveABody() ||
        function->isDependentContext() ||
        function->getType()->isDependentType()) {
      return true;
    }
    auto &sm = context_.getSourceManager();
    const auto location = sm.getSpellingLoc(function->getLocation());
    const auto file = sm.getFileEntryRefForID(sm.getFileID(location));
    if (!file) {
      return true;
    }
    llvm::SmallString<256> physical;
    if (llvm::sys::fs::real_path(file->getName(), physical)) {
      return true;
    }
    bool trusted = false;
    llvm::SmallString<256> engine;
    if (!llvm::sys::fs::real_path(context_header_directory(), engine)) {
      const auto filename = llvm::sys::path::filename(physical);
      trusted = ::tracer::compat::starts_with(llvm::StringRef(physical),
                                              llvm::StringRef(engine).str() + "/") &&
                (filename == "context_facade.hpp" || filename == "context_codec.hpp" ||
                 filename == "bounded_proto.cpp" || filename == "bounded_proto.hpp");
      for (const auto &candidate :
           {llvm::StringRef(engine).str() + "/tracer/callback.hpp",
            llvm::StringRef(engine).str() +
                "/../include/tracer/callback.hpp"}) {
        llvm::SmallString<256> callback;
        if (!llvm::sys::fs::real_path(candidate, callback) &&
            callback == physical) {
          trusted = true;
        }
      }
    }
    llvm::StringRef roots(TRACER_CONTEXT_SYSTEM_ROOTS);
    while (!trusted && !roots.empty()) {
      auto pair = roots.split('|');
      roots = pair.second;
      llvm::SmallString<256> root;
      if (!pair.first.empty() && !llvm::sys::fs::real_path(pair.first, root) &&
          ::tracer::compat::starts_with(llvm::StringRef(physical),
                                        llvm::StringRef(root).str() + "/")) {
        trusted = true;
        break;
      }
    }
    if (!trusted) {
      return true;
    }
    auto add = [&](clang::GlobalDecl declaration) {
      std::string value;
      llvm::raw_string_ostream stream(value);
      if (names_->shouldMangleDeclName(function)) {
        names_->mangleName(declaration, stream);
      } else {
        stream << function->getName();
      }
      if (seen_.insert(value).second) {
        out_.push_back(std::move(value));
      }
    };
    if (auto *ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(function)) {
      add(clang::GlobalDecl(ctor, clang::Ctor_Base));
      add(clang::GlobalDecl(ctor, clang::Ctor_Complete));
    } else if (auto *dtor =
                   llvm::dyn_cast<clang::CXXDestructorDecl>(function)) {
      add(clang::GlobalDecl(dtor, clang::Dtor_Base));
      add(clang::GlobalDecl(dtor, clang::Dtor_Complete));
      add(clang::GlobalDecl(dtor, clang::Dtor_Deleting));
    } else {
      add(clang::GlobalDecl(function));
    }
    return true;
  }

private:
  clang::ASTContext &context_;
  std::vector<std::string> &out_;
  std::unique_ptr<clang::MangleContext> names_;
  std::set<std::string> seen_;
};

std::string canonical_record_type_name(clang::ASTContext &context,
                                       const clang::CXXRecordDecl *record) {
#if CLANG_VERSION_MAJOR >= 22
  return clang::QualType(context.getCanonicalTagType(record).getTypePtr(), 0)
      .getAsString();
#else
  return clang::QualType(record->getTypeForDecl(), 0)
      .getCanonicalType()
      .getAsString();
#endif
}

void find_records(clang::DeclContext &context, clang::ASTContext &ast_context,
                  const std::set<std::string> &wanted,
                  std::vector<const clang::CXXRecordDecl *> &found) {
  for (auto *decl : context.decls()) {
    if (const auto *record = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
      if (record->isThisDeclarationADefinition() && !record->isImplicit()) {
        const auto canonical = canonical_record_type_name(ast_context, record);
        if (::tracer::compat::contains(wanted, record->getQualifiedNameAsString()) ||
            ::tracer::compat::contains(wanted, canonical)) {
          found.push_back(record);
        }
      }
    }
    if (auto *space = llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
      find_records(*space, ast_context, wanted, found);
    }
  }
}

class ContractTypeAliases final
    : public clang::RecursiveASTVisitor<ContractTypeAliases> {
public:
  explicit ContractTypeAliases(const std::vector<std::string> &names)
      : names_(names) {}

  bool VisitTypeAliasDecl(clang::TypeAliasDecl *declaration) {
    const auto name = declaration->getNameAsString();
    if (std::find(names_.begin(), names_.end(), name) != names_.end()) {
      found_.emplace_back(name, declaration->getUnderlyingType());
    }
    return true;
  }

  const std::vector<std::pair<std::string, clang::QualType>> &found() const {
    return found_;
  }

private:
  const std::vector<std::string> &names_;
  std::vector<std::pair<std::string, clang::QualType>> found_;
};
} // namespace

llvm::Expected<std::vector<std::string>>
context_bridge_symbols(const ContextCompilationContract &contract,
                       const std::vector<std::string> &source_names) {
  std::vector<std::string> result;
  for (const auto &name : source_names) {
    const ContextCompilationContract::Bridge *found = nullptr;
    for (const auto &bridge : contract.bridges) {
      if (bridge.source_name != name) {
        continue;
      }
      if (found) {
        return invalid("ambiguous generated C++ bridge: " + name);
      }
      found = &bridge;
    }
    if (!found) {
      return invalid("missing generated C++ bridge: " + name);
    }
    result.push_back(found->symbol);
  }
  return result;
}

llvm::Expected<ContextCompilationContract>
context_compilation_contract(clang::ASTUnit &unit,
                             const std::vector<std::string> &state_types) {
  if (unit.getDiagnostics().hasErrorOccurred()) {
    return invalid("compiler diagnostics contain errors");
  }
  return context_compilation_contract(unit.getASTContext(),
                                      unit.getPreprocessor(), state_types);
}

llvm::Expected<ContextCompilationContract>
context_compilation_contract(clang::ASTContext &context,
                             clang::Preprocessor &preprocessor,
                             const std::vector<std::string> &state_types,
                             const std::vector<std::string> &type_aliases,
                             std::string_view owned_input_marker) {
  const auto &target = context.getTargetInfo();
  const auto &options = context.getLangOpts();
  ContextCompilationContract contract;
  contract.target = llvm::Triple::normalize(target.getTriple().str());
  contract.data_layout = target.getDataLayoutString();
  contract.compiler = CLANG_VERSION_STRING;
  llvm::raw_string_ostream language(contract.language);
  language << "CXXABI=" << static_cast<unsigned>(target.getCXXABI().getKind())
           << ';';
  // Serialize every table-defined language option rather than maintaining an
  // incomplete hand-picked list of ABI-affecting switches.
#define LANGOPT(Name, Bits, Default, ...)                                      \
  language << #Name "=" << options.Name << ';';
#define ENUM_LANGOPT(Name, Type, Bits, Default, ...)                           \
  language << #Name "=" << static_cast<unsigned>(options.get##Name()) << ';';
#include <clang/Basic/LangOptions.def>
  const auto &target_options = target.getTargetOpts();
  language << "CPU=" << target_options.CPU << ";ABI=" << target_options.ABI
           << ';';
  for (const auto &feature : target_options.Features) {
    language << feature << ';';
  }
  llvm::raw_string_ostream library(contract.standard_library);
  for (const auto *name :
       {"_LIBCPP_VERSION", "_LIBCPP_ABI_VERSION", "_LIBCPP_ABI_UNSTABLE",
        "_LIBCPP_ABI_NAMESPACE", "_LIBCPP_HARDENING_MODE", "__GLIBCXX__",
        "_GLIBCXX_USE_CXX11_ABI", "_GLIBCXX_USE_DUAL_ABI", "_GLIBCXX_DEBUG",
        "_GLIBCXX_ASSERTIONS", "_GLIBCXX_SANITIZE_VECTOR",
        "_ITERATOR_DEBUG_LEVEL"}) {
    library << name << '=';
    if (const auto *macro =
            preprocessor.getMacroInfo(preprocessor.getIdentifierInfo(name))) {
      for (const auto &token : macro->tokens()) {
        library << preprocessor.getSpelling(token) << ' ';
      }
    } else {
      library << "<undefined>";
    }
    library << ';';
  }
  const std::set<std::string> wanted(state_types.begin(), state_types.end());
  if (wanted.size() != state_types.size()) {
    return invalid("duplicate requested state type");
  }
  Layouts layouts(context);
  if (!type_aliases.empty()) {
    if (type_aliases.size() != state_types.size()) {
      return invalid("requested type alias count does not match state roots");
    }
    ContractTypeAliases aliases(type_aliases);
    aliases.TraverseDecl(context.getTranslationUnitDecl());
    for (const auto &alias_name : type_aliases) {
      const auto first = std::find_if(
          aliases.found().begin(), aliases.found().end(),
          [&](const auto &alias) { return alias.first == alias_name; });
      if (first == aliases.found().end() ||
          std::find_if(std::next(first), aliases.found().end(),
                       [&](const auto &alias) {
                         return alias.first == alias_name;
                       }) != aliases.found().end()) {
        return invalid("missing or ambiguous compiler-owned type alias " +
                       alias_name);
      }
      if (auto error = layouts.add(first->second, 0, false, true)) {
        return std::move(error);
      }
    }
  } else {
    std::vector<const clang::CXXRecordDecl *> found;
    find_records(*context.getTranslationUnitDecl(), context, wanted, found);
    std::set<std::string> matched;
    for (const auto *record : found) {
      matched.insert(record->getQualifiedNameAsString());
      matched.insert(canonical_record_type_name(context, record));
    }
    for (const auto &type : wanted) {
      if (!::tracer::compat::contains(matched, type) && type.find('<') != std::string::npos) {
        return invalid("missing or ambiguous state declaration " + type);
      }
    }
    if (found.size() != wanted.size() || found.size() > 1024) {
      return invalid("missing or ambiguous state declaration");
    }
    for (const auto *record : found) {
#if CLANG_VERSION_MAJOR >= 22
      const auto type = context.getCanonicalTagType(record);
#else
      const auto type = clang::QualType(record->getTypeForDecl(), 0);
#endif
      if (auto error = layouts.add(type)) {
        return error;
      }
    }
  }
  contract.types = layouts.take();
  Bridges bridges(context, contract, owned_input_marker);
  bridges.TraverseDecl(context.getTranslationUnitDecl());
  if (!bridges.error_.empty()) {
    return invalid(bridges.error_);
  }
  StandardDefinitions standard(context, contract.standard_definitions);
  standard.TraverseDecl(context.getTranslationUnitDecl());
  std::sort(contract.standard_definitions.begin(),
            contract.standard_definitions.end());
  return contract;
}
} // namespace tracer

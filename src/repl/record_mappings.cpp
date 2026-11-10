#include "record_mappings.hpp"
#include "../record_factory.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Sema/Sema.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/raw_ostream.h>

namespace tracer_repl {
namespace {
using Record = clang::CXXRecordDecl;
using DestructorGroups = std::set<std::vector<std::string>>;

struct MappedField {
  const clang::FieldDecl* field;
  std::string owner;
  std::string wire_name;
};

struct ConstructorArgument {
  std::string wire_name;
  std::string value_type;
};

struct RecordMapping {
  Record* record;
  std::vector<MappedField> fields;
  std::vector<ConstructorArgument> constructor_arguments;

  bool returns_prvalue() const {
    return !constructor_arguments.empty() && fields.empty();
  }
};

std::string member_pointer(const MappedField& mapped) {
  return "&::" + mapped.owner + "::" + mapped.field->getNameAsString();
}

std::string field_reference(const std::string& type, const MappedField& mapped) {
  return "decltype(std::declval<" + type + "&>().*(" + member_pointer(mapped) + "))";
}

std::string selector(std::string name) {
  name.erase(
      std::remove_if(name.begin(), name.end(), [](unsigned char c) { return std::isspace(c); }),
      name.end());
  if (name.rfind("::", 0) == 0) {
    name.erase(0, 2);
  }
  return name;
}

bool reserved(const std::string& name) {
  for (const auto* space :
       {"std", "__gnu_cxx", "__cxxabiv1", "tracer_repl", "clang", "llvm", "google", "grpc"}) {
    if (name == space || name.rfind(std::string(space) + "::", 0) == 0) {
      return true;
    }
  }
  return false;
}

bool identifier(const std::string& name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
  });
}

class Generator {
  clang::CompilerInstance& ci_;
  clang::ASTContext& ast_;
  clang::Sema& sema_;
  clang::PrintingPolicy policy_;
  std::map<std::string, Record*> candidates_;
  std::map<std::string, Record*> aliases_;
  std::set<Record*> defaults_, manual_, instantiated_;
  std::map<Record*, unsigned> state_;
  std::map<Record*, std::string> failures_;
  std::vector<RecordMapping> ordered_;
  const std::function<bool(const std::string&)>& viable_;
  const std::set<std::string>* external_destructors_;
  DestructorGroups needed_destructors_;
  std::set<Record*> waiting_for_destructors_;

  Record* key(Record* record) { return record->getCanonicalDecl(); }

  clang::QualType record_type(const Record* record) {
#if LLVM_VERSION_MAJOR >= 22
    return ast_.getCanonicalTagType(record);
#else
    return ast_.getRecordType(record);
#endif
  }

  std::string name(Record* record) {
    return record_type(record).getCanonicalType().getAsString(policy_);
  }

  bool header_decl(const clang::NamedDecl* decl) {
    const auto loc = ci_.getSourceManager().getSpellingLoc(decl->getLocation());
    return loc.isValid() &&
           ci_.getSourceManager().getFileEntryForID(ci_.getSourceManager().getFileID(loc));
  }

  void note(Record* record) {
    if (record->isDependentType()) {
      return;
    }
    if (auto* special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      const auto primary = special->getSpecializedTemplate()->getQualifiedNameAsString();
      if ((primary == "tracer_repl::EventRecord" || primary == "tracer_repl::EventCodec") &&
          special->getTemplateArgs().size() &&
          special->getTemplateArgs()[0].getKind() == clang::TemplateArgument::Type) {
        if (auto* mapped = special->getTemplateArgs()[0]
                               .getAsType()
                               .getCanonicalType()
                               ->getAsCXXRecordDecl()) {
          if (special->getSpecializationKind() == clang::TSK_ExplicitSpecialization) {
            manual_.insert(key(mapped));
          } else if (special->isCompleteDefinition()) {
            instantiated_.insert(key(mapped));
          }
        }
      }
    }
    const auto spelling = name(record);
    if (reserved(spelling) || !header_decl(record)) {
      return;
    }
    candidates_.emplace(selector(spelling), record);
    const auto loc = ci_.getSourceManager().getSpellingLoc(record->getLocation());
    if (!ci_.getSourceManager().isInSystemHeader(loc)) {
      defaults_.insert(key(record));
    }
  }

  void scan(clang::DeclContext* context) {
    for (auto* decl : context->decls()) {
      if (auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(decl)) {
        scan(linkage);
      } else if (auto* exported = llvm::dyn_cast<clang::ExportDecl>(decl)) {
        scan(exported);
      } else if (auto* space = llvm::dyn_cast<clang::NamespaceDecl>(decl)) {
        const auto spelling = space->getQualifiedNameAsString();
        if (!reserved(spelling) || spelling == "tracer_repl") {
          scan(space);
        }
      } else if (auto* record = llvm::dyn_cast<Record>(decl)) {
        note(record);
        if (record->isCompleteDefinition() && !record->isDependentType() &&
            !reserved(name(record))) {
          scan(record);
        }
      } else if (auto* templ = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
        const auto spelling = templ->getQualifiedNameAsString();
        if (!reserved(spelling) || spelling == "tracer_repl::EventRecord" ||
            spelling == "tracer_repl::EventCodec") {
          for (auto* special : templ->specializations()) {
            note(special);
          }
        }
      } else if (auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(decl)) {
        if (!header_decl(alias) || reserved(alias->getQualifiedNameAsString())) {
          continue;
        }
        if (auto* record = alias->getUnderlyingType().getCanonicalType()->getAsCXXRecordDecl()) {
          if (!record->isDependentType() && !reserved(name(record))) {
            aliases_[selector(alias->getQualifiedNameAsString())] = record;
            note(record);
          }
        }
      }
    }
  }

  bool failure(Record* record, const std::string& reason) {
    failures_[key(record)] = reason;
    state_[key(record)] = 3;
    return false;
  }

  bool supported(clang::QualType type, std::string& reason, unsigned depth,
                 std::string& waiting_for_field) {
    if (depth > 32) {
      reason = "type nesting exceeds 32";
      return false;
    }
    type = type.getCanonicalType();
    if (type.isVolatileQualified() || type->isReferenceType() || type->isPointerType()) {
      reason = "pointer/reference/volatile fields require a custom codec";
      return false;
    }
    type = type.getUnqualifiedType();
    if (type->isEnumeralType() || type->isArithmeticType()) {
      if (ast_.getTypeSize(type) <= 64) {
        return true;
      }
      reason = "numeric fields wider than 64 bits are unsupported";
      return false;
    }
    auto* record = type->getAsCXXRecordDecl();
    if (!record) {
      reason = "field type has no automatic codec";
      return false;
    }
    if (manual_.count(key(record))) {
      return true;
    }
    if (auto* special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      const auto primary = special->getSpecializedTemplate()->getQualifiedNameAsString();
      const auto simple = special->getSpecializedTemplate()->getNameAsString();
      if (primary.rfind("std::", 0) == 0) {
        const auto& args = special->getTemplateArgs();
        if (simple == "basic_string") {
          bool standard = args.size() >= 3 && args[0].getKind() == clang::TemplateArgument::Type &&
                          args[0].getAsType()->isCharType();
          for (unsigned i = 1; standard && i < 3; ++i) {
            const auto* parameter =
                args[i].getKind() == clang::TemplateArgument::Type
                    ? llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                          args[i].getAsType()->getAsCXXRecordDecl())
                    : nullptr;
            standard = parameter &&
                       parameter->getSpecializedTemplate()->getQualifiedNameAsString().rfind(
                           "std::", 0) == 0 &&
                       parameter->getSpecializedTemplate()->getNameAsString() ==
                           (i == 1 ? "char_traits" : "allocator");
          }
          if (standard) {
            return true;
          }
          reason = "only narrow std::string has an automatic string codec";
          return false;
        }
        const bool map = simple == "map" || simple == "unordered_map";
        if (map || simple == "vector" || simple == "array" || simple == "list" ||
            simple == "deque" || simple == "set" || simple == "unordered_set" ||
            simple == "optional") {
          for (unsigned i = 0; i < (map ? 2U : 1U); ++i) {
            if (args.size() <= i || args[i].getKind() != clang::TemplateArgument::Type ||
                !supported(args[i].getAsType(), reason, depth + 1, waiting_for_field)) {
              return false;
            }
          }
          return true;
        }
        reason = "standard type has no built-in EventCodec";
        return false;
      }
    }
    if (!plan(record, depth + 1)) {
      auto* canonical = key(record);
      if (!waiting_for_destructors_.count(canonical)) {
        reason = failures_[canonical];
        return false;
      }
      if (waiting_for_field.empty()) {
        waiting_for_field = failures_[canonical];
      }
    }
    return true;
  }

  bool collect_fields(Record* record, bool virtual_base, std::map<Record*, bool>& seen,
                      std::vector<MappedField>& fields, std::string& reason) {
    record = record ? record->getDefinition() : nullptr;
    if (!record || !record->getIdentifier()) {
      reason = "incomplete or unnamed record in base hierarchy";
      return false;
    }
    const auto [previous, inserted] = seen.emplace(record, virtual_base);
    if (!inserted) {
      if (virtual_base && previous->second) {
        return true;
      }
      reason = "repeated non-virtual snapshot base: " + name(record);
      return false;
    }
    for (auto* context = record->getDeclContext(); context; context = context->getParent()) {
      if (auto* space = llvm::dyn_cast<clang::NamespaceDecl>(context);
          space && space->isAnonymousNamespace()) {
        reason = "anonymous namespace type in base hierarchy";
        return false;
      }
      if (llvm::isa<clang::FunctionDecl>(context)) {
        reason = "local class in base hierarchy";
        return false;
      }
    }
    for (const auto* method : record->methods()) {
      const auto is_destructor = llvm::isa<clang::CXXDestructorDecl>(method);
      if (!method->isVirtual() || method->isImplicit() || method->isDeleted() ||
          (method->isPureVirtual() && !is_destructor) ||
          (external_destructors_ && is_destructor)) {
        continue;
      }
      if (!method->getDefinition()) {
        reason = "virtual method definition is unavailable for local decoding: " +
                 method->getQualifiedNameAsString();
        return false;
      }
    }
    for (const auto& base : record->bases()) {
      auto* base_record = base.getType()->getAsCXXRecordDecl();
      if (const auto* specialization = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(base_record)) {
        const auto primary = specialization->getSpecializedTemplate()->getQualifiedNameAsString();
        if (primary == "tracer::state" || primary == "tracer::shared_state") {
          continue;
        }
      }
      if (!collect_fields(base_record, base.isVirtual(), seen, fields, reason)) {
        return false;
      }
    }
    const auto owner = name(record);
    for (const auto* field : record->fields()) {
      if (field->isUnnamedBitField()) {
        continue;
      }
      if (fields.size() == 128) {
        reason = "more than 128 fields";
        return false;
      }
      fields.push_back({field, owner, field->getNameAsString()});
    }
    return true;
  }

  bool default_constructor_available(Record* record, std::set<Record*>& seen,
                                     std::string& reason) {
    record = record->getDefinition();
    if (!seen.insert(record).second) {
      return true;
    }
    for (const auto* ctor : record->ctors()) {
      if (!ctor->isDefaultConstructor() || ctor->isImplicit()) {
        continue;
      }
      if (!ctor->getDefinition()) {
        reason = "default constructor definition is unavailable for local decoding: " +
                 ctor->getQualifiedNameAsString();
        return false;
      }
      if (!ctor->isDefaulted()) {
        // A user-defined constructor may explicitly select other base
        // constructors. Let its definition govern those calls.
        return true;
      }
    }
    for (const auto& base : record->bases()) {
      if (!default_constructor_available(base.getType()->getAsCXXRecordDecl(), seen, reason)) {
        return false;
      }
    }
    return true;
  }

  std::vector<std::string> destructor_symbols(const clang::CXXDestructorDecl* destructor) {
    if (!destructor->hasExternalFormalLinkage() || destructor->isInlined()) {
      return {};
    }
    auto mangler = std::unique_ptr<clang::MangleContext>(ast_.createMangleContext());
    auto* itanium = llvm::dyn_cast<clang::ItaniumMangleContext>(mangler.get());
    if (!itanium) {
      return {};
    }
    std::vector<std::string> symbols;
    const auto append_symbol = [&](auto write) {
      llvm::raw_string_ostream out(symbols.emplace_back());
      write(out);
    };
    const auto add_destructor = [&](clang::CXXDtorType variant) {
      append_symbol([&](auto& out) {
        itanium->mangleName(clang::GlobalDecl(destructor, variant), out);
      });
    };
    add_destructor(clang::Dtor_Complete);
    add_destructor(clang::Dtor_Base);
    const auto* owner = destructor->getParent();
    const auto* key = owner->isPolymorphic() ? ast_.getCurrentKeyFunction(owner) : nullptr;
    if (key && key->getCanonicalDecl() == destructor->getCanonicalDecl()) {
      // The translation unit defining the key function owns the vtable and RTTI.
      append_symbol([&](auto& out) { itanium->mangleCXXVTable(owner, out); });
      if (owner->getNumVBases()) {
        append_symbol([&](auto& out) { itanium->mangleCXXVTT(owner, out); });
      }
      if (ast_.getLangOpts().RTTI) {
        append_symbol([&](auto& out) {
          itanium->mangleCXXRTTI(record_type(owner).getCanonicalType(), out);
        });
      }
    } else if (destructor->isVirtual() && !destructor->isPureVirtual()) {
      // A vtable emitted by the interpreter can also reference the deleting variant.
      add_destructor(clang::Dtor_Deleting);
    }
    return symbols;
  }

  const clang::CXXDestructorDecl* destructor_without_definition(Record* record) {
    const auto* destructor = record->getDestructor();
    return destructor && !destructor->isImplicit() && !destructor->getDefinition()
               ? destructor
               : nullptr;
  }

  bool lifetime_available(Record* record, const std::map<Record*, bool>& bases,
                          bool default_construct, bool needs_move, std::string& reason,
                          DestructorGroups& needed, std::string& missing_destructor) {
    for (const auto& base : bases) {
      if (const auto* destructor = destructor_without_definition(base.first)) {
        auto symbols = external_destructors_ ? destructor_symbols(destructor)
                                             : std::vector<std::string>{};
        if (symbols.empty()) {
          reason = "destructor definition is unavailable for local decoding: " +
                   destructor->getQualifiedNameAsString();
          return false;
        }
        if (!std::all_of(symbols.begin(), symbols.end(), [&](const auto& symbol) {
              return external_destructors_->count(symbol) != 0;
            })) {
          if (missing_destructor.empty()) {
            missing_destructor = destructor->getQualifiedNameAsString();
          }
          needed.insert(std::move(symbols));
        }
      }
    }
    if (default_construct) {
      std::set<Record*> visited;
      if (!default_constructor_available(record, visited, reason)) {
        return false;
      }
    }
    if (!needs_move) {
      return true;
    }
    const auto ctors = record->ctors();
    const bool has_move = std::any_of(ctors.begin(), ctors.end(), [](const auto* ctor) {
      return ctor->isMoveConstructor() && !ctor->isDeleted();
    });
    for (const auto* ctor : ctors) {
      unsigned qualifiers = 0;
      const bool copy_fallback = !has_move && ctor->isCopyConstructor(qualifiers) &&
                                 qualifiers == clang::Qualifiers::Const;
      if ((ctor->isMoveConstructor() || copy_fallback) && !ctor->isDeleted() &&
          !ctor->isImplicit() && !ctor->getDefinition()) {
        reason = "copy/move constructor definition is unavailable for local decoding: " +
                 ctor->getQualifiedNameAsString();
        return false;
      }
    }
    return true;
  }

  bool can_decode(const std::string& type, const RecordMapping& mapping) {
    std::string trait;
    if (mapping.constructor_arguments.empty()) {
      trait = "std::is_default_constructible_v<" + type + ">";
    } else {
      trait = "std::is_constructible_v<" + type;
      for (const auto& arg : mapping.constructor_arguments) {
        trait += ", " + arg.value_type;
      }
      trait += '>';
    }
    trait += " && std::is_destructible_v<" + type + ">";
    if (!mapping.returns_prvalue()) {
      trait += " && std::is_move_constructible_v<" + type + ">";
    }
    for (const auto& mapped : mapping.fields) {
      if (mapped.field->getType().isConstQualified()) {
        return false;
      }
      const auto reference = field_reference(type, mapped);
      trait += " && std::is_assignable_v<" + reference + ", std::remove_reference_t<" +
               reference + ">>";
    }
    return viable_(trait);
  }

  bool plan_constructor(RecordMapping& mapping, std::string& reason) {
    const auto recipe = tracer::record_factory::infer(*mapping.record);
    if (!recipe) {
      if (reason.empty()) {
        reason = "no usable default decoding or inferable field constructor; provide EventCodec factory";
      }
      return false;
    }
    auto type_policy = policy_;
    type_policy.FullyQualifiedName = true;
    std::set<const clang::FieldDecl*> initialized;
    for (unsigned i = 0; i < recipe->fields_by_parameter.size(); ++i) {
      const auto* field = recipe->fields_by_parameter[i];
      const auto mapped = std::find_if(mapping.fields.begin(), mapping.fields.end(),
                                       [field](const auto& entry) { return entry.field == field; });
      if (mapped == mapping.fields.end() || !initialized.insert(field).second) {
        reason = "constructor parameter needs an unambiguous captured field";
        return false;
      }
      const auto value_type = recipe->constructor->getParamDecl(i)->getType()
                                  .getCanonicalType().getNonReferenceType().getUnqualifiedType();
      mapping.constructor_arguments.push_back({mapped->wire_name, value_type.getAsString(type_policy)});
    }
    mapping.fields.erase(std::remove_if(mapping.fields.begin(), mapping.fields.end(),
                                       [&](const auto& entry) { return initialized.count(entry.field); }),
                         mapping.fields.end());
    return true;
  }

  bool plan(Record* record, unsigned depth = 0) {
    auto* canonical = key(record);
    if (manual_.count(canonical)) {
      return true;
    }
    if (state_[canonical]) {
      return state_[canonical] != 3;
    }
    if (state_.size() > 128) {
      throw std::runtime_error("more than 128 records; select --type explicitly");
    }
    state_[canonical] = 1;
    if (instantiated_.count(canonical)) {
      return failure(record, "EventCodec was already instantiated; import "
                             "before decoding this type");
    }
    if (depth > 32 || !sema_.isCompleteType(record->getLocation(), record_type(record))) {
      return failure(record, "incomplete or excessively nested type");
    }
    record = record->getDefinition();
    if (!record || !record->getIdentifier()) {
      return failure(canonical, "unnamed record");
    }
    if (record->isUnion()) {
      return failure(record, "union snapshots require an active member discriminator");
    }
    if (record->isAbstract()) {
      return failure(record, "an abstract class cannot be constructed as a local value");
    }
    RecordMapping mapping{record, {}, {}};
    std::map<Record*, bool> seen;
    std::string reason;
    DestructorGroups needed;
    std::string missing_destructor;
    std::string waiting_for_field;
    if (!collect_fields(record, false, seen, mapping.fields, reason)) {
      return failure(record, reason);
    }
    const auto type_name = "::" + name(record);
    std::map<std::string, unsigned> field_counts;
    for (const auto& mapped : mapping.fields) {
      const auto* field = mapped.field;
      if (field->isBitField() || !identifier(field->getNameAsString())) {
        return failure(record, "bit-fields and unnamed/non-ASCII fields are unsupported");
      }
      if (!supported(field->getType(), reason, depth + 1, waiting_for_field)) {
        return failure(record, field->getNameAsString() + ": " + reason);
      }
      ++field_counts[mapped.wire_name];
    }
    for (auto& mapped : mapping.fields) {
      if (field_counts[mapped.wire_name] > 1) {
        mapped.wire_name = mapped.owner + "::" + mapped.wire_name;
      }
    }
    const bool check_lifetime = record->getNumBases() || record->isPolymorphic() ||
                                (external_destructors_ && destructor_without_definition(record));
    if (!can_decode(type_name, mapping) ||
        (check_lifetime &&
         !lifetime_available(record, seen, true, true, reason, needed, missing_destructor))) {
      if (!plan_constructor(mapping, reason)) {
        return failure(record, reason);
      }
      if (!can_decode(type_name, mapping)) {
        return failure(record, "inferred constructor and remaining assignable fields are not usable; "
                               "provide EventCodec factory");
      }
      needed.clear();
      missing_destructor.clear();
      if (!lifetime_available(record, seen, false, !mapping.returns_prvalue(), reason,
                              needed, missing_destructor)) {
        return failure(record, reason);
      }
    }
    if (!needed.empty()) {
      needed_destructors_.merge(needed);
      waiting_for_destructors_.insert(canonical);
      return failure(record, "destructor definition is unavailable for local decoding: " +
                                 missing_destructor);
    }
    if (!waiting_for_field.empty()) {
      waiting_for_destructors_.insert(canonical);
      return failure(record, waiting_for_field);
    }
    state_[canonical] = 2;
    ordered_.push_back(std::move(mapping));
    return true;
  }

public:
  Generator(clang::CompilerInstance& ci, const std::function<bool(const std::string&)>& viable,
            const std::set<std::string>* external_destructors)
      : ci_(ci)
      , ast_(ci.getASTContext())
      , sema_(ci.getSema())
      , policy_(ci.getLangOpts())
      , viable_(viable)
      , external_destructors_(external_destructors) {
    policy_.SuppressTagKeyword = true;
  }

  GeneratedRecordMappings run(const std::vector<std::string>& types) {
    for (auto* unit : ast_.getTranslationUnitDecl()->redecls()) {
      scan(unit);
    }
    if (types.empty()) {
      for (const auto& [spelling, record] : candidates_) {
        if (defaults_.count(key(record))) {
          plan(record);
        }
      }
    } else {
      for (const auto& requested : types) {
        const auto spelling = selector(requested);
        auto found = candidates_.find(spelling);
        Record* record = found == candidates_.end() ? nullptr : found->second;
        if (!record) {
          auto alias = aliases_.find(spelling);
          if (alias != aliases_.end()) {
            record = alias->second;
          }
        }
        if (!record) {
          throw std::runtime_error("no application record found: " + requested);
        }
        if (!plan(record) && !waiting_for_destructors_.count(key(record))) {
          throw std::runtime_error("cannot map " + requested + ": " + failures_[key(record)]);
        }
      }
    }
    std::ostringstream code;
    GeneratedRecordMappings generated;
    code << "// Generated EventRecord mappings:";
    const auto single_line = [](std::string text) {
      std::replace(text.begin(), text.end(), '\n', ' ');
      std::replace(text.begin(), text.end(), '\r', ' ');
      return text;
    };
    for (const auto& mapping : ordered_) {
      code << " " << single_line(name(mapping.record));
    }
    for (const auto& [record, reason] : failures_) {
      code << "; skipped " << single_line(name(record)) << ": " << single_line(reason);
    }
    if (ordered_.empty() && failures_.empty()) {
      code << " none needed (built-in or existing codecs)";
    }
    code << '\n';
    for (const auto& group : needed_destructors_) {
      code << "// tracer-repl-needed-destructor";
      for (const auto& symbol : group) {
        code << ' ' << std::quoted(symbol);
      }
      code << '\n';
    }
    for (const auto& mapping : ordered_) {
      const auto type = name(mapping.record);
      generated.types.push_back(type);
      if (!mapping.constructor_arguments.empty()) {
        code << "template<> struct tracer_repl::EventCodec<::" << type << "> {\n"
             << "static ::" << type << " decode(const EventValue& value) {\n"
             << "if (!value.matches_record_type(" << std::quoted(type) << ")) "
             << "throw std::invalid_argument(\"event record type mismatch: \" + value.cpp_type());\n";
        if (mapping.returns_prvalue()) {
          code << "return ::" << type << '(';
        } else {
          code << "::" << type << " result(";
        }
        bool first = true;
        for (const auto& arg : mapping.constructor_arguments) {
          if (!first) {
            code << ',';
          }
          first = false;
          code << "value.field(" << std::quoted(arg.wire_name) << ").as<" << arg.value_type << ">()";
        }
        code << ");\n";
        for (const auto& mapped : mapping.fields) {
          const auto member = "result.*(" + member_pointer(mapped) + ")";
          code << member << " = value.field(" << std::quoted(mapped.wire_name)
               << ").as<std::remove_reference_t<decltype(" << member << ")>>();\n";
        }
        if (!mapping.returns_prvalue()) {
          code << "return result;\n";
        }
        code << "}};\n";
        continue;
      }
      code << "template<> struct tracer_repl::EventRecord<::" << type << "> {\n"
           << "static constexpr const char* type_name=" << std::quoted(type) << ";\n"
           << "static auto fields(){return std::make_tuple(";
      bool first = true;
      for (const auto& mapped : mapping.fields) {
        if (!first) {
          code << ',';
        }
        first = false;
        code << "tracer_repl::event_member(" << std::quoted(mapped.wire_name) << ", "
             << member_pointer(mapped) << ')';
      }
      code << ");}};\n";
    }
    if (code.str().size() > 60000) {
      throw std::runtime_error("mapping output exceeds 60 KiB; select --type explicitly");
    }
    generated.source = code.str();
    return generated;
  }
};
} // namespace

GeneratedRecordMappings
generate_record_mappings(clang::CompilerInstance& compiler, const std::vector<std::string>& types,
                         const std::function<bool(const std::string&)>& viable,
                         const std::set<std::string>* external_destructors) {
  return Generator(compiler, viable, external_destructors).run(types);
}
} // namespace tracer_repl

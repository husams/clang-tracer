#include "record_mappings.hpp"
#include "../record_factory.hpp"
#include "../snapshot_members.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecordLayout.h>
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
  std::uint64_t offset = 0; // Byte offset in the complete record.
  std::string nested = {};  // Reconstructed record type rebuilt in place.
  Record* nested_record = nullptr;
  Record* adopt_pointee = nullptr; // Pointee of a unique_ptr that only views a copy.
};

struct ConstructorArgument {
  std::string wire_name;
  std::string value_type;
};

// A raw pointer member of a reconstructed record and the pointee layouts, in
// the order they are tried, that may decode its `<field>_snapshot`.
struct Followed {
  MappedField field;
  std::vector<Record*> pointees;
  Record* declared = nullptr; // Set for a reference member, which must be captured.
};

struct RecordMapping {
  Record* record = nullptr;
  std::vector<MappedField> fields;
  std::vector<ConstructorArgument> constructor_arguments;
  bool reconstructed = false;
  std::vector<Followed> followed;
  std::vector<MappedField> adopted; // unique_ptr members of reconstructed pointees.
  std::vector<MappedField> texts;   // char pointer members holding a captured string.
  std::vector<MappedField> scalars; // Pointers to one captured arithmetic value.
  // Records whose codecs this one names (field types, element types, smart
  // pointer pointees); the mapping is dropped if any of them is.
  std::set<Record*> depends;

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
  std::set<Record*> defaults_, manual_, manual_reconstructed_, instantiated_, reconstructing_;
  // Set when a by-value field depends on a record that is still being planned.
  bool value_in_progress_ = false;
  std::set<std::string> notes_;
  // Receives the records the mapping being planned names in its field types.
  std::set<Record*>* collecting_ = nullptr;
  struct Collect {
    Generator& generator;
    std::set<Record*>* saved;
    Collect(Generator& owner, std::set<Record*>* target)
        : generator(owner), saved(owner.collecting_) {
      owner.collecting_ = target;
    }
    ~Collect() { generator.collecting_ = saved; }
  };
  std::map<Record*, unsigned> state_;
  std::map<Record*, std::string> failures_;
  std::vector<RecordMapping> ordered_;
  const std::function<bool(const std::string&)>& viable_;
  const std::set<std::string>* external_destructors_;
  // The last pass: a destructor that is still unbound will stay unbound.
  bool destructors_final_;
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
      if ((primary == "tracer_repl::EventRecord" || primary == "tracer_repl::EventCodec" ||
           primary == "tracer_repl::ReconstructedRecord") &&
          special->getTemplateArgs().size() &&
          special->getTemplateArgs()[0].getKind() == clang::TemplateArgument::Type) {
        if (auto* mapped = special->getTemplateArgs()[0]
                               .getAsType()
                               .getCanonicalType()
                               ->getAsCXXRecordDecl()) {
          if (special->getSpecializationKind() == clang::TSK_ExplicitSpecialization) {
            manual_.insert(key(mapped));
            if (primary == "tracer_repl::ReconstructedRecord") {
              manual_reconstructed_.insert(key(mapped));
            }
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
            spelling == "tracer_repl::EventCodec" ||
            spelling == "tracer_repl::ReconstructedRecord") {
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
    reconstructing_.erase(key(record));
    return false;
  }

  static constexpr const char* reconstructed_only =
      "nested record is only available as a reconstructed value";

  bool supported(clang::QualType type, std::string& reason, unsigned depth,
                 std::string& waiting_for_field, bool allow_reconstructed = false) {
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
    if (manual_reconstructed_.count(key(record))) {
      reason = "nested record has a user-defined reconstruction";
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
        if (simple == "unique_ptr" || simple == "shared_ptr" || simple == "weak_ptr") {
          if (simple == "weak_ptr") {
            return true;
          }
          const bool unique = simple == "unique_ptr";
          const auto element = args.size() && args[0].getKind() == clang::TemplateArgument::Type
                                   ? args[0].getAsType()
                                   : clang::QualType();
          bool standard = !element.isNull() && !element->isArrayType() && !element->isVoidType() &&
                          !element.isVolatileQualified();
          if (standard && unique) {
            const auto* deleter =
                args.size() == 2 && args[1].getKind() == clang::TemplateArgument::Type
                    ? llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                          args[1].getAsType()->getAsCXXRecordDecl())
                    : nullptr;
            standard = deleter &&
                       deleter->getSpecializedTemplate()->getQualifiedNameAsString() ==
                           "std::default_delete" &&
                       deleter->getTemplateArgs().size() == 1 &&
                       deleter->getTemplateArgs()[0].getAsType().getCanonicalType() ==
                           element.getCanonicalType();
          } else if (standard) {
            standard = args.size() == 1;
          }
          if (!standard) {
            reason = "only std::unique_ptr with the default deleter and std::shared_ptr to an "
                     "object have an automatic codec";
            return false;
          }
          if (unique) {
            // The pointee must not turn into a reconstructed record after this decision.
            if (auto* pointee = element->getAsCXXRecordDecl()) {
              const auto found = state_.find(key(pointee));
              if (found != state_.end() && found->second == 1 &&
                  !reconstructing_.count(key(pointee))) {
                reason = "cyclic unique_ptr requires an EventCodec";
                return false;
              }
            }
          }
          return supported(element, reason, depth + 1, waiting_for_field, !unique);
        }
        reason = "standard type has no built-in EventCodec";
        return false;
      }
    }
    if (reconstructing_.count(key(record)) && !allow_reconstructed) {
      reason = reconstructed_only;
      return false;
    }
    if (collecting_) {
      collecting_->insert(key(record));
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
    if (state_[key(record)] == 1) {
      value_in_progress_ = true;
    }
    if (reconstructing_.count(key(record)) && !allow_reconstructed) {
      reason = reconstructed_only;
      return false;
    }
    return true;
  }

  // A raw pointer or a reference to a record: the address cannot be a local value.
  static bool raw_record_pointer(const clang::FieldDecl* field) {
    const auto type = field->getType().getCanonicalType();
    return (type->isPointerType() || type->isReferenceType()) &&
           type->getPointeeType()->getAsCXXRecordDecl();
  }

  // The pointee itself, then prepared records derived from it through one
  // public path. Records still being planned are never offered, so a cycle
  // leaves the inner pointer null.
  std::vector<Record*> followed_pointees(const clang::FieldDecl* field, unsigned depth) {
    std::vector<Record*> result;
    const auto type = field->getType().getCanonicalType();
    if (type.isVolatileQualified() || type->getPointeeType().isVolatileQualified()) {
      return result;
    }
    auto* declared = type->getPointeeType()->getAsCXXRecordDecl();
    declared = declared ? declared->getDefinition() : nullptr;
    // Only application records are planned from here, never system headers.
    if (!declared || declared->isDependentType() || reserved(name(declared)) ||
        !(defaults_.count(key(declared)) || manual_.count(key(declared)))) {
      return result;
    }
    std::vector<Record*> tried{declared};
    // At most this many derived records are tried per pointer (the declared
    // class does not count). TRACER_REPL_DERIVED_CANDIDATES changes it.
    std::size_t derived_limit = 15;
    if (const char* setting = std::getenv("TRACER_REPL_DERIVED_CANDIDATES"); setting && *setting) {
      char* end = nullptr;
      const auto value = std::strtoull(setting, &end, 10);
      if (*end != '\0') {
        throw std::runtime_error("TRACER_REPL_DERIVED_CANDIDATES must be a number");
      }
      derived_limit = static_cast<std::size_t>(value);
    }
    for (const auto& entry : candidates_) {
      auto* derived = entry.second->getDefinition();
      if (tried.size() == derived_limit + 1) {
        for (const auto& rest : candidates_) {
          auto* more = rest.second->getDefinition();
          clang::CXXBasePaths other;
          if (more && more != declared && defaults_.count(key(more)) &&
              std::find(tried.begin(), tried.end(), more) == tried.end() &&
              more->isDerivedFrom(declared, other)) {
            notes_.insert("derived records of " + name(declared) + " beyond " +
                          std::to_string(derived_limit) +
                          " are not tried (TRACER_REPL_DERIVED_CANDIDATES)");
            break;
          }
        }
        break;
      }
      if (!derived || derived == declared || derived->isDependentType() ||
          !defaults_.count(key(derived)) ||
          std::find(tried.begin(), tried.end(), derived) != tried.end()) {
        continue;
      }
      clang::CXXBasePaths paths;
      if (!derived->isDerivedFrom(declared, paths) ||
          paths.isAmbiguous(ast_.getCanonicalType(record_type(declared))) ||
          std::none_of(paths.begin(), paths.end(),
                       [](const auto& path) { return path.Access == clang::AS_public; })) {
        continue;
      }
      tried.push_back(derived);
    }
    for (auto* pointee : tried) {
      if (pointee->isUnion() || pointee->isAbstract() || !plan(pointee, depth + 1)) {
        continue;
      }
      // A record still being planned is offered too (a linked structure refers
      // back to its own type); a final pass drops any that fail.
      const auto state = state_[key(pointee)];
      if (manual_.count(key(pointee)) || state == 1 || state == 2 || state == 4) {
        result.push_back(pointee);
      }
    }
    return result;
  }

  bool collect_fields(Record* record, bool virtual_base, std::map<Record*, bool>& seen,
                      std::vector<MappedField>& fields, std::string& reason,
                      std::string& virtual_reason, std::uint64_t base_offset = 0) {
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
      if (!method->getDefinition() && virtual_reason.empty()) {
        virtual_reason = "virtual method definition is unavailable for local decoding: " +
                         method->getQualifiedNameAsString();
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
      // Virtual bases cannot be reconstructed, so their offset is never used.
      const auto offset =
          base.isVirtual()
              ? base_offset
              : base_offset + ast_.getASTRecordLayout(record)
                                  .getBaseClassOffset(base_record->getDefinition())
                                  .getQuantity();
      if (!collect_fields(base_record, base.isVirtual(), seen, fields, reason,
                         virtual_reason, offset)) {
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
      fields.push_back({field, owner, field->getNameAsString(),
                        base_offset + ast_.getASTRecordLayout(record).getFieldOffset(
                                          field->getFieldIndex()) / ast_.getCharWidth()});
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

  // The record a std::unique_ptr with the default deleter owns, if any.
  static Record* default_unique_pointee(const Record* record) {
    const auto* special = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
    if (!special || !special->isInStdNamespace() || special->getName() != "unique_ptr") {
      return nullptr;
    }
    const auto& args = special->getTemplateArgs();
    if (args.size() != 2 || args[0].getKind() != clang::TemplateArgument::Type ||
        args[1].getKind() != clang::TemplateArgument::Type) {
      return nullptr;
    }
    const auto* deleter = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
        args[1].getAsType()->getAsCXXRecordDecl());
    if (!deleter ||
        deleter->getSpecializedTemplate()->getQualifiedNameAsString() != "std::default_delete" ||
        deleter->getTemplateArgs().size() != 1 ||
        deleter->getTemplateArgs()[0].getAsType().getCanonicalType() !=
            args[0].getAsType().getCanonicalType()) {
      return nullptr;
    }
    auto* pointee = args[0].getAsType()->getAsCXXRecordDecl();
    return pointee ? pointee->getDefinition() : nullptr;
  }

  // Rebuilds `record` field by field without running any of its constructors
  // or destructors. `original` is why a normal mapping was impossible.
  bool reconstruct(Record* record, const std::string& original, unsigned depth) {
    auto* canonical = key(record);
    const auto fail = [&](const std::string& why) {
      return failure(record, original.empty() ? why : original + " (reconstruction: " + why + ")");
    };
    if (record->getNumVBases() != 0) {
      return fail("virtual bases cannot be reconstructed");
    }
    std::map<Record*, bool> seen;
    std::vector<MappedField> fields;
    std::string reason, ignored;
    if (!collect_fields(record, false, seen, fields, reason, ignored)) {
      return fail(reason);
    }
    reconstructing_.insert(canonical);
    RecordMapping mapping;
    mapping.record = record;
    mapping.reconstructed = true;
    const Collect collect(*this, &mapping.depends);
    const auto type_name = "::" + name(record);
    std::map<std::string, unsigned> counts;
    for (const auto& mapped : fields) {
      ++counts[mapped.wire_name];
    }
    std::string waiting_for_field;
    std::vector<MappedField> pointers;
    for (auto mapped : fields) {
      const auto* field = mapped.field;
      const auto field_name = field->getNameAsString();
      if (field->isBitField() || !identifier(field_name)) {
        return fail("bit-fields and unnamed/non-ASCII fields are unsupported");
      }
      if (counts[mapped.wire_name] > 1) {
        mapped.wire_name = mapped.owner + "::" + mapped.wire_name;
      }
      const auto type = field->getType().getCanonicalType();
      if (type.isVolatileQualified()) {
        return fail(field_name + ": pointer/reference/volatile fields require a custom codec");
      }
      if (type->isReferenceType()) {
        auto* declared = type->getPointeeType()->getAsCXXRecordDecl();
        declared = declared ? declared->getDefinition() : nullptr;
        if (!declared || type->getPointeeType().isVolatileQualified()) {
          return fail(field_name + ": a reference to a non-record cannot be reconstructed");
        }
        auto pointees = followed_pointees(field, depth);
        if (pointees.empty()) {
          return fail(field_name + ": the referenced record has no decodable layout");
        }
        mapping.followed.push_back({mapped, std::move(pointees), declared});
        continue;
      }
      if (type->isPointerType()) {
        // Only a pointer to a record, a C string or one scalar can be rebuilt
        // from the snapshot; any other pointer stays null locally.
        if (raw_record_pointer(field)) {
          pointers.push_back(mapped);
        } else if (!type->getPointeeType().isVolatileQualified()) {
          const auto kind = tracer::snapshot_members::classify(type);
          if (kind == tracer::snapshot_members::Referent::text) {
            mapping.texts.push_back(std::move(mapped));
          } else if (kind == tracer::snapshot_members::Referent::scalar &&
                     ast_.getTypeSize(type->getPointeeType()) <= 64) {
            mapping.scalars.push_back(std::move(mapped));
          }
        }
        continue;
      }
      auto* nested = type->getAsCXXRecordDecl();
      nested = nested ? nested->getDefinition() : nullptr;
      if (nested && !manual_.count(key(nested)) && !manual_reconstructed_.count(key(nested)) &&
          !nested->isInStdNamespace() && !reserved(name(nested))) {
        // A record that is itself reconstructed is rebuilt in place.
        if (plan(nested, depth + 1) && reconstructing_.count(key(nested)) &&
            state_[key(nested)] == 4) {
          mapped.nested = name(nested);
          mapped.nested_record = nested;
          mapping.fields.push_back(std::move(mapped));
          continue;
        }
      }
      if (auto* pointee = nested ? default_unique_pointee(nested) : nullptr) {
        if (!pointee->isInStdNamespace() && !manual_.count(key(pointee)) &&
            !manual_reconstructed_.count(key(pointee)) && plan(pointee, depth + 1) &&
            reconstructing_.count(key(pointee))) {
          // The pointee is held by this record, so the pointer only views it.
          mapped.adopt_pointee = pointee;
          mapping.adopted.push_back(std::move(mapped));
          continue;
        }
      }
      value_in_progress_ = false;
      if (!supported(field->getType(), reason, depth + 1, waiting_for_field)) {
        return fail(field_name + ": " + reason);
      }
      if (value_in_progress_) {
        return fail(field_name + ": cyclic value field requires an EventCodec");
      }
      mapping.fields.push_back(std::move(mapped));
    }
    std::string trait = "true";
    for (const auto& mapped : mapping.fields) {
      if (!mapped.nested.empty()) {
        continue;
      }
      const auto value = "std::remove_cv_t<std::remove_reference_t<" +
                         field_reference(type_name, mapped) + ">>";
      trait += " && std::is_move_constructible_v<" + value + "> && std::is_destructible_v<" +
               value + ">";
    }
    if (!viable_(trait)) {
      return fail("fields cannot be rebuilt locally; provide an EventCodec");
    }
    if (!waiting_for_field.empty()) {
      waiting_for_destructors_.insert(canonical);
      return failure(record, waiting_for_field);
    }
    for (auto& mapped : pointers) {
      auto pointees = followed_pointees(mapped.field, depth);
      if (!pointees.empty()) {
        mapping.followed.push_back({mapped, std::move(pointees)});
      }
    }
    state_[canonical] = 4;
    ordered_.push_back(std::move(mapping));
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
    RecordMapping mapping;
    mapping.record = record;
    const Collect collect(*this, &mapping.depends);
    std::map<Record*, bool> seen;
    std::string reason;
    DestructorGroups needed;
    std::string missing_destructor;
    std::string waiting_for_field;
    std::string virtual_reason;
    if (!collect_fields(record, false, seen, mapping.fields, reason, virtual_reason)) {
      return failure(record, reason);
    }
    // Records holding raw pointers or references can never be local values.
    const bool holds_addresses =
        std::any_of(mapping.fields.begin(), mapping.fields.end(), [](const auto& mapped) {
          const auto type = mapped.field->getType().getCanonicalType();
          return type->isPointerType() || raw_record_pointer(mapped.field);
        });
    if (holds_addresses) {
      return reconstruct(record, "", depth);
    }
    // Anything that only blocks construction is rebuilt field by field instead.
    if (!virtual_reason.empty()) {
      return reconstruct(record, virtual_reason, depth);
    }
    const auto type_name = "::" + name(record);
    std::map<std::string, unsigned> field_counts;
    for (const auto& mapped : mapping.fields) {
      const auto* field = mapped.field;
      if (field->isBitField() || !identifier(field->getNameAsString())) {
        return failure(record, "bit-fields and unnamed/non-ASCII fields are unsupported");
      }
      if (!supported(field->getType(), reason, depth + 1, waiting_for_field)) {
        if (reason == reconstructed_only) {
          return reconstruct(record, field->getNameAsString() + ": " + reason, depth);
        }
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
        return reconstruct(record, reason, depth);
      }
      if (!can_decode(type_name, mapping)) {
        return reconstruct(record,
                           "inferred constructor and remaining assignable fields are not usable; "
                           "provide EventCodec factory",
                           depth);
      }
      needed.clear();
      missing_destructor.clear();
      if (!lifetime_available(record, seen, false, !mapping.returns_prvalue(), reason,
                              needed, missing_destructor)) {
        return reconstruct(record, reason, depth);
      }
    }
    if (!needed.empty()) {
      const auto message =
          "destructor definition is unavailable for local decoding: " + missing_destructor;
      if (destructors_final_) {
        return reconstruct(record, message, depth);
      }
      needed_destructors_.merge(needed);
      waiting_for_destructors_.insert(canonical);
      return failure(record, message);
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
            const std::set<std::string>* external_destructors, bool destructors_final)
      : ci_(ci)
      , ast_(ci.getASTContext())
      , sema_(ci.getSema())
      , policy_(ci.getLangOpts())
      , viable_(viable)
      , external_destructors_(external_destructors)
      , destructors_final_(destructors_final) {
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
    // A reconstruction may refer to a record that was still being planned and
    // then failed. Drop those candidates so the generated code stays valid; a
    // record that cannot do without one (a reference member's referent, a
    // nested record) fails too, and so do the records that embed it.
    for (bool changed = true; changed;) {
      changed = false;
      std::set<Record*> built(manual_.begin(), manual_.end());
      for (const auto& mapping : ordered_) {
        built.insert(key(mapping.record));
      }
      for (auto it = ordered_.begin(); it != ordered_.end(); ++it) {
        std::string why;
        for (auto& followed : it->followed) {
          followed.pointees.erase(
              std::remove_if(followed.pointees.begin(), followed.pointees.end(),
                             [&](Record* pointee) { return !built.count(key(pointee)); }),
              followed.pointees.end());
          if (followed.declared && followed.pointees.empty() && why.empty()) {
            why = followed.field.field->getNameAsString() +
                  ": the referenced record could not be mapped";
          }
        }
        it->adopted.erase(
            std::remove_if(it->adopted.begin(), it->adopted.end(),
                           [&](const MappedField& mapped) {
                             return !built.count(key(mapped.adopt_pointee));
                           }),
            it->adopted.end());
        for (const auto& mapped : it->fields) {
          if (mapped.nested_record && !built.count(key(mapped.nested_record)) && why.empty()) {
            why = mapped.field->getNameAsString() + ": the nested record could not be mapped";
          }
        }
        // Any record whose codec this one names, directly or as an element or
        // pointee, must still exist; otherwise the generated code would not compile.
        for (auto* dependency : it->depends) {
          if (why.empty() && dependency != key(it->record) && !built.count(dependency)) {
            why = "it uses " + name(dependency) + ", which could not be mapped";
          }
        }
        if (!why.empty()) {
          failures_[key(it->record)] = why;
          state_[key(it->record)] = 3;
          ordered_.erase(it);
          changed = true;
          break;
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
      if (!mapping.reconstructed) {
        code << " " << single_line(name(mapping.record));
      }
    }
    for (const auto& mapping : ordered_) {
      if (mapping.reconstructed) {
        code << "; reconstructed " << single_line(name(mapping.record));
      }
    }
    for (const auto& [record, reason] : failures_) {
      code << "; skipped " << single_line(name(record)) << ": " << single_line(reason);
    }
    for (const auto& note : notes_) {
      code << "; note: " << single_line(note);
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
      if (mapping.reconstructed) {
        continue;
      }
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
    // Reconstructions come last and in two steps: every class is declared
    // before any body is defined, so records may refer to each other.
    for (const auto& mapping : ordered_) {
      if (!mapping.reconstructed) {
        continue;
      }
      const auto type = name(mapping.record);
      code << "template<> struct tracer_repl::ReconstructedRecord<::" << type << "> {\n"
           << "static void fill(tracer_repl::Reconstructed<::" << type
           << ">& result, const tracer_repl::EventValue& value);\n"
           << "static tracer_repl::Reconstructed<::" << type
           << "> decode(const tracer_repl::EventValue& value);\n};\n";
    }
    for (const auto& mapping : ordered_) {
      if (!mapping.reconstructed) {
        continue;
      }
      const auto type = name(mapping.record);
      const auto self = "tracer_repl::ReconstructedRecord<::" + type + ">";
      code << "inline void " << self << "::fill(tracer_repl::Reconstructed<::" << type
           << ">& result, const tracer_repl::EventValue& value) {\n";
      for (const auto& mapped : mapping.fields) {
        if (mapped.nested.empty()) {
          code << "result.decode(" << member_pointer(mapped) << ", value.field("
               << std::quoted(mapped.wire_name) << "));\n";
        } else {
          code << "result.fill_nested<::" << mapped.nested << ">(" << member_pointer(mapped)
               << ", value.field(" << std::quoted(mapped.wire_name) << "));\n";
        }
      }
      for (const auto& mapped : mapping.adopted) {
        code << "result.adopt(" << member_pointer(mapped) << ", value.field("
             << std::quoted(mapped.wire_name) << "));\n";
      }
      for (const auto& mapped : mapping.texts) {
        code << "result.follow_text(" << member_pointer(mapped) << ", value, "
             << std::quoted(mapped.wire_name) << ");\n";
      }
      for (const auto& mapped : mapping.scalars) {
        code << "result.follow_scalar(" << member_pointer(mapped) << ", value, "
             << std::quoted(mapped.wire_name) << ");\n";
      }
      for (const auto& followed : mapping.followed) {
        const bool reference = followed.declared != nullptr;
        const std::string missing_text =
            "event record " + type + " has no captured referent for reference member " +
            followed.field.owner + "::" + followed.field.field->getNameAsString();
        const auto missing = std::quoted(missing_text);
        if (followed.pointees.empty()) {
          if (reference) {
            code << "throw std::runtime_error(" << missing << ");\n";
          }
          continue;
        }
        code << (reference ? "if (!(" : "(void)(");
        bool first = true;
        for (auto* pointee : followed.pointees) {
          const auto pointee_name = name(pointee);
          if (!first) {
            code << " || ";
          }
          first = false;
          if (reference) {
            code << "result.follow_reference<::" << name(followed.declared) << ", ::"
                 << pointee_name << ">(" << followed.field.offset;
          } else {
            code << "result.follow<::" << pointee_name << ">(" << member_pointer(followed.field);
          }
          code << ", value, " << std::quoted(followed.field.wire_name) << ", "
               << std::quoted(pointee_name) << ")";
        }
        if (reference) {
          code << ")) throw std::runtime_error(" << missing << ");\n";
        } else {
          code << ");\n";
        }
      }
      code << "}\n"
           << "inline tracer_repl::Reconstructed<::" << type << "> " << self
           << "::decode(const tracer_repl::EventValue& value) {\n"
           << "if (!value.matches_record_type(" << std::quoted(type) << ")) "
           << "throw std::invalid_argument(\"event record type mismatch: \" + value.cpp_type());\n"
           << "tracer_repl::Reconstructed<::" << type << "> result;\n"
           << "fill(result, value);\n"
           << "return result;\n}\n";
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
                         const std::set<std::string>* external_destructors,
                         bool destructors_final) {
  return Generator(compiler, viable, external_destructors, destructors_final).run(types);
}
} // namespace tracer_repl

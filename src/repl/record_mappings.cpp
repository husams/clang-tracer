#include "record_mappings.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Sema/Sema.h>
#include <llvm/Config/llvm-config.h>

namespace tracer_repl {
namespace {
using Record = clang::CXXRecordDecl;

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
  std::vector<Record*> ordered_;
  const std::function<bool(const std::string&)>& viable_;

  Record* key(Record* record) { return record->getCanonicalDecl(); }

  clang::QualType record_type(Record* record) {
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

  bool supported(clang::QualType type, std::string& reason, unsigned depth) {
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
                !supported(args[i].getAsType(), reason, depth + 1)) {
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
      reason = failures_[key(record)];
      return false;
    }
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
    for (auto* context = record->getDeclContext(); context; context = context->getParent()) {
      if (auto* space = llvm::dyn_cast<clang::NamespaceDecl>(context)) {
        if (space->isAnonymousNamespace()) {
          return failure(record, "anonymous namespace type");
        }
      }
      if (llvm::isa<clang::FunctionDecl>(context)) {
        return failure(record, "local class");
      }
      if (const auto* parent = llvm::dyn_cast<Record>(context)) {
        if (parent->getAccess() == clang::AS_private ||
            parent->getAccess() == clang::AS_protected) {
          return failure(record, "enclosing type is private/protected");
        }
      }
    }
    if (record->getAccess() == clang::AS_private || record->getAccess() == clang::AS_protected) {
      return failure(record, "nested type is private/protected");
    }
    if (record->isUnion() || record->getNumBases() || record->isPolymorphic() ||
        record->isAbstract()) {
      return failure(record, "unions, inheritance and polymorphic classes "
                             "require custom handling");
    }
    unsigned fields = 0;
    const auto type_name = "::" + name(record);
    std::string trait = "std::is_default_constructible_v<" + type_name +
                        "> && std::is_move_constructible_v<" + type_name +
                        "> && std::is_destructible_v<" + type_name + ">";
    for (const auto* field : record->fields()) {
      if (++fields > 128) {
        return failure(record, "more than 128 fields");
      }
      if (field->getAccess() != clang::AS_public) {
        return failure(record, "private/protected fields cannot be captured automatically");
      }
      if (field->isBitField() || !identifier(field->getNameAsString())) {
        return failure(record, "bit-fields and unnamed/non-ASCII fields are unsupported");
      }
      if (field->getType().isConstQualified()) {
        return failure(record, "const members require an EventCodec factory");
      }
      std::string reason;
      if (!supported(field->getType(), reason, depth + 1)) {
        return failure(record, field->getNameAsString() + ": " + reason);
      }
      const auto expression = "std::declval<" + type_name + "&>()." + field->getNameAsString();
      trait +=
          " && std::is_assignable_v<decltype((" + expression + ")), decltype(" + expression + ")>";
    }
    if (!viable_(trait)) {
      return failure(record, "accessible default/move construction, destruction and "
                             "assignable fields required; provide EventCodec factory");
    }
    state_[canonical] = 2;
    ordered_.push_back(record);
    return true;
  }

public:
  Generator(clang::CompilerInstance& ci, const std::function<bool(const std::string&)>& viable)
      : ci_(ci)
      , ast_(ci.getASTContext())
      , sema_(ci.getSema())
      , policy_(ci.getLangOpts())
      , viable_(viable) {
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
        if (!plan(record)) {
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
    for (auto* record : ordered_) {
      code << " " << single_line(name(record));
    }
    for (const auto& [record, reason] : failures_) {
      code << "; skipped " << single_line(name(record)) << ": " << single_line(reason);
    }
    if (ordered_.empty() && failures_.empty()) {
      code << " none needed (built-in or existing codecs)";
    }
    code << '\n';
    for (auto* record : ordered_) {
      const auto type = name(record);
      generated.types.push_back(type);
      code << "template<> struct tracer_repl::EventRecord<::" << type << "> {\n"
           << "static constexpr const char* type_name=" << std::quoted(type) << ";\n"
           << "static auto fields(){return std::make_tuple(";
      bool first = true;
      for (const auto* field : record->fields()) {
        if (!first) {
          code << ',';
        }
        first = false;
        code << "tracer_repl::event_member(\"" << field->getNameAsString() << "\", &::" << type
             << "::" << field->getNameAsString() << ')';
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
                         const std::function<bool(const std::string&)>& viable) {
  return Generator(compiler, viable).run(types);
}
} // namespace tracer_repl

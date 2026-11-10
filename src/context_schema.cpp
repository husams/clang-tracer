#include "context_schema.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <clang/AST/ASTContext.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <google/protobuf/descriptor.pb.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

#include "tracer/compat.hpp"

namespace tracer::context_engine {
namespace {
using FD = google::protobuf::FieldDescriptorProto;
using MP = google::protobuf::DescriptorProto;
using FP = google::protobuf::FileDescriptorProto;
using FDS = google::protobuf::FileDescriptorSet;

std::string record_spelling(const clang::CXXRecordDecl *record) {
  clang::PrintingPolicy policy(record->getASTContext().getLangOpts());
  policy.SuppressTagKeyword = true;
  return clang::QualType(record->getTypeForDecl(), 0)
      .getCanonicalType().getAsString(policy);
}

std::string canonical(clang::QualType t) {
  if (const auto *r = t.getCanonicalType()->getAsCXXRecordDecl()) {
    return record_spelling(r);
  }
  return t.getCanonicalType().getAsString();
}

std::string canonical_with_template_arguments(clang::QualType type) {
  if (type.isNull()) {
    return {};
  }
  type = type.getNonReferenceType().getCanonicalType();
  const auto *record = type->getAsCXXRecordDecl();
  if (record && llvm::isa<clang::ClassTemplateSpecializationDecl>(record)) {
    return type.getAsString();
  }
  return canonical(type);
}

std::string normalize_std(std::string s) {
  for (const std::string prefix : {"std::__1::", "std::__cxx11::"}) {
    std::size_t p = 0;
    while ((p = s.find(prefix, p)) != std::string::npos) {
      s.replace(p, prefix.size(), "std::");
    }
  }
  for (const std::string tag : {"class ", "struct ", "enum "}) {
    std::size_t p = 0;
    while ((p = s.find(tag, p)) != std::string::npos) {
      s.erase(p, tag.size());
    }
  }
  std::size_t p = 0;
  while ((p = s.find("> >", p)) != std::string::npos) {
    s.replace(p, 3, ">>");
  }
  p = 0;
  while ((p = s.find("std::string", p)) != std::string::npos) {
    s.replace(p, 11, "std::basic_string<char>");
    p += 23;
  }
  p = 0;
  while ((p = s.find("std::basic_string", p)) != std::string::npos) {
    const auto after = p + 17;
    if (after >= s.size() || (s[after] != '<' && s[after] != '_')) {
      s.insert(after, "<char>");
      p = after + 6;
    } else {
      p = after;
    }
  }
  return s;
}

bool physical_standard_library_header(clang::ASTContext &ast,
                                      clang::SourceLocation location) {
  if (location.isInvalid())
    return false;
  auto &source = ast.getSourceManager();
  location = source.getSpellingLoc(location);
  const auto filename = source.getFilename(location);
  if (filename.empty())
    return false;
  llvm::SmallString<256> physical;
  if (llvm::sys::fs::real_path(filename, physical))
    return false;
  const llvm::StringRef path(physical);
  if (path.find("/include/c++/") == llvm::StringRef::npos)
    return false;
  llvm::StringRef roots(TRACER_CONTEXT_SYSTEM_ROOTS);
  while (!roots.empty()) {
    const auto pair = roots.split('|');
    roots = pair.second;
    if (pair.first.empty())
      continue;
    llvm::SmallString<256> canonical_root;
    if (!llvm::sys::fs::real_path(pair.first, canonical_root) &&
        ::tracer::compat::starts_with(path, llvm::StringRef(canonical_root).str() + "/")) {
      return true;
    }
  }
  return false;
}

const clang::ClassTemplateSpecializationDecl *spec(clang::QualType t) {
  return llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
      t.getNonReferenceType()->getAsCXXRecordDecl());
}

std::string template_name(const clang::ClassTemplateSpecializationDecl *s) {
  return s ? normalize_std(
                 s->getSpecializedTemplate()->getQualifiedNameAsString())
           : "";
}

clang::QualType arg(const clang::ClassTemplateSpecializationDecl *s,
                    unsigned i) {
  return s && i < s->getTemplateArgs().size() &&
                 s->getTemplateArgs()[i].getKind() ==
                     clang::TemplateArgument::Type
             ? s->getTemplateArgs()[i].getAsType()
             : clang::QualType{};
}

bool owning_snapshot_pointer(const std::string &name) {
  return name == "std::unique_ptr" || name == "std::shared_ptr";
}

bool snapshot_pointee(clang::QualType type) {
  return !type.isNull() && type->isObjectType() && !type->isArrayType() &&
         !type->isIncompleteType() && !type.isVolatileQualified();
}

SchemaField snapshot_scalar(std::string name, std::uint32_t number,
                            SchemaKind kind, unsigned width) {
  SchemaField field;
  field.name = std::move(name);
  field.number = number;
  field.kind = kind;
  field.bit_width = width;
  return field;
}

struct Builder {
  clang::ASTContext &ast;
  SchemaLimits limits;
  FDS files;
  FP *file = nullptr;
  std::unordered_set<const clang::CXXRecordDecl *> active;
  std::unordered_map<const clang::CXXRecordDecl *,
                     std::shared_ptr<const SchemaMessage>>
      cache;
  std::size_t fields = 0, messages = 0;
  std::string error;
  bool snapshot = false;

  bool system_default_specialization(
      const clang::ClassTemplateSpecializationDecl *s) const {
    if (!s) {
      return false;
    }
    // A QualType's RecordType can retain an earlier forward declaration of a
    // specialization (often created while parsing the standard container
    // header).  In particular, looking only at that declaration can report
    // TSK_Undeclared/TSK_ImplicitInstantiation even when the program later
    // provides an explicit specialization in a project header.  Make the
    // admission decision from the most recent definition when one exists.
    if (const auto *definition =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                s->getDefinition())) {
      s = definition;
    }
    const auto kind = s->getSpecializationKind();
    const auto *primary = s->getSpecializedTemplate();
    if (!primary ||
        !physical_standard_library_header(ast, primary->getLocation()))
      return false;
    if (kind == clang::TSK_ImplicitInstantiation) {
      return true;
    }
    if (kind == clang::TSK_Undeclared) {
      return true;
    }
    if (kind == clang::TSK_ExplicitSpecialization ||
        kind == clang::TSK_ExplicitInstantiationDeclaration ||
        kind == clang::TSK_ExplicitInstantiationDefinition) {
      return physical_standard_library_header(ast, s->getLocation());
    }
    return false;
  }

  void collect_fields(const clang::CXXRecordDecl *r, SchemaMessage &out,
                      unsigned depth, std::uint32_t &id,
                      std::unordered_set<const clang::CXXRecordDecl *> &seen,
                      std::unordered_map<std::string, std::pair<std::size_t, std::string>> &owners) {
    if (!r || !r->hasDefinition() || !error.empty()) {
      return;
    }
    r = r->getDefinition();
    if (!seen.insert(r).second) {
      return;
    }
    if (r->isUnion()) {
      error =
          "union state layout is unsupported: " + r->getQualifiedNameAsString();
      return;
    }
    for (const auto &base : r->bases()) {
      auto bt = base.getType();
      if (const auto *bs = spec(bt)) {
        auto n = template_name(bs);
        if (n == "tracer::state" || n == "tracer::shared_state") {
          continue;
        }
      }
      auto *br = bt->getAsCXXRecordDecl();
      if (br && br->hasDefinition()) {
        collect_fields(br, out, depth, id, seen, owners);
      }
    }
    for (const auto *f : r->fields()) {
      if (snapshot && f->isUnnamedBitField())
        continue;
      if (f->isBitField() && !snapshot) {
        error = "bit-field state member unsupported";
        return;
      }
      auto sf = describe(f->getType(), f->getNameAsString(), id++, depth);
      if (!error.empty()) {
        return;
      }
      if (snapshot) {
        const auto owner = record_spelling(r);
        const auto [it, inserted] = owners.try_emplace(sf.name, out.fields.size(), owner);
        if (!inserted) {
          auto &first = out.fields[it->second.first];
          if (first.name == sf.name)
            first.name = it->second.second + "::" + first.name;
          sf.name = owner + "::" + sf.name;
        }
      } else if (std::any_of(
              out.fields.begin(), out.fields.end(),
              [&](const auto &prior) { return prior.name == sf.name; })) {
        error = "ambiguous inherited state member: " + sf.name;
        return;
      }
      out.fields.push_back(std::move(sf));
      if (++fields > limits.fields) {
        error = "schema field limit exceeded";
        return;
      }
    }
  }

  std::string proto_name(clang::QualType t) {
    if (const auto *r = t->getAsCXXRecordDecl()) {
      return r->getQualifiedNameAsString();
    }
    return canonical(t);
  }

  SchemaField describe(clang::QualType t, const std::string &name,
                       std::uint32_t number, unsigned depth) {
    SchemaField out;
    out.name = name;
    out.cpp_type = canonical(t);
    out.number = number;
    if (depth > limits.depth) {
      error = "schema nesting limit exceeded";
      return out;
    }
    if (t->isBooleanType()) {
      out.kind = SchemaKind::boolean;
      out.bit_width = 1;
      return out;
    }
    if (t->isEnumeralType()) {
      out.kind = SchemaKind::enumeration;
      out.enum_name = proto_name(t);
      if (const auto *e = t->getAs<clang::EnumType>()->getDecl()) {
        for (const auto *v : e->enumerators()) {
          const auto value = v->getInitVal().getExtValue();
          if (value < INT32_MIN || value > INT32_MAX) {
            error = "enum value exceeds protobuf int32 range: " + name;
            return out;
          }
          out.enum_values.emplace_back(v->getNameAsString(),
                                       static_cast<std::int32_t>(value));
        }
      }
      return out;
    }
    if (t->isIntegerType()) {
      if (snapshot && (ast.getTypeSize(t) > 64 || t->isBitIntType())) {
        error = "unsupported integer width in snapshot: " + name;
        return out;
      }
      out.kind = t->isUnsignedIntegerType() ? SchemaKind::unsigned_integer
                                            : SchemaKind::signed_integer;
      out.bit_width = ast.getTypeSize(t);
      return out;
    }
    if (t->isSpecificBuiltinType(clang::BuiltinType::Float)) {
      out.kind = SchemaKind::float32;
      return out;
    }
    if (t->isSpecificBuiltinType(clang::BuiltinType::Double)) {
      out.kind = SchemaKind::float64;
      return out;
    }
    if (snapshot && t->isPointerType()) {
      out.kind = SchemaKind::unsigned_integer;
      out.bit_width = ast.getTypeSize(t);
      return out;
    }
    if (snapshot) {
      if (const auto *array = ast.getAsConstantArrayType(t)) {
        out.kind = SchemaKind::array;
        out.repeated = true;
        out.fixed_count = array->getSize().getLimitedValue();
        out.value = std::make_shared<SchemaField>(
            describe(array->getElementType(), name + "[]", 1, depth + 1));
        return out;
      }
    }
    if (t->isPointerType() || t->isReferenceType() ||
        t->isMemberPointerType()) {
      error = "unsupported pointer/reference field: " + name;
      return out;
    }
    auto *s = spec(t);
    const auto tn = template_name(s);
    if (s && tn.rfind("std::", 0) == 0 && !system_default_specialization(s)) {
      error =
          "user-defined standard-library specialization is unsupported: " + tn;
      return out;
    }
    if (snapshot && (owning_snapshot_pointer(tn) || tn == "std::weak_ptr" ||
                     tn == "std::variant")) {
      auto record = std::make_shared<SchemaMessage>();
      record->cpp_type = out.cpp_type;
      record->full_name = record->cpp_type;
      const auto add_optional = [&](clang::QualType type, std::string field) {
        SchemaField optional;
        optional.name = std::move(field);
        optional.number = record->fields.size() + 1;
        optional.kind = SchemaKind::optional;
        optional.optional = true;
        optional.value = std::make_shared<SchemaField>(
            describe(type, optional.name, 1, depth + 1));
        record->fields.push_back(std::move(optional));
      };
      if (tn == "std::variant") {
        record->fields.push_back(snapshot_scalar("index", 1, SchemaKind::unsigned_integer, 64));
        for (const auto &element : s->getTemplateArgs()[0].pack_elements()) {
          if (!error.empty())
            break;
          add_optional(element.getAsType(),
                       "alternative_" + std::to_string(record->fields.size() - 1));
        }
      } else if (tn == "std::weak_ptr") {
        record->fields.push_back(snapshot_scalar("expired", 1, SchemaKind::boolean, 1));
      } else {
        if (tn == "std::unique_ptr") {
          const auto *deleter = spec(arg(s, 1));
          if (template_name(deleter) != "std::default_delete" ||
              !system_default_specialization(deleter) ||
              arg(deleter, 0).getCanonicalType() != arg(s, 0).getCanonicalType()) {
            error = "smart pointer snapshot requires std::default_delete: " + name;
            return out;
          }
        }
        record->fields.push_back(snapshot_scalar("address", 1, SchemaKind::unsigned_integer, 64));
        if (snapshot_pointee(arg(s, 0)))
          add_optional(arg(s, 0), "value");
      }
      out.kind = SchemaKind::message;
      out.message = std::move(record);
      return out;
    }
    if (snapshot && tn.empty()) {
      const auto *record = t->getAsCXXRecordDecl();
      const auto standard = record ? normalize_std(record->getQualifiedNameAsString())
                                   : std::string{};
      if ((standard == "std::monostate" || standard == "std::any") &&
          physical_standard_library_header(ast, record->getLocation())) {
        auto message = std::make_shared<SchemaMessage>();
        message->cpp_type = out.cpp_type;
        message->full_name = message->cpp_type;
        if (standard == "std::any") {
          // Matches the tagged payload encoded by Codec<std::any>.
          message->fields.push_back(snapshot_scalar("kind", 1, SchemaKind::string, 0));
          const auto add_optional = [&](const char *name, std::uint32_t number,
                                        SchemaKind kind, unsigned width) {
            SchemaField field;
            field.name = name;
            field.number = number;
            field.kind = SchemaKind::optional;
            field.optional = true;
            field.value = std::make_shared<SchemaField>(
                snapshot_scalar(name, 1, kind, width));
            message->fields.push_back(std::move(field));
          };
          add_optional("int_value", 2, SchemaKind::signed_integer, 32);
          add_optional("bool_value", 3, SchemaKind::boolean, 1);
          add_optional("string_value", 4, SchemaKind::string, 0);
          add_optional("type_name", 5, SchemaKind::string, 0);
        }
        out.kind = SchemaKind::message;
        out.message = std::move(message);
        return out;
      }
    }
    if (snapshot && (tn == "std::pair" || tn == "std::tuple")) {
      auto record = std::make_shared<SchemaMessage>();
      record->cpp_type = out.cpp_type;
      record->full_name = record->cpp_type;
      const auto add = [&](clang::QualType type, const std::string &field) {
        record->fields.push_back(describe(type, field, record->fields.size() + 1,
                                          depth + 1));
      };
      if (tn == "std::pair") {
        add(arg(s, 0), "first");
        add(arg(s, 1), "second");
      } else {
        for (const auto &element : s->getTemplateArgs()[0].pack_elements())
          add(element.getAsType(), "item_" + std::to_string(record->fields.size()));
      }
      out.kind = SchemaKind::message;
      out.message = std::move(record);
      return out;
    }
    if (snapshot && tn == "std::basic_string_view") {
      const auto character = arg(s, 0);
      const auto *traits = spec(arg(s, 1));
      if (!character->isCharType() || !traits ||
          template_name(traits) != "std::char_traits" ||
          !system_default_specialization(traits)) {
        error = "unsupported string_view specialization: " + name;
        return out;
      }
      out.kind = SchemaKind::string;
      return out;
    }
    if (tn.rfind("std::basic_string", 0) == 0) {
      const auto traits = spec(arg(s, 1));
      const auto allocator = spec(arg(s, 2));
      if (!system_default_specialization(s) ||
          s->getTemplateArgs().size() != 3 || !traits ||
          template_name(traits) != "std::char_traits" ||
          !system_default_specialization(traits) || !allocator ||
          template_name(allocator) != "std::allocator" ||
          !system_default_specialization(allocator)) {
        error = "unsupported string specialization: " + name;
        return out;
      }
      out.kind = SchemaKind::string;
      return out;
    }
    if (s) {
      const auto a = arg(s, 0), b = arg(s, 1);
      if (tn == "std::optional") {
        out.kind = SchemaKind::optional;
        out.optional = true;
        out.value =
            std::make_shared<SchemaField>(describe(a, name, 1, depth + 1));
        return out;
      }
      if (tn == "std::array" || tn == "std::vector" || tn == "std::deque" ||
          tn == "std::list" || tn == "std::set" || tn == "std::unordered_set") {
        // Only the standard default allocator/comparator/hash/equality forms
        // are admitted.
        const unsigned expected =
            tn == "std::array"
                ? 2
                : (tn == "std::set" ? 3 : (tn == "std::unordered_set" ? 4 : 2));
        if (s->getTemplateArgs().size() != expected) {
          error = "unsupported standard container specialization: " + name;
          return out;
        }
        if (tn != "std::array") {
          const auto allocator =
              spec(s->getTemplateArgs()[expected - 1].getAsType());
          if (!allocator || template_name(allocator) != "std::allocator" ||
              !system_default_specialization(allocator) ||
              arg(allocator, 0).getCanonicalType() != a.getCanonicalType()) {
            error = "unsupported container allocator: " + name;
            return out;
          }
        }
        auto default_unary = [&](unsigned index,
                                 const char *template_name_expected) {
          const auto *specialization =
              spec(s->getTemplateArgs()[index].getAsType());
          return specialization &&
                 template_name(specialization) == template_name_expected &&
                 system_default_specialization(specialization) &&
                 arg(specialization, 0).getCanonicalType() ==
                     a.getCanonicalType();
        };
        if ((tn == "std::set" && !default_unary(1, "std::less")) ||
            (tn == "std::unordered_set" &&
             (!default_unary(1, "std::hash") ||
              !default_unary(2, "std::equal_to")))) {
          error =
              "unsupported comparator/hash/equality specialization: " + name;
          return out;
        }
        out.kind = tn == "std::array"    ? SchemaKind::array
                   : tn == "std::vector" ? SchemaKind::vector
                   : tn == "std::deque"  ? SchemaKind::deque
                   : tn == "std::list"   ? SchemaKind::list
                   : tn == "std::set"    ? SchemaKind::set
                                         : SchemaKind::unordered_set;
        if (tn == "std::array" && s->getTemplateArgs()[1].getKind() ==
                                      clang::TemplateArgument::Integral) {
          out.fixed_count =
              s->getTemplateArgs()[1].getAsIntegral().getZExtValue();
        }
        out.repeated = true;
        auto element = describe(a, name + "[]", 1, depth + 1);
        if (!error.empty()) {
          return out;
        }
        if (tn == "std::unordered_set" && element.kind == SchemaKind::message) {
          error = "unordered_set custom element hash is unsupported: " + name;
          return out;
        }
        out.value = std::make_shared<SchemaField>(std::move(element));
        return out;
      }
      if (tn == "std::map" || tn == "std::unordered_map") {
        const unsigned expected = tn == "std::map" ? 4 : 5;
        if (s->getTemplateArgs().size() != expected) {
          error = "unsupported map specialization: " + name;
          return out;
        }
        auto default_unary = [&](unsigned index,
                                 const char *template_expected) {
          const auto *sp = spec(s->getTemplateArgs()[index].getAsType());
          return sp && template_name(sp) == template_expected &&
                 system_default_specialization(sp) &&
                 arg(sp, 0).getCanonicalType() == a.getCanonicalType();
        };
        auto default_pair_allocator = [&](unsigned index) {
          const auto *alloc = spec(s->getTemplateArgs()[index].getAsType());
          if (!alloc || template_name(alloc) != "std::allocator" ||
              !system_default_specialization(alloc)) {
            return false;
          }
          const auto *pair = spec(arg(alloc, 0));
          return pair && template_name(pair) == "std::pair" &&
                 system_default_specialization(pair) &&
                 pair->getTemplateArgs().size() == 2 &&
                 arg(pair, 0).isConstQualified() &&
                 arg(pair, 0).getCanonicalType().getUnqualifiedType() ==
                     a.getCanonicalType().getUnqualifiedType() &&
                 arg(pair, 1).getCanonicalType() == b.getCanonicalType();
        };
        if (tn == "std::map" &&
            (!default_unary(2, "std::less") || !default_pair_allocator(3))) {
          error = "unsupported map comparator/allocator: " + name;
          return out;
        }
        if (tn == "std::unordered_map" && (!default_unary(2, "std::hash") ||
                                           !default_unary(3, "std::equal_to") ||
                                           !default_pair_allocator(4))) {
          error = "unsupported map hash/equality/allocator: " + name;
          return out;
        }
        out.kind =
            tn == "std::map" ? SchemaKind::map : SchemaKind::unordered_map;
        out.repeated = true;
        auto key_field = describe(a, name + "{key}", 1, depth + 1);
        if (!error.empty()) {
          return out;
        }
        if (key_field.kind != SchemaKind::boolean &&
            key_field.kind != SchemaKind::signed_integer &&
            key_field.kind != SchemaKind::unsigned_integer &&
            key_field.kind != SchemaKind::enumeration &&
            key_field.kind != SchemaKind::string) {
          error = "unsupported non-scalar map key: " + name;
          return out;
        }
        if (tn == "std::unordered_map" &&
            key_field.kind == SchemaKind::message) {
          error = "unordered_map custom key hash is unsupported: " + name;
          return out;
        }
        out.key = std::make_shared<SchemaField>(std::move(key_field));
        out.value = std::make_shared<SchemaField>(
            describe(b, name + "{value}", 2, depth + 1));
        return out;
      }
      if (tn.rfind("std::", 0) == 0) {
        error = "unsupported standard type/specialization: " + tn;
        return out;
      }
    }
    auto *r = t->getAsCXXRecordDecl();
    if (!r || !r->hasDefinition()) {
      error = "incomplete or unsupported field type: " + name;
      return out;
    }
    out.kind = SchemaKind::message;
    out.message = message(r->getDefinition(), depth + 1);
    return out;
  }

  std::shared_ptr<const SchemaMessage> message(const clang::CXXRecordDecl *r,
                                               unsigned depth) {
    auto result = std::make_shared<SchemaMessage>();
    result->cpp_type = record_spelling(r);
    result->full_name = result->cpp_type;
    if (auto i = cache.find(r); i != cache.end()) {
      return i->second;
    }
    if (active.count(r)) {
      error =
          "recursive/cyclic state schema is unsupported: " + result->cpp_type;
      return result;
    }
    if (++messages > limits.messages) {
      error = "schema message limit exceeded";
      return result;
    }
    active.insert(r);
    std::uint32_t id = 1;
    std::unordered_set<const clang::CXXRecordDecl *> seen;
    std::unordered_map<std::string, std::pair<std::size_t, std::string>> owners;
    collect_fields(r, *result, depth, id, seen, owners);
    active.erase(r);
    cache.emplace(r, result);
    return result;
  }
};

std::string ident(std::string_view text) {
  std::uint64_t h = 1469598103934665603ULL;
  for (unsigned char c : text) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << "M_" << std::hex << h;
  return out.str();
}

struct DescriptorEmitter {
  FP &file;
  bool snapshot = false;
  std::unordered_set<std::string> emitted_messages, emitted_enums;

  std::string full(std::string_view n) {
    return "." + file.package() + "." + std::string(n);
  }

  std::string message_name(std::string_view cpp) { return ident(cpp); }

  void role(FD *, const std::string &) {}

  void enum_def(const SchemaField &f) {
    const auto name = ident(f.enum_name);
    if (!emitted_enums.insert(name).second) {
      return;
    }
    auto *e = file.add_enum_type();
    e->set_name(name);
    if (f.enum_values.empty()) {
      auto *v = e->add_value();
      v->set_name("UNSPECIFIED");
      v->set_number(0);
      return;
    }
    e->mutable_options()->set_allow_alias(true);
    if (std::none_of(f.enum_values.begin(), f.enum_values.end(),
                     [](const auto &v) { return v.second == 0; })) {
      auto *x = e->add_value();
      x->set_name("UNSPECIFIED");
      x->set_number(0);
    }
    for (const auto &[n, v] : f.enum_values) {
      auto *x = e->add_value();
      std::string value_name = name + "_" + n;
      for (auto &c : value_name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
          c = '_';
        }
      }
      x->set_name(std::move(value_name));
      x->set_number(v);
    }
  }

  std::string type(const SchemaField &f, const std::string &path) {
    switch (f.kind) {
    case SchemaKind::boolean:
      return "bool";
    case SchemaKind::signed_integer:
      return f.bit_width <= 32 ? "sint32" : "sint64";
    case SchemaKind::unsigned_integer:
      return f.bit_width <= 32 ? "uint32" : "uint64";
    case SchemaKind::float32:
      return "float";
    case SchemaKind::float64:
      return "double";
    case SchemaKind::string:
      return "string";
    case SchemaKind::enumeration:
      enum_def(f);
      return "enum:" + full(ident(f.enum_name));
    case SchemaKind::message:
      emit(*f.message);
      return full(message_name(f.message->cpp_type));
    case SchemaKind::optional: {
      const auto n = ident(path + "_optional");
      if (emitted_messages.insert(n).second) {
        auto *m = file.add_message_type();
        m->set_name(n);
        auto *x = m->add_field();
        x->set_name("value");
        x->set_number(1);
        x->set_label(f.value->repeated ? FD::LABEL_REPEATED
                                       : FD::LABEL_OPTIONAL);
        set_type(*f.value, path + "_value", x);
        role(x, "optional_value");
      }
      return full(n);
    }
    case SchemaKind::map:
    case SchemaKind::unordered_map: {
      const auto n = ident(path + "_entry");
      if (emitted_messages.insert(n).second) {
        auto *m = file.add_message_type();
        m->set_name(n);
        auto *k = m->add_field();
        k->set_name("key");
        k->set_number(1);
        k->set_label(FD::LABEL_OPTIONAL);
        set_type(*f.key, path + "_key", k);
        auto *v = m->add_field();
        v->set_name("value");
        v->set_number(2);
        v->set_label(f.value->repeated ? FD::LABEL_REPEATED
                                       : FD::LABEL_OPTIONAL);
        set_type(*f.value, path + "_value", v);
      }
      return full(n);
    }
    case SchemaKind::array:
    case SchemaKind::vector:
    case SchemaKind::deque:
    case SchemaKind::list:
    case SchemaKind::set:
    case SchemaKind::unordered_set:
      if (f.value && (f.value->kind == SchemaKind::array ||
                      f.value->kind == SchemaKind::vector ||
                      f.value->kind == SchemaKind::deque ||
                      f.value->kind == SchemaKind::list ||
                      f.value->kind == SchemaKind::set ||
                      f.value->kind == SchemaKind::unordered_set ||
                      f.value->kind == SchemaKind::map ||
                      f.value->kind == SchemaKind::unordered_map)) {
        return full(container_wrapper(*f.value, path + "_nested"));
      }
      return type(*f.value, path + "_element");
    }
    return "bytes";
  }

  std::string container_wrapper(const SchemaField &container,
                                const std::string &path) {
    const auto name = ident(path + "_container");
    if (emitted_messages.insert(name).second) {
      auto *m = file.add_message_type();
      m->set_name(name);
      auto *x = m->add_field();
      x->set_name("items");
      x->set_number(1);
      x->set_label(FD::LABEL_REPEATED);
      set_type(container, path + "_items", x);
      role(x, "nested_container_items");
    }
    return name;
  }

  void set_type(const SchemaField &f, const std::string &path, FD *out) {
    const auto t = type(f, path);
    if (t == "bool") {
      out->set_type(FD::TYPE_BOOL);
    } else if (t == "sint32") {
      out->set_type(FD::TYPE_SINT32);
    } else if (t == "sint64") {
      out->set_type(FD::TYPE_SINT64);
    } else if (t == "uint32") {
      out->set_type(FD::TYPE_UINT32);
    } else if (t == "uint64") {
      out->set_type(FD::TYPE_UINT64);
    } else if (t == "float") {
      out->set_type(FD::TYPE_FLOAT);
    } else if (t == "double") {
      out->set_type(FD::TYPE_DOUBLE);
    } else if (t == "string") {
      out->set_type(FD::TYPE_STRING);
    } else if (t.rfind("enum:", 0) == 0) {
      out->set_type(FD::TYPE_ENUM);
      out->set_type_name(t.substr(5));
    } else if (!t.empty() && t[0] == '.') {
      out->set_type(FD::TYPE_MESSAGE);
      out->set_type_name(t);
    } else {
      out->set_type(FD::TYPE_BYTES);
    }
  }

  void emit(const SchemaMessage &m) {
    const auto name = message_name(m.cpp_type);
    if (!emitted_messages.insert(name).second) {
      return;
    }
    auto *out = file.add_message_type();
    out->set_name(name);
    for (const auto &f : m.fields) {
      auto *x = out->add_field();
      const bool simple =
          !f.name.empty() &&
          std::all_of(
              f.name.begin(), f.name.end(),
              [](unsigned char c) { return std::isalnum(c) || c == '_'; }) &&
          !std::isdigit(static_cast<unsigned char>(f.name.front()));
      x->set_name(snapshot || !simple ? "field_" + std::to_string(f.number) : f.name);
      if (snapshot)
        x->set_json_name(f.name);
      x->set_number(f.number);
      x->set_label(f.repeated ? FD::LABEL_REPEATED : FD::LABEL_OPTIONAL);
      set_type(f, m.cpp_type + "." + f.name, x);
      if (!snapshot && !simple) {
        auto *opt = x->mutable_options()->add_uninterpreted_option();
        auto *part = opt->add_name();
        part->set_name_part("tracer_cpp_field_name");
        part->set_is_extension(false);
        opt->set_string_value(f.name);
      }
      switch (f.kind) {
      case SchemaKind::optional:
        role(x, "optional");
        break;
      case SchemaKind::array:
        role(x, "array:" + std::to_string(f.fixed_count));
        break;
      case SchemaKind::vector:
        role(x, "vector");
        break;
      case SchemaKind::deque:
        role(x, "deque");
        break;
      case SchemaKind::list:
        role(x, "list");
        break;
      case SchemaKind::set:
        role(x, "set");
        break;
      case SchemaKind::unordered_set:
        role(x, "unordered_set");
        break;
      case SchemaKind::map:
        role(x, "map");
        break;
      case SchemaKind::unordered_map:
        role(x, "unordered_map");
        break;
      default:
        break;
      }
    }
  }
};
} // namespace

namespace {
std::size_t
schema_tree_bytes(const std::shared_ptr<const SchemaMessage> &root) {
  std::unordered_set<const SchemaMessage *> messages;
  std::unordered_set<const SchemaField *> fields;
  std::size_t total = 0;
  std::function<void(const SchemaMessage *)> visit_message;
  std::function<void(const SchemaField *)> visit_field;
  visit_field = [&](const SchemaField *f) {
    if (!f || !fields.insert(f).second) {
      return;
    }
    total +=
        sizeof(SchemaField) + f->name.size() + f->cpp_type.size() +
        f->enum_name.size() +
        f->enum_values.size() * sizeof(std::pair<std::string, std::int32_t>);
    for (const auto &v : f->enum_values) {
      total += v.first.size();
    }
    visit_message(f->message.get());
    visit_field(f->key.get());
    visit_field(f->value.get());
  };
  visit_message = [&](const SchemaMessage *m) {
    if (!m || !messages.insert(m).second) {
      return;
    }
    total += sizeof(SchemaMessage) + m->cpp_type.size() + m->full_name.size() +
             m->snapshot_cpp_type.size();
    for (const auto &f : m->fields) {
      visit_field(&f);
    }
  };
  visit_message(root.get());
  return total;
}
} // namespace

namespace {
void hash_length_prefixed(std::uint64_t &hash, std::string_view value) {
  const auto size = static_cast<std::uint64_t>(value.size());
  for (unsigned i = 0; i < 8; ++i) {
    hash ^= static_cast<unsigned char>(size >> (i * 8));
    hash *= 1099511628211ULL;
  }
  for (const unsigned char byte : value) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
}
} // namespace

std::string compute_schema_revision_id(std::string_view descriptor_bytes,
                                       const StateFamilyKey& family, std::string_view root_type,
                                       ::tracer::compat::span<const std::string> container_roles) {
  std::uint64_t hash = 1469598103934665603ULL;
  hash_length_prefixed(hash, "clang-tracer-context-schema-revision-v1");
  hash_length_prefixed(hash, descriptor_bytes);
  hash_length_prefixed(
      hash, std::string_view(
                reinterpret_cast<const char *>(family.source_lineage_id.data()),
                family.source_lineage_id.size()));
  hash_length_prefixed(hash, family.cpp_state_type);
  hash_length_prefixed(hash, family.cpp_scope_type);
  hash_length_prefixed(hash, root_type);
  hash_length_prefixed(hash, std::to_string(container_roles.size()));
  for (const auto &role : container_roles) {
    hash_length_prefixed(hash, role);
  }
  std::ostringstream identity;
  identity << std::hex << hash;
  return identity.str();
}

SchemaRecord::SchemaRecord(StateFamilyKey f, std::string root, std::string rev,
                           std::string desc,
                           std::shared_ptr<const SchemaMessage> m,
                           std::vector<std::string> roles)
    : family(std::move(f)), root_type(std::move(root)),
      revision_id(std::move(rev)), descriptor_set(std::move(desc)),
      container_roles(std::move(roles)), root(std::move(m)),
      accounted_bytes(
          sizeof(SchemaRecord) + descriptor_set.size() +
          family.cpp_state_type.size() + family.cpp_scope_type.size() +
          root_type.size() + revision_id.size() + sizeof(StateFamilyKey) +
          schema_tree_bytes(this->root) +
          std::accumulate(container_roles.begin(), container_roles.end(),
                          std::size_t{},
                          [](auto n, const auto &s) { return n + s.size(); })) {
}

CodecSourceResult
render_state_codec_sources(::tracer::compat::span<const clang::CXXRecordDecl* const> roots,
                           CodecAccess access_policy) {
  const bool snapshot = access_policy == CodecAccess::snapshot_members;
  CodecSourceResult result;
  std::vector<const clang::CXXRecordDecl *> records;
  std::unordered_set<const clang::CXXRecordDecl *> done, active;
  std::function<void(clang::QualType)> gather_type;
  std::function<void(const clang::CXXRecordDecl *)> gather_record;
  auto is_marker = [](clang::QualType t) {
    if (const auto *s = spec(t)) {
      auto n = template_name(s);
      return n == "tracer::state" || n == "tracer::shared_state";
    }
    return false;
  };
  gather_type = [&](clang::QualType t) {
    if (snapshot) {
      if (const auto *array = t->getAsArrayTypeUnsafe()) {
        gather_type(array->getElementType());
        return;
      }
    }
    if (auto *s = spec(t)) {
      auto n = template_name(s);
      if (n == "std::optional" || n == "std::array" || n == "std::vector" ||
          n == "std::deque" || n == "std::list" || n == "std::set" ||
          n == "std::unordered_set") {
        gather_type(arg(s, 0));
        return;
      }
      if (snapshot && owning_snapshot_pointer(n) && snapshot_pointee(arg(s, 0))) {
        gather_type(arg(s, 0));
        return;
      }
      if (snapshot && (n == "std::tuple" || n == "std::variant")) {
        for (const auto &element : s->getTemplateArgs()[0].pack_elements())
          gather_type(element.getAsType());
        return;
      }
      if (n == "std::map" || n == "std::unordered_map" || (snapshot && n == "std::pair")) {
        gather_type(arg(s, 0));
        gather_type(arg(s, 1));
        return;
      }
      if (n.rfind("std::", 0) == 0) {
        return;
      }
    }
    auto *r = t->getAsCXXRecordDecl();
    if (r && r->hasDefinition() &&
        r->getQualifiedNameAsString().rfind("std::", 0) != 0) {
      gather_record(r->getDefinition());
    }
  };
  gather_record = [&](const clang::CXXRecordDecl *r) {
    if (!r || !r->hasDefinition() || done.count(r)) {
      return;
    }
    r = r->getDefinition();
    if (active.count(r)) {
      result.error = "recursive codec type is unsupported";
      return;
    }
    if (r->getQualifiedNameAsString().empty() ||
        r->getDescribedClassTemplate()) {
      result.error = "anonymous or primary-template state class cannot receive "
                     "an automatic codec";
      return;
    }
    active.insert(r);
    for (const auto &base : r->bases()) {
      auto bt = base.getType();
      if (is_marker(bt)) {
        continue;
      }
      auto *br = bt->getAsCXXRecordDecl();
      if (!br || !br->hasDefinition()) {
        result.error = "unsupported state base class";
        return;
      }
      if (!snapshot && base.getAccessSpecifier() != clang::AS_public &&
          !(base.getAccessSpecifier() == clang::AS_none && r->isStruct())) {
        result.error = "non-public state base is unsupported by automatic "
                       "codec generation";
        return;
      }
      gather_record(br->getDefinition());
    }
    for (const auto *f : r->fields()) {
      gather_type(f->getType());
    }
    active.erase(r);
    done.insert(r);
    records.push_back(r);
  };
  for (const auto *root : roots) {
    gather_record(root);
  }
  if (!result.error.empty()) {
    return result;
  }
  std::ostringstream source;
  source << "namespace tracer::context_engine {\n";
  std::size_t next_tag = 0;
  if (snapshot) {
    source << "namespace snapshot_access {\n"
              "template<class Tag, auto Member> struct Grant {\n"
              "  friend constexpr auto member(Tag) { return Member; }\n"
              "};\n"
              "template<class T> T bit_field(const void* object, unsigned offset, unsigned width) {\n"
              "  const auto* bytes = static_cast<const unsigned char*>(object);\n"
              "  std::uint64_t bits = 0;\n"
              "  for (unsigned i = 0; i < width; ++i)\n"
              "    bits |= std::uint64_t((bytes[(offset+i)/8] >> ((offset+i)%8)) & 1) << i;\n"
              "  if constexpr (std::is_enum_v<T>) {\n"
              "    return static_cast<T>(bit_field<std::underlying_type_t<T>>(object, offset, width));\n"
              "  } else if constexpr (std::is_signed_v<T>) {\n"
              "    const auto sign = std::uint64_t(1) << (width-1);\n"
              "    return static_cast<T>(static_cast<std::int64_t>((bits ^ sign) - sign));\n"
              "  } else { return static_cast<T>(bits); }\n"
              "}\n}\n";
  }
  for (const auto *record : records) {
    const auto type_name = record_spelling(record);
    std::ostringstream fields;
    std::unordered_map<const clang::CXXRecordDecl *, bool> seen_bases;
    const auto emit_fields = [&](auto &&emit, const clang::CXXRecordDecl *r,
                                 const std::string &object, bool virtual_base) -> void {
      r = r->getDefinition();
      const auto [previous, inserted] = seen_bases.emplace(r, virtual_base);
      if (!inserted) {
        if (!virtual_base || !previous->second)
          result.error = "repeated non-virtual snapshot base: " + record_spelling(r);
        return;
      }
      for (const auto &base : r->bases()) {
        if (is_marker(base.getType()))
          continue;
        const auto *br = base.getType()->getAsCXXRecordDecl();
        const auto base_name = record_spelling(br);
        if (snapshot) {
          // An otherwise valid derived-to-base reference conversion can
          // ignore base access with the explicit cast syntax.
          emit(emit, br, "((const ::" + base_name + "&)" + object + ")",
               base.isVirtual());
        } else {
          fields << "  if (!Codec<::" << base_name
                 << ">::write_fields(w, static_cast<const ::" << base_name
                 << "&>(value), budget, field_number)) return false;\n";
        }
      }
      for (const auto *field : r->fields()) {
        if (snapshot && field->isUnnamedBitField())
          continue;
        auto access = field->getAccess();
        if (access == clang::AS_none)
          access = r->isStruct() ? clang::AS_public : clang::AS_private;
        const auto name = field->getNameAsString();
        std::string expression = object + "." + name;
        if (snapshot && field->isBitField()) {
          auto &ast = r->getASTContext();
          const auto width = std::min<std::uint64_t>(field->getBitWidthValue(),
                                                     ast.getTypeSize(field->getType()));
          if (!width || width > 64) {
            result.error = "unsupported snapshot bit-field width: " + name;
            return;
          }
          expression = "snapshot_access::bit_field<" + canonical(field->getType()) +
                       ">(__builtin_addressof(" + object + "), " +
                       std::to_string(ast.getASTRecordLayout(r).getFieldOffset(field->getFieldIndex())) +
                       ", " + std::to_string(width) + ")";
        } else if (access != clang::AS_public) {
          if (!snapshot || field->getType()->isReferenceType()) {
            result.error = "non-public member cannot be automatically serialized: " + name;
            return;
          }
          const auto tag = "Tag" + std::to_string(next_tag++);
          // Access is checked on uses of the resulting pointer. The
          // explicit instantiation itself can name a private member.
          source << "namespace snapshot_access {\nstruct " << tag
                 << " { friend constexpr auto member(" << tag << "); };\n"
                 << "template struct Grant<" << tag << ", &::" << record_spelling(r)
                 << "::" << name << ">;\n}\n";
          expression = object + ".*member(snapshot_access::" + tag + "{})";
        }
        fields << "  if (!encode_value(w, field_number++, " << expression
               << ", budget)) return false;\n";
      }
    };
    emit_fields(emit_fields, record, "value", false);
    if (!result.error.empty())
      return result;
    source << "template <> struct Codec<::" << type_name << "> {\n"
           << " static bool write_fields(BoundedProtoWriter& w, const ::"
           << type_name
           << "& value, EncodeBudget& budget, std::uint32_t& field_number) "
              "noexcept {\n" << fields.str();
    source << "  return true;\n }\n"
           << " static bool write(BoundedProtoWriter& w, std::uint32_t field, "
              "const ::"
           << type_name << "& value, EncodeBudget& budget) noexcept {\n"
           << "  if (budget.max_items > 4096 || !w.begin_message(field)) "
              "return false; std::uint32_t field_number=1; const bool "
              "ok=write_fields(w,value,budget,field_number); return "
              "w.end_message() && ok;\n }\n};\n";
  }
  source << "}\n";
  result.source = source.str();
  return result;
}

CodecSourceResult render_state_codec_source(const clang::CXXRecordDecl &root) {
  const clang::CXXRecordDecl *roots[] = {&root};
  return render_state_codec_sources(roots);
}

CodecSourceResult
render_context_codec_sources(clang::ASTContext& ast,
                             ::tracer::compat::span<const clang::CXXRecordDecl* const> state_roots,
                             ::tracer::compat::span<const clang::QualType> payload_roots,
                             CodecAccess access_policy) {
  std::vector<const clang::CXXRecordDecl *> roots(state_roots.begin(),
                                                  state_roots.end());
  std::unordered_set<const clang::CXXRecordDecl *> seen(roots.begin(),
                                                        roots.end());
  std::function<void(clang::QualType)> visit = [&](clang::QualType type) {
    if (type.isNull()) {
      return;
    }
    if (const auto *array = ast.getAsArrayType(type)) {
      visit(array->getElementType());
      return;
    }
    if (const auto *special = spec(type)) {
      const auto name = template_name(special);
      if (name == "std::optional" || name == "std::array" ||
          name == "std::vector" || name == "std::deque" ||
          name == "std::list" || name == "std::set" ||
          name == "std::unordered_set") {
        visit(arg(special, 0));
        return;
      }
      if (access_policy == CodecAccess::snapshot_members && owning_snapshot_pointer(name) &&
          snapshot_pointee(arg(special, 0))) {
        visit(arg(special, 0));
        return;
      }
      if (access_policy == CodecAccess::snapshot_members &&
          (name == "std::tuple" || name == "std::variant")) {
        for (const auto &element : special->getTemplateArgs()[0].pack_elements())
          visit(element.getAsType());
        return;
      }
      if (name == "std::map" || name == "std::unordered_map" ||
          (access_policy == CodecAccess::snapshot_members && name == "std::pair")) {
        visit(arg(special, 0));
        visit(arg(special, 1));
        return;
      }
      if (name.rfind("std::", 0) == 0) {
        return;
      }
    }
    const auto *record = type->getAsCXXRecordDecl();
    if (!record || !record->hasDefinition() ||
        record->getQualifiedNameAsString().rfind("std::", 0) == 0) {
      return;
    }
    record = record->getDefinition();
    if (seen.insert(record).second) {
      roots.push_back(record);
    }
  };
  for (const auto payload : payload_roots) {
    visit(payload);
  }
  return render_state_codec_sources(roots, access_policy);
}

CodecSourceResult render_pimpl_context_codec_sources(
    clang::ASTContext &ast,
    ::tracer::compat::span<const clang::QualType> payload_roots,
    clang::QualType root_type, std::string_view member_name,
    CodecAccess access_policy) {
  CodecSourceResult result;
  if (access_policy != CodecAccess::snapshot_members || root_type.isNull() ||
      member_name.empty()) {
    result.error = "PImpl snapshot codec requires a snapshot root and member";
    return result;
  }
  const auto *root = root_type->getAsCXXRecordDecl();
  root = root ? root->getDefinition() : nullptr;
  const clang::FieldDecl *member = nullptr;
  if (root)
    for (const auto *field : root->fields())
      if (field->getNameAsString() == member_name) {
        member = field;
        break;
      }
  if (!root || !member || !member->getType()->isPointerType()) {
    result.error = "PImpl snapshot codec member is not a direct raw pointer field";
    return result;
  }
  result = render_context_codec_sources(ast, {}, payload_roots, access_policy);
  if (!result)
    return result;
  const auto root_name = record_spelling(root);
  result.source += "\nnamespace tracer::context_engine::snapshot_access {\n"
                   "struct PImplMemberTag { friend constexpr auto member(PImplMemberTag); };\n"
                   "template struct Grant<PImplMemberTag, &::" + root_name +
                   "::" + std::string(member_name) + ">;\n"
                   "template<class T> decltype(auto) pimpl_member(const T& value) {\n"
                   "  return value.*member(PImplMemberTag{});\n}\n}\n";
  return result;
}

CodecSourceResult render_payload_codec_source(clang::ASTContext &ast,
                                              clang::QualType payload,
                                              CodecAccess access_policy) {
  CodecSourceResult result;
  const auto type = payload.getCanonicalType().getAsString();
  if (type.empty()) {
    result.error = "generic payload type has no canonical spelling";
    return result;
  }
  Builder validator{ast, SchemaLimits{}};
  validator.snapshot = access_policy == CodecAccess::snapshot_members;
  (void)validator.describe(payload, "value", 1, 0);
  if (!validator.error.empty()) {
    result.error = std::move(validator.error);
    return result;
  }
  const clang::QualType payload_roots[] = {payload};
  const auto generated = render_context_codec_sources(ast, {}, payload_roots, access_policy);
  if (!generated) {
    result.error = generated.error;
    return result;
  }
  std::ostringstream source;
  source << generated.source << '\n';
  source << "namespace tracer::context_engine {\ninline bool "
            "encode_generic_payload_root(BoundedProtoWriter& writer, const "
         << type
         << "& payload, EncodeBudget& budget) noexcept { return "
            "encode_value(writer, 1, payload, budget); }\n}\n";
  result.source = source.str();
  return result;
}

std::vector<RevisionField>
query_field_path(::tracer::compat::span<const std::shared_ptr<const SchemaRecord>> revisions,
                 ::tracer::compat::span<const std::string_view> path) {
  std::vector<RevisionField> out;
  out.reserve(revisions.size());
  const StateFamilyKey *family = nullptr;
  for (const auto &r : revisions) {
    if (r) {
      family = &r->family;
      break;
    }
  }
  for (const auto &r : revisions) {
    RevisionField result;
    if (!r || path.empty()) {
      out.push_back(std::move(result));
      continue;
    }
    result.revision_id = r->revision_id;
    if (family && !(r->family == *family)) {
      result.family_matches = false;
      out.push_back(std::move(result));
      continue;
    }
    const SchemaMessage *message = r->root.get();
    const SchemaField *field = nullptr;
    bool valid = true;
    for (std::size_t i = 0; i < path.size(); ++i) {
      if (!message) {
        valid = false;
        break;
      }
      auto f = std::find_if(message->fields.begin(), message->fields.end(),
                            [&](const auto &x) { return x.name == path[i]; });
      if (f == message->fields.end()) {
        valid = false;
        break;
      }
      field = &*f;
      if (i + 1 < path.size()) {
        const SchemaField *nested = field;
        if (nested->kind == SchemaKind::optional ||
            nested->kind == SchemaKind::array ||
            nested->kind == SchemaKind::vector ||
            nested->kind == SchemaKind::deque ||
            nested->kind == SchemaKind::list ||
            nested->kind == SchemaKind::set ||
            nested->kind == SchemaKind::unordered_set) {
          nested = nested->value.get();
        } else if (nested->kind == SchemaKind::map ||
                   nested->kind == SchemaKind::unordered_map) {
          nested = nested->value.get();
        }
        if (!nested || nested->kind != SchemaKind::message) {
          valid = false;
          break;
        }
        message = nested->message.get();
      }
    }
    if (valid && field) {
      result.present = true;
      result.kind = field->kind;
      result.cpp_type = field->cpp_type;
      result.optional = field->optional;
      result.repeated = field->repeated;
    }
    out.push_back(std::move(result));
  }
  return out;
}

SchemaResult build_state_schema(clang::ASTContext &ast,
                                const clang::CXXRecordDecl &decl,
                                StateFamilyKey family, SchemaLimits limits) {
  SchemaResult result;
  if (family.cpp_state_type.size() > limits.name_bytes ||
      family.cpp_scope_type.size() > limits.name_bytes) {
    result.error = "state-family type name exceeds limit";
    return result;
  }
  if (std::none_of(family.source_lineage_id.begin(),
                   family.source_lineage_id.end(),
                   [](auto b) { return b != 0; })) {
    result.error = "state-family source lineage UUID is missing";
    return result;
  }
  if (family.cpp_state_type != decl.getQualifiedNameAsString()) {
    result.error = "state-family C++ state type does not match its declaration";
    return result;
  }
  std::unordered_set<const clang::CXXRecordDecl *> seen_scopes;
  std::unordered_set<std::string> scopes;
  std::function<void(const clang::CXXRecordDecl *)> find_scope =
      [&](const clang::CXXRecordDecl *r) {
        if (!r || !seen_scopes.insert(r).second) {
          return;
        }
        for (const auto &base : r->bases()) {
          auto bt = base.getType();
          if (const auto *sp = spec(bt)) {
            auto n = template_name(sp);
            if (n == "tracer::state" || n == "tracer::shared_state") {
              auto t = arg(sp, 0);
              if (!t.isNull()) {
                scopes.insert(canonical(t));
              }
              continue;
            }
          }
          if (auto *br = bt->getAsCXXRecordDecl(); br && br->hasDefinition()) {
            find_scope(br);
          }
        }
      };
  find_scope(&decl);
  if (scopes.size() != 1 || *scopes.begin() != family.cpp_scope_type) {
    std::ostringstream message;
    message << "state-family scope type does not match its marker (expected '"
            << family.cpp_scope_type << "', found ";
    if (scopes.empty()) {
      message << "none";
    } else {
      bool first = true;
      for (const auto &scope : scopes) {
        if (!first) {
          message << ", ";
        }
        message << '\'' << scope << '\'';
        first = false;
      }
    }
    message << ')';
    result.error = message.str();
    return result;
  }
  Builder b{ast, limits};
  auto *f = b.files.add_file();
  f->set_name("state_schema.proto");
  f->set_package("tracer.generated.state");
  f->set_syntax("proto3");
  b.file = f;
  auto root = b.message(decl.getDefinition(), 0);
  if (!b.error.empty()) {
    result.error = std::move(b.error);
    return result;
  }
  std::vector<std::string> roles;
  std::function<void(const SchemaField &, const std::string &)> collect_roles;
  std::function<void(const SchemaMessage &, const std::string &)>
      collect_message;
  collect_message = [&](const SchemaMessage &m, const std::string &prefix) {
    for (const auto &field : m.fields) {
      const auto path = prefix.empty()
                            ? std::to_string(field.number)
                            : prefix + "." + std::to_string(field.number);
      collect_roles(field, path);
    }
  };
  collect_roles = [&](const SchemaField &field, const std::string &path) {
    std::string role;
    switch (field.kind) {
    case SchemaKind::optional:
      role = "optional";
      break;
    case SchemaKind::array:
      role = "array:" + std::to_string(field.fixed_count);
      break;
    case SchemaKind::vector:
      role = "vector";
      break;
    case SchemaKind::map:
      role = "map";
      break;
    case SchemaKind::deque:
      role = "deque";
      break;
    case SchemaKind::list:
      role = "list";
      break;
    case SchemaKind::set:
      role = "set";
      break;
    case SchemaKind::unordered_set:
      role = "unordered_set";
      break;
    case SchemaKind::unordered_map:
      role = "unordered_map";
      break;
    default:
      break;
    }
    if (!role.empty()) {
      roles.push_back(path + "=" + role);
    }
    if (field.message) {
      collect_message(*field.message, path);
    }
    if (field.value) {
      collect_roles(*field.value, path + "/value");
    }
    if (field.key) {
      collect_roles(*field.key, path + "/key");
    }
  };
  collect_message(*root, "");
  std::sort(roles.begin(), roles.end());
  if (std::adjacent_find(roles.begin(), roles.end()) != roles.end()) {
    result.error = "duplicate generated container role";
    return result;
  }
  DescriptorEmitter emitter{*f};
  emitter.emit(*root);
  const auto root_name = emitter.message_name(root->cpp_type);
  const auto root_full_name = "tracer.generated.state." + root_name;
  std::size_t message_count = 0, field_count = 0;
  for (const auto &file : b.files.file()) {
    if (b.files.file_size() > static_cast<int>(limits.files)) {
      result.error = "descriptor file limit exceeded";
      return result;
    }
    for (const auto &m : file.message_type()) {
      ++message_count;
      field_count += m.field_size();
    }
  }
  if (message_count > limits.messages || field_count > limits.fields) {
    result.error = "generated descriptor count limit exceeded";
    return result;
  }
  std::string bytes;
  if (!b.files.SerializeToString(&bytes)) {
    result.error = "descriptor serialization failed";
    return result;
  }
  if (bytes.size() > limits.descriptor_bytes) {
    result.error = "descriptor bundle exceeds limit";
    return result;
  }
  google::protobuf::DescriptorPool validation_pool;
  for (const auto &file : b.files.file()) {
    if (!validation_pool.BuildFile(file)) {
      result.error = "generated descriptor is invalid";
      return result;
    }
  }
  if (!validation_pool.FindMessageTypeByName(root_full_name)) {
    result.error = "generated root descriptor is missing";
    return result;
  }
  const auto id =
      compute_schema_revision_id(bytes, family, root_full_name, roles);
  result.record = std::make_shared<const SchemaRecord>(
      std::move(family), root_full_name, id, std::move(bytes), std::move(root),
      std::move(roles));
  return result;
}

namespace {
SchemaResult build_payload_schema_impl(clang::ASTContext &ast,
                                       clang::QualType payload,
                                       StateFamilyKey family,
                                       SchemaLimits limits,
                                       CodecAccess access_policy,
                                       std::string_view pimpl_member,
                                       clang::QualType implementation_type,
                                       clang::QualType projection_type) {
  SchemaResult result;
  const auto payload_type = canonical_with_template_arguments(payload);
  if (payload.isNull() || payload_type.empty()) {
    result.error = "generic payload type is empty";
    return result;
  }
  if (family.cpp_state_type != payload_type) {
    result.error = "payload family type does not match canonical payload type";
    return result;
  }
  if (family.cpp_state_type.size() > limits.name_bytes ||
      family.cpp_scope_type.empty() ||
      family.cpp_scope_type.size() > limits.name_bytes) {
    result.error = "payload family name is missing or exceeds limit";
    return result;
  }
  if (std::none_of(family.source_lineage_id.begin(),
                   family.source_lineage_id.end(),
                   [](auto b) { return b != 0; })) {
    result.error = "payload family source lineage UUID is missing";
    return result;
  }
  Builder b{ast, limits};
  b.snapshot = access_policy == CodecAccess::snapshot_members;
  auto *file = b.files.add_file();
  file->set_name("state_schema.proto");
  file->set_package("tracer.generated.state");
  file->set_syntax("proto3");
  b.file = file;
  auto root = std::make_shared<SchemaMessage>();
  root->cpp_type = "__tracer_payload_root<" + payload_type + ">";
  root->full_name = root->cpp_type;
  auto value = b.describe(payload, "value", 1, 0);
  if (!pimpl_member.empty()) {
    const std::string snapshot_field = std::string(pimpl_member) + "_snapshot";
    const auto *root_record = payload->getAsCXXRecordDecl();
    root_record = root_record ? root_record->getDefinition() : nullptr;
    const auto *implementation = implementation_type->getAsCXXRecordDecl();
    implementation = implementation ? implementation->getDefinition() : nullptr;
    if (!root_record || !implementation) {
      result.error = "PImpl snapshot requires a record root, implementation, and non-colliding member name";
      return result;
    }
    const clang::FieldDecl *member = nullptr;
    for (const auto *field : root_record->fields())
      if (field->getNameAsString() == pimpl_member) {
        member = field;
        break;
      }
    if (!member || !member->getType()->isPointerType() ||
        member->getType().isVolatileQualified() ||
        member->getType()->getPointeeType().isVolatileQualified()) {
      result.error = "PImpl snapshot member is not a direct raw pointer field: " +
                     std::string(pimpl_member);
      return result;
    }
    const auto *member_base =
        member->getType()->getPointeeType()->getAsCXXRecordDecl();
    member_base = member_base ? member_base->getDefinition() : nullptr;
    clang::CXXBasePaths paths;
    if (!member_base || !member_base->isPolymorphic() ||
        (implementation->getCanonicalDecl() != member_base->getCanonicalDecl() &&
         (!implementation->isDerivedFrom(member_base, paths) ||
          paths.isAmbiguous(ast.getCanonicalType(ast.getRecordType(member_base)))))) {
      result.error = "PImpl implementation is not a polymorphic pointee-compatible record";
      return result;
    }
    if (!value.message ||
        std::any_of(value.message->fields.begin(), value.message->fields.end(),
                    [&](const auto &field) { return field.name == snapshot_field; })) {
      result.error = "PImpl snapshot field collides with an existing root field";
      return result;
    }
    auto extended = std::make_shared<SchemaMessage>(*value.message);
    extended->cpp_type += ".__tracer_pimpl_snapshot";
    extended->full_name = extended->cpp_type;
    extended->snapshot_cpp_type = payload_type;
    std::uint32_t number = 1;
    for (const auto &field : extended->fields)
      number = std::max(number, field.number + 1);
    auto projection = b.describe(projection_type, snapshot_field, number, 1);
    if (projection.message) {
      auto projected_message =
          std::make_shared<SchemaMessage>(*projection.message);
      projected_message->cpp_type += ".__tracer_pimpl_projection";
      projected_message->full_name = projected_message->cpp_type;
      projected_message->snapshot_cpp_type =
          canonical_with_template_arguments(implementation_type);
      projection.message = std::move(projected_message);
    }
    extended->fields.push_back(std::move(projection));
    value.message = std::move(extended);
  }
  root->fields.push_back(std::move(value));
  if (!b.error.empty()) {
    result.error = std::move(b.error);
    return result;
  }
  std::vector<std::string> roles;
  std::function<void(const SchemaField &, const std::string &)> collect_roles;
  std::function<void(const SchemaMessage &, const std::string &)>
      collect_message;
  collect_message = [&](const SchemaMessage &m, const std::string &prefix) {
    for (const auto &field : m.fields) {
      const auto path = prefix.empty()
                            ? std::to_string(field.number)
                            : prefix + "." + std::to_string(field.number);
      collect_roles(field, path);
    }
  };
  collect_roles = [&](const SchemaField &field, const std::string &path) {
    std::string role;
    switch (field.kind) {
    case SchemaKind::optional:
      role = "optional";
      break;
    case SchemaKind::array:
      role = "array:" + std::to_string(field.fixed_count);
      break;
    case SchemaKind::vector:
      role = "vector";
      break;
    case SchemaKind::map:
      role = "map";
      break;
    case SchemaKind::deque:
      role = "deque";
      break;
    case SchemaKind::list:
      role = "list";
      break;
    case SchemaKind::set:
      role = "set";
      break;
    case SchemaKind::unordered_set:
      role = "unordered_set";
      break;
    case SchemaKind::unordered_map:
      role = "unordered_map";
      break;
    default:
      break;
    }
    if (!role.empty()) {
      roles.push_back(path + "=" + role);
    }
    if (field.message) {
      collect_message(*field.message, path);
    }
    if (field.value) {
      collect_roles(*field.value, path + "/value");
    }
    if (field.key) {
      collect_roles(*field.key, path + "/key");
    }
  };
  collect_message(*root, "");
  std::sort(roles.begin(), roles.end());
  if (std::adjacent_find(roles.begin(), roles.end()) != roles.end()) {
    result.error = "duplicate generated container role";
    return result;
  }
  DescriptorEmitter emitter{*file, b.snapshot};
  emitter.emit(*root);
  const auto root_name = emitter.message_name(root->cpp_type);
  const auto root_full = "tracer.generated.state." + root_name;
  std::size_t messages = 0, fields = 0;
  for (const auto &f : b.files.file()) {
    if (b.files.file_size() > static_cast<int>(limits.files)) {
      result.error = "descriptor file limit exceeded";
      return result;
    }
    for (const auto &m : f.message_type()) {
      ++messages;
      fields += m.field_size();
    }
  }
  if (messages > limits.messages || fields > limits.fields) {
    result.error = "generated descriptor count limit exceeded";
    return result;
  }
  std::string bytes;
  if (!b.files.SerializeToString(&bytes)) {
    result.error = "descriptor serialization failed";
    return result;
  }
  if (bytes.size() > limits.descriptor_bytes) {
    result.error = "descriptor bundle exceeds limit";
    return result;
  }
  google::protobuf::DescriptorPool pool;
  for (const auto &f : b.files.file()) {
    if (!pool.BuildFile(f)) {
      result.error = "generated payload descriptor is invalid";
      return result;
    }
  }
  if (!pool.FindMessageTypeByName(root_full)) {
    result.error = "generated payload root is missing";
    return result;
  }
  const auto id = compute_schema_revision_id(bytes, family, root_full, roles);
  result.record = std::make_shared<const SchemaRecord>(
      std::move(family), root_full, id, std::move(bytes), std::move(root),
      std::move(roles));
  return result;
}
} // namespace

SchemaResult build_payload_schema(clang::ASTContext &ast,
                                  clang::QualType payload,
                                  StateFamilyKey family, SchemaLimits limits,
                                  CodecAccess access_policy) {
  return build_payload_schema_impl(ast, payload, std::move(family), limits,
                                   access_policy, {}, {}, {});
}

SchemaResult build_pimpl_payload_schema(clang::ASTContext &ast,
                                        clang::QualType payload,
                                        StateFamilyKey family,
                                        std::string_view member_name,
                                        clang::QualType implementation_type,
                                        SchemaLimits limits,
                                        CodecAccess access_policy) {
  if (member_name.empty() || implementation_type.isNull())
    return {{}, "PImpl snapshot member and implementation type are required"};
  return build_payload_schema_impl(ast, payload, std::move(family), limits,
                                   access_policy, member_name,
                                   implementation_type, implementation_type);
}

SchemaResult build_pimpl_payload_schema(clang::ASTContext &ast,
                                        clang::QualType payload,
                                        StateFamilyKey family,
                                        std::string_view member_name,
                                        clang::QualType implementation_type,
                                        clang::QualType projection_type,
                                        SchemaLimits limits,
                                        CodecAccess access_policy) {
  if (member_name.empty() || implementation_type.isNull() ||
      projection_type.isNull())
    return {{}, "PImpl snapshot member and projection types are required"};
  return build_payload_schema_impl(ast, payload, std::move(family), limits,
                                   access_policy, member_name,
                                   implementation_type, projection_type);
}
} // namespace tracer::context_engine

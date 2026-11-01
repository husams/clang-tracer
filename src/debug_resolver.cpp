#include "debug_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#endif

namespace tracer::client {
namespace {
namespace api = clangtracer::v1;
using llvm::DWARFDie;
namespace dw = llvm::dwarf;

std::string name(DWARFDie d) {
  const char* n = d ? d.getName(llvm::DINameKind::ShortName) : nullptr;
  return n ? n : "";
}

std::uint64_t number(DWARFDie d, dw::Attribute a, std::uint64_t fallback = 0) {
  const auto v = d.findRecursively({a});
  return v ? v->getAsUnsignedConstant().value_or(fallback) : fallback;
}

DWARFDie type_of(DWARFDie d) {
  const auto v = d.findRecursively({dw::DW_AT_type});
  return v ? d.getAttributeValueAsReferencedDie(*v) : DWARFDie{};
}

std::string qualified(DWARFDie d) {
  std::vector<std::string> parts;
  for (unsigned depth = 0; d && depth < 64; ++depth, d = d.getParent()) {
    const auto tag = d.getTag();
    if (tag == dw::DW_TAG_namespace || tag == dw::DW_TAG_class_type ||
        tag == dw::DW_TAG_structure_type || tag == dw::DW_TAG_subprogram) {
      if (name(d).empty()) {
        return {};
      }
      parts.push_back(name(d));
    }
  }
  std::reverse(parts.begin(), parts.end());
  std::string out;
  for (const auto& p : parts) {
    if (!out.empty()) {
      out += "::";
    }
    out += p;
  }
  return out;
}

std::string compact(const std::string& text) {
  std::string out;
  bool space = false;
  for (unsigned char c : text) {
    if (std::isspace(c)) {
      space = true;
      continue;
    }
    // Whitespace around punctuation is optional; whitespace separating two
    // identifier tokens is significant (unsigned long != unsignedlong).
    const auto identifier = [](unsigned char value) {
      return std::isalnum(value) || value == '_';
    };
    if (space && !out.empty() &&
        identifier(static_cast<unsigned char>(out.back())) && identifier(c)) {
      out += ' ';
    }
    out += c;
    space = false;
  }
  return out;
}

struct Type {
  std::string spelling;
  std::uint64_t size = 0;
  api::ValueType value = api::VALUE_TYPE_UNSPECIFIED;
  DWARFDie record;
  bool reference = false;
};

Type read_type(DWARFDie d, unsigned depth = 0) {
  if (!d) {
    return {"void", 0, api::VOID, {}, false};
  }
  if (depth > 32) {
    throw std::runtime_error("debug type nesting exceeds 32");
  }
  switch (d.getTag()) {
  case dw::DW_TAG_typedef:
    return read_type(type_of(d), depth + 1);
  case dw::DW_TAG_const_type: {
    auto t = read_type(type_of(d), depth + 1);
    t.spelling = t.value == api::POINTER ? t.spelling + " const" : "const " + t.spelling;
    return t;
  }
  case dw::DW_TAG_volatile_type: {
    auto t = read_type(type_of(d), depth + 1);
    t.spelling += " volatile";
    return t;
  }
  case dw::DW_TAG_pointer_type: {
    auto t = read_type(type_of(d), depth + 1);
    if (number(d, dw::DW_AT_byte_size, 8) != 8) {
      throw std::runtime_error("non-LP64 debug pointer");
    }
    return {t.spelling + "*", 8, api::POINTER, t.record, false};
  }
  case dw::DW_TAG_reference_type:
  case dw::DW_TAG_rvalue_reference_type: {
    auto t = read_type(type_of(d), depth + 1);
    return {t.spelling + (d.getTag() == dw::DW_TAG_reference_type ? "&" : "&&"), 8, api::POINTER,
            t.record, true};
  }
  case dw::DW_TAG_class_type:
  case dw::DW_TAG_structure_type:
    return {qualified(d), number(d, dw::DW_AT_byte_size), api::OBJECT, d, false};
  case dw::DW_TAG_base_type:
    break;
  default:
    throw std::runtime_error("unsupported debug type: " + name(d));
  }
  Type t{name(d), number(d, dw::DW_AT_byte_size), api::VALUE_TYPE_UNSPECIFIED, {}, false};
  static const std::set<std::string> supported = {"bool",
                                                  "char",
                                                  "signed char",
                                                  "unsigned char",
                                                  "wchar_t",
                                                  "short",
                                                  "short int",
                                                  "unsigned short",
                                                  "unsigned short int",
                                                  "int",
                                                  "unsigned int",
                                                  "long",
                                                  "long int",
                                                  "unsigned long",
                                                  "long unsigned int",
                                                  "unsigned long int",
                                                  "long long",
                                                  "long long int",
                                                  "unsigned long long",
                                                  "long long unsigned int",
                                                  "unsigned long long int",
                                                  "float",
                                                  "double"};
  if (!supported.count(t.spelling)) {
    throw std::runtime_error("unsupported debug builtin: " + t.spelling);
  }
  const auto enc = number(d, dw::DW_AT_encoding);
  if (enc == dw::DW_ATE_signed && t.size == 4) {
    t.value = api::INT32;
  }
  if (enc == dw::DW_ATE_signed && t.size == 8) {
    t.value = api::INT64;
  }
  if (enc == dw::DW_ATE_unsigned && t.size == 8) {
    t.value = api::UINT64;
  }
  if (enc == dw::DW_ATE_float && t.size == 4) {
    t.value = api::FLOAT32;
  }
  if (enc == dw::DW_ATE_float && t.size == 8) {
    t.value = api::FLOAT64;
  }
  return t;
}

void describe_type_chain(DWARFDie type,
                         google::protobuf::RepeatedPtrField<api::DebugTypeLevel>* chain,
                         unsigned depth = 0) {
  if (depth > 32) throw std::runtime_error("debug field type nesting exceeds 32");
  bool is_const = false, is_volatile = false;
  unsigned qualifiers = 0;
  while (type && (type.getTag() == dw::DW_TAG_typedef ||
                  type.getTag() == dw::DW_TAG_const_type ||
                  type.getTag() == dw::DW_TAG_volatile_type)) {
    if (++qualifiers > 32) throw std::runtime_error("debug field qualifiers exceed 32");
    is_const |= type.getTag() == dw::DW_TAG_const_type;
    is_volatile |= type.getTag() == dw::DW_TAG_volatile_type;
    type = type_of(type);
  }
  auto* level = chain->Add();
  level->set_is_const(is_const);
  level->set_is_volatile(is_volatile);
  if (!type) { level->set_kind(api::DebugTypeLevel::VOID); return; }
  if (number(type, dw::DW_AT_address_class))
    throw std::runtime_error("nondefault debug address space is unsupported");
  if (type.getTag() == dw::DW_TAG_pointer_type) {
    level->set_kind(api::DebugTypeLevel::POINTER);
    level->set_byte_size(number(type, dw::DW_AT_byte_size, 8));
    describe_type_chain(type_of(type), chain, depth + 1);
  } else if (type.getTag() == dw::DW_TAG_base_type) {
    level->set_kind(api::DebugTypeLevel::BUILTIN);
    level->set_byte_size(number(type, dw::DW_AT_byte_size));
    level->set_dwarf_encoding(number(type, dw::DW_AT_encoding));
  } else {
    throw std::runtime_error("unsupported debug field type chain");
  }
}

DWARFDie declaration(DWARFDie d) {
  for (unsigned i = 0; d && i < 32; ++i) {
    auto next = d.getAttributeValueAsReferencedDie(dw::DW_AT_specification);
    if (!next) {
      next = d.getAttributeValueAsReferencedDie(dw::DW_AT_abstract_origin);
    }
    if (!next) {
      return d;
    }
    d = next;
  }
  return {};
}

std::vector<std::string> modules(std::uint32_t pid) {
  std::set<std::string> paths;
  const auto proc = "/proc/" + std::to_string(pid);
  paths.insert(std::filesystem::read_symlink(proc + "/exe").string());
  std::ifstream maps(proc + "/maps");
  std::string line;
  while (std::getline(maps, line)) {
    std::istringstream in(line);
    std::string range, perms, offset, dev, inode, path;
    in >> range >> perms >> offset >> dev >> inode;
    std::getline(in >> std::ws, path);
    if (perms.find('x') == std::string::npos || path.empty() || path[0] != '/' ||
        path.ends_with(" (deleted)")) {
      continue;
    }
    const auto base = std::filesystem::path(path).filename().string();
    bool runtime = false;
    for (const char* prefix :
         {"libc.", "ld-linux", "libpthread", "libdl.", "librt.", "libm.", "libstdc++", "libgcc_s",
          "libclang", "libLLVM", "libprotobuf", "libgrpc", "libabsl"}) {
      if (base.starts_with(prefix)) {
        runtime = true;
      }
    }
    if (!runtime) {
      paths.insert(path);
    }
    if (paths.size() > 256) {
      throw std::runtime_error("debug discovery exceeds 256 mapped images");
    }
  }
  return {paths.begin(), paths.end()};
}

void identity(api::DebugInfo& info, const std::string& path) {
  struct stat st{};
  if (::stat(path.c_str(), &st) != 0) {
    throw std::runtime_error("cannot stat debug image: " + path);
  }
  info.set_module(path);
  info.set_device(st.st_dev);
  info.set_inode(st.st_ino);
  info.set_file_size(st.st_size);
#if defined(__APPLE__)
  info.set_modification_ns(st.st_mtimespec.tv_sec * 1000000000LL + st.st_mtimespec.tv_nsec);
  info.set_change_ns(st.st_ctimespec.tv_sec * 1000000000LL + st.st_ctimespec.tv_nsec);
#else
  info.set_modification_ns(st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec);
  info.set_change_ns(st.st_ctim.tv_sec * 1000000000LL + st.st_ctim.tv_nsec);
#endif
}

// Translate through the ELF segment's file offset, then the matching live
// executable mapping. This handles PIE/DSO relocation and ET_EXEC without
// assuming that a mapping's start or file offset equals the ELF load base.
std::uint64_t runtime_entry(std::uint32_t pid, const llvm::object::ELF64LEObjectFile& object,
                            const api::DebugInfo& info, std::uint64_t low, std::uint64_t high) {
  const auto& elf = object.getELFFile();
  const auto kind = elf.getHeader().e_type;
  if (kind != llvm::ELF::ET_EXEC && kind != llvm::ELF::ET_DYN) {
    throw std::runtime_error("debug image is not an executable or shared object");
  }
  auto programs = elf.program_headers();
  if (!programs) {
    throw std::runtime_error("invalid ELF program headers: " +
                             llvm::toString(programs.takeError()));
  }
  std::set<std::uint64_t> file_offsets;
  for (const auto& p : *programs) {
    if (p.p_type != llvm::ELF::PT_LOAD || !(p.p_flags & llvm::ELF::PF_X) || low < p.p_vaddr ||
        low - p.p_vaddr >= p.p_filesz || high - low > p.p_filesz - (low - p.p_vaddr)) {
      continue;
    }
    if (p.p_offset > info.file_size() || p.p_filesz > info.file_size() - p.p_offset) {
      throw std::runtime_error("executable ELF segment exceeds debug image");
    }
    file_offsets.insert(p.p_offset + (low - p.p_vaddr));
  }
  if (file_offsets.empty()) {
    throw std::runtime_error("DWARF code range is outside executable ELF segments");
  }
  std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
  if (!maps) {
    throw std::runtime_error("cannot read target executable mappings");
  }
  std::set<std::uint64_t> entries;
  std::string line;
  while (std::getline(maps, line)) {
    std::istringstream in(line);
    std::string range, permissions, offset, device, inode;
    if (!(in >> range >> permissions >> offset >> device >> inode) || permissions.size() < 3 ||
        permissions[2] != 'x' || std::stoull(inode) != info.inode()) {
      continue;
    }
    const auto colon = device.find(':');
    const auto dash = range.find('-');
    if (colon == std::string::npos || dash == std::string::npos) {
      throw std::runtime_error("invalid target mapping identity");
    }
#if defined(__linux__)
    if (std::stoull(device.substr(0, colon), nullptr, 16) !=
            major(static_cast<dev_t>(info.device())) ||
        std::stoull(device.substr(colon + 1), nullptr, 16) !=
            minor(static_cast<dev_t>(info.device()))) {
      continue;
    }
#else
    throw std::runtime_error("DWARF entry relocation requires Linux");
#endif
    const auto begin = std::stoull(range.substr(0, dash), nullptr, 16);
    const auto end = std::stoull(range.substr(dash + 1), nullptr, 16);
    const auto mapped_offset = std::stoull(offset, nullptr, 16);
    if (end <= begin) {
      throw std::runtime_error("invalid target mapping range");
    }
    for (const auto file_offset : file_offsets) {
      if (file_offset < mapped_offset || file_offset - mapped_offset >= end - begin) {
        continue;
      }
      const auto entry = begin + (file_offset - mapped_offset);
      if (high - low > end - entry || (kind == llvm::ELF::ET_EXEC && entry != low)) {
        continue;
      }
      entries.insert(entry);
    }
  }
  if (entries.size() != 1) {
    throw std::runtime_error("DWARF entry has no unique identity-matched executable mapping");
  }
  return *entries.begin();
}

void describe_record(DWARFDie record, api::DebugInfo& info) {
  if (record.getTag() != dw::DW_TAG_class_type && record.getTag() != dw::DW_TAG_structure_type) {
    throw std::runtime_error("unsupported debug receiver");
  }
  if (record.getParent().getTag() == dw::DW_TAG_class_type ||
      record.getParent().getTag() == dw::DW_TAG_structure_type) {
    throw std::runtime_error("nested debug receiver classes are unsupported");
  }
  info.set_record_name(qualified(record));
  info.set_record_size(number(record, dw::DW_AT_byte_size));
  info.set_receiver_volatile(false);
  if (info.record_name().empty() || !info.record_size()) {
    throw std::runtime_error("incomplete receiver debug information");
  }
  for (auto member : record.children()) {
    if (member.getTag() == dw::DW_TAG_inheritance) {
      throw std::runtime_error("debug receiver inheritance is unsupported");
    }
    if (member.getTag() != dw::DW_TAG_member) {
      continue;
    }
    if (member.find(dw::DW_AT_bit_size)) {
      throw std::runtime_error("debug receiver bitfields are unsupported");
    }
    auto location = member.find(dw::DW_AT_data_member_location);
    if (!location) {
      continue; // Static member, not object storage.
    }
    auto offset = location->getAsUnsignedConstant();
    if (!offset || name(member).empty() || info.fields_size() == 128) {
      throw std::runtime_error("unsupported debug member location");
    }
    auto t = read_type(type_of(member));
    if (!t.size || *offset > info.record_size() || t.size > info.record_size() - *offset) {
      throw std::runtime_error("debug field is outside the receiver layout");
    }
    if (t.record) {
      throw std::runtime_error("nested object fields are unsupported");
    }
    auto* f = info.add_fields();
    f->set_name(name(member));
    f->set_builtin_type(t.spelling);
    describe_type_chain(type_of(member), f->mutable_type_chain());
    f->set_byte_offset(*offset);
    f->set_byte_size(t.size);
    const auto access = number(member, dw::DW_AT_accessibility,
                               record.getTag() == dw::DW_TAG_class_type ? dw::DW_ACCESS_private
                                                                        : dw::DW_ACCESS_public);
    f->set_access(access == dw::DW_ACCESS_private     ? "private"
                  : access == dw::DW_ACCESS_protected ? "protected"
                                                      : "public");
  }
  std::uint64_t natural_alignment = 1;
  for (const auto& field : info.fields()) {
    natural_alignment = std::max(natural_alignment, std::min<std::uint64_t>(field.byte_size(), 8));
  }
  const auto declared_alignment = number(record, dw::DW_AT_alignment);
  if (declared_alignment && declared_alignment != natural_alignment) {
    throw std::runtime_error("nonordinary debug record alignment is unsupported");
  }
}
} // namespace

ResolvedCallable resolve_debug_callable(std::uint32_t pid, const std::string& selector) {
  if (selector.empty() || selector.size() > 4096) {
    throw std::runtime_error("invalid debug selector");
  }
  const auto selected = compact(selector);
  const auto paren = selected.find('(');
  const auto selected_name = selected.substr(0, paren);
  std::map<std::string, ResolvedCallable> matches;
  std::string rejected;
  for (const auto& path : modules(pid)) {
    api::DebugInfo before;
    identity(before, path);
    auto binary = llvm::object::createBinary(path);
    if (!binary) {
      llvm::consumeError(binary.takeError());
      continue;
    }
    const auto* object = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(binary->getBinary());
    if (!object || !object->isELF() || object->getArch() != llvm::Triple::x86_64 ||
        object->getBytesInAddress() != 8 || !object->isLittleEndian()) {
      continue;
    }
    auto dwarf = llvm::DWARFContext::create(*object);
    std::size_t visited = 0;
    std::function<void(DWARFDie, unsigned)> visit = [&](DWARFDie die, unsigned depth) {
      if (!die) {
        return;
      }
      if (depth > 128 || ++visited > 1000000) {
        throw std::runtime_error("debug traversal limits exceeded");
      }
      if (die.getTag() == dw::DW_TAG_subprogram) {
        auto decl = declaration(die);
        const auto qname = qualified(decl);
        if (!qname.empty() && (compact(qname) == selected_name || name(decl) == selected_name)) {
          try {
            if (name(decl).starts_with("~") || name(decl) == name(decl.getParent())) {
              throw std::runtime_error("debug constructors/destructors are unsupported");
            }
            if (number(decl, dw::DW_AT_calling_convention, dw::DW_CC_normal) != dw::DW_CC_normal) {
              throw std::runtime_error("nondefault debug calling convention is unsupported");
            }
            if (number(decl, dw::DW_AT_virtuality)) {
              throw std::runtime_error("virtual debug methods are unsupported");
            }
            const char* linkage = die.getName(llvm::DINameKind::LinkageName);
            std::string symbol = linkage ? linkage : name(decl);
            ResolvedCallable found;
            auto* sig = found.spec.mutable_signature();
            auto ret = read_type(type_of(decl));
            if (ret.value == api::VALUE_TYPE_UNSPECIFIED || ret.value == api::OBJECT) {
              throw std::runtime_error("unsupported debug return ABI");
            }
            sig->set_result(ret.value);
            DWARFDie record;
            bool cv_const = false, cv_volatile = false;
            std::vector<std::string> types;
            std::vector<api::DebugArgument> argument_types;
            // Definitions retain parameter names even when their class
            // declaration does not.
            std::vector<DWARFDie> params;
            for (auto c : die.children()) {
              if (c.getTag() == dw::DW_TAG_formal_parameter) {
                params.push_back(c);
              }
            }
            if (params.empty()) {
              for (auto c : decl.children()) {
                if (c.getTag() == dw::DW_TAG_formal_parameter) {
                  params.push_back(c);
                }
              }
            }
            for (auto p : params) {
              if (number(p, dw::DW_AT_artificial) && name(p) == "this") {
                auto t = type_of(p);
                while (t && (t.getTag() == dw::DW_TAG_const_type ||
                             t.getTag() == dw::DW_TAG_volatile_type ||
                             t.getTag() == dw::DW_TAG_typedef)) {
                  t = type_of(t);
                }
                if (!t || t.getTag() != dw::DW_TAG_pointer_type) {
                  throw std::runtime_error("invalid debug this pointer");
                }
                t = type_of(t);
                while (t && (t.getTag() == dw::DW_TAG_const_type ||
                             t.getTag() == dw::DW_TAG_volatile_type ||
                             t.getTag() == dw::DW_TAG_typedef)) {
                  cv_const |= t.getTag() == dw::DW_TAG_const_type;
                  cv_volatile |= t.getTag() == dw::DW_TAG_volatile_type;
                  t = type_of(t);
                }
                record = t;
                continue;
              }
              if (number(p, dw::DW_AT_artificial)) {
                throw std::runtime_error("unsupported artificial debug parameter");
              }
              auto t = read_type(type_of(p));
              if (t.value == api::VALUE_TYPE_UNSPECIFIED || t.value == api::VOID ||
                  types.size() == 8) {
                throw std::runtime_error("unsupported debug parameter ABI");
              }
              api::DebugArgument argument;
              argument.set_index(types.size());
              argument.set_cpp_type(t.spelling);
              argument.set_is_reference(t.reference);
              argument.set_is_object_value(t.value == api::OBJECT);
              if (t.record) {
                if (t.value == api::OBJECT &&
                    number(t.record, dw::DW_AT_calling_convention) != dw::DW_CC_pass_by_value) {
                  throw std::runtime_error("by-value class arguments require explicit DWARF "
                                           "pass-by-value ABI evidence");
                }
                describe_record(t.record, *argument.mutable_object_type());
              }
              argument_types.push_back(argument);
              sig->add_arguments(t.value);
              types.push_back(t.spelling);
              found.spec.add_argument_names(
                  name(p).empty() ? "arg" + std::to_string(types.size() - 1) : name(p));
            }
            for (auto c : decl.children()) {
              if (c.getTag() == dw::DW_TAG_unspecified_parameters) {
                throw std::runtime_error("variadic debug callable is unsupported");
              }
            }
            sig->set_kind(record ? api::METHOD : api::FUNCTION);
            std::string signature = qname + "(";
            for (std::size_t i = 0; i < types.size(); ++i) {
              if (i) {
                signature += ", ";
              }
              signature += types[i];
            }
            signature += ")";
            if (cv_const) {
              signature += " const";
            }
            if (cv_volatile) {
              signature += " volatile";
            }
            if (paren != std::string::npos && compact(signature) != selected) {
              return;
            }
            auto* info = found.spec.mutable_debug_info();
            *info = before;
            std::uint64_t low = 0, high = 0, section = 0;
            if (!die.getLowAndHighPC(low, high, section) || high <= low) {
              throw std::runtime_error("callable has no concrete contiguous DWARF code range");
            }
            if (auto entry_pc = die.find(dw::DW_AT_entry_pc)) {
              const auto entry = entry_pc->getAsAddress();
              if (!entry || *entry != low) {
                throw std::runtime_error("unsupported DWARF entry_pc");
              }
            }
            info->set_entry_virtual_address(low);
            info->set_entry_size(high - low);
            info->set_runtime_entry(runtime_entry(pid, *object, *info, low, high));
            info->set_selector(signature);
            if (record) {
              describe_record(record, *info);
              info->set_receiver_volatile(cv_volatile);
            }
            for (const auto& argument : argument_types) {
              *info->add_arguments() = argument;
            }
            found.spec.set_symbol(symbol);
            found.declaration = ret.spelling + " " + signature;
            found.source = path;
            api::DebugInfo after;
            identity(after, path);
            if (after.device() != info->device() || after.inode() != info->inode() ||
                after.file_size() != info->file_size() ||
                after.modification_ns() != info->modification_ns() ||
                after.change_ns() != info->change_ns()) {
              throw std::runtime_error("debug image changed while resolving");
            }
            const auto key = path + ":" + std::to_string(low);
            matches[key] = std::move(found);
          } catch (const std::exception& e) {
            rejected = e.what();
          }
        }
      }
      for (auto child : die.children()) {
        visit(child, depth + 1);
      }
    };
    for (const auto& unit : dwarf->compile_units()) {
      visit(unit->getUnitDIE(false), 0);
    }
  }
  if (matches.size() != 1) {
    std::string error = matches.empty()
                            ? "no supported matching callable in mapped ELF debug information; "
                              "build/install matching DWARF symbols"
                            : "ambiguous debug callable; specify its full signature";
    if (!rejected.empty()) {
      error += ": " + rejected;
    }
    for (const auto& [key, c] : matches) {
      error += "\n  " + c.declaration;
    }
    throw std::runtime_error(error);
  }
  return std::move(matches.begin()->second);
}
} // namespace tracer::client

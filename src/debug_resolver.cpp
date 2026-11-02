#include "debug_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/SHA256.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/stat.h>

#include "tracer/compat.hpp"
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

std::vector<std::string> modules(std::uint32_t pid, bool include_runtime = false) {
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
        ::tracer::compat::ends_with(path, " (deleted)")) {
      continue;
    }
    const auto base = std::filesystem::path(path).filename().string();
    bool runtime = false;
    for (const char* prefix : {"libc-", "libdl-", "librt-", "libm-", "ld-", "libc.", "ld-linux",
                               "libpthread", "libdl.", "librt.", "libm.", "libstdc++", "libgcc_s",
                               "libclang", "libLLVM", "libprotobuf", "libgrpc", "libabsl"}) {
      if (::tracer::compat::starts_with(base, prefix)) {
        runtime = true;
      }
    }
    if (!runtime || include_runtime) {
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

struct ExecutableMapping {
  std::string path;
  std::string raw;
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t offset = 0;
  std::uint64_t device_major = 0;
  std::uint64_t device_minor = 0;
  std::uint64_t inode = 0;
};

std::vector<ExecutableMapping> read_executable_mappings(
    std::uint32_t pid, const std::vector<std::string>& image_paths) {
  const std::set<std::string> path_set(image_paths.begin(), image_paths.end());
  std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
  if (!maps) {
    throw std::runtime_error("cannot read target executable mappings");
  }
  std::vector<ExecutableMapping> result;
  std::string line;
  while (std::getline(maps, line)) {
    std::istringstream in(line);
    std::string range, permissions, offset, device, inode, path;
    if (!(in >> range >> permissions >> offset >> device >> inode) ||
        permissions.size() < 3 || permissions[2] != 'x') {
      continue;
    }
    std::getline(in >> std::ws, path);
    if (!::tracer::compat::contains(path_set, path)) {
      continue;
    }
    const auto dash = range.find('-');
    const auto colon = device.find(':');
    if (dash == std::string::npos || colon == std::string::npos) {
      continue;
    }
    try {
      ExecutableMapping mapping;
      mapping.path = path;
      mapping.raw = line;
      mapping.begin = std::stoull(range.substr(0, dash), nullptr, 16);
      mapping.end = std::stoull(range.substr(dash + 1), nullptr, 16);
      mapping.offset = std::stoull(offset, nullptr, 16);
      mapping.device_major = std::stoull(device.substr(0, colon), nullptr, 16);
      mapping.device_minor = std::stoull(device.substr(colon + 1), nullptr, 16);
      mapping.inode = std::stoull(inode);
      if (mapping.end > mapping.begin) {
        result.push_back(std::move(mapping));
      }
    } catch (...) {
      continue;
    }
    if (result.size() > 65536) {
      throw std::runtime_error("executable mapping count exceeds 65536");
    }
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return left.raw < right.raw;
  });
  return result;
}

std::string mapping_fingerprint(
    const std::vector<std::string>& image_paths,
    const std::vector<ExecutableMapping>& executable_mappings) {
  llvm::SHA256 hash;
  std::uint64_t image_bytes = 0;
  constexpr std::uint64_t kMaxMappedImageBytes = 2ULL * 1024 * 1024 * 1024;
  for (const auto& path : image_paths) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
      throw std::runtime_error("cannot stat mapped debug image");
    }
    const auto file_size = static_cast<std::uint64_t>(st.st_size);
    if (file_size > kMaxMappedImageBytes - image_bytes) {
      throw std::runtime_error("symbol discovery image budget exceeded (2 GiB)");
    }
    image_bytes += file_size;
    std::string identity = path;
    identity.push_back('\0');
    identity += std::to_string(st.st_dev) + ":" + std::to_string(st.st_ino) + ":" +
                std::to_string(st.st_size) + ":";
#if defined(__APPLE__)
    identity += std::to_string(st.st_mtimespec.tv_sec) + ":" +
                std::to_string(st.st_mtimespec.tv_nsec) + ":" +
                std::to_string(st.st_ctimespec.tv_sec) + ":" +
                std::to_string(st.st_ctimespec.tv_nsec);
#else
    identity += std::to_string(st.st_mtim.tv_sec) + ":" +
                std::to_string(st.st_mtim.tv_nsec) + ":" +
                std::to_string(st.st_ctim.tv_sec) + ":" +
                std::to_string(st.st_ctim.tv_nsec);
#endif
    identity.push_back('\0');
    hash.update(identity);
  }

  for (const auto& mapping : executable_mappings) {
    std::string record = mapping.raw;
    record.push_back('\0');
    hash.update(record);
  }

  const auto digest = hash.final();
  std::ostringstream fingerprint;
  for (const auto byte : digest) {
    fingerprint << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(byte);
  }
  return fingerprint.str();
}

struct ExecutableSegment {
  std::uint64_t virtual_address = 0;
  std::uint64_t file_offset = 0;
  std::uint64_t file_size = 0;
};

std::vector<ExecutableSegment> executable_segments(
    const llvm::object::ELF64LEObjectFile& object) {
  std::vector<ExecutableSegment> result;
  auto programs = object.getELFFile().program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    return result;
  }
  for (const auto& program : *programs) {
    if (program.p_type == llvm::ELF::PT_LOAD &&
        (program.p_flags & llvm::ELF::PF_X) != 0 && program.p_filesz != 0 &&
        program.p_offset <= std::numeric_limits<std::uint64_t>::max() - program.p_filesz &&
        program.p_vaddr <= std::numeric_limits<std::uint64_t>::max() - program.p_filesz) {
      result.push_back(
          {program.p_vaddr, program.p_offset, program.p_filesz});
    }
  }
  return result;
}

struct MappedExecutableRange {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
};

std::vector<MappedExecutableRange> mapped_executable_ranges(
    const llvm::object::ELF64LEObjectFile& object,
    const api::DebugInfo& info,
    const std::vector<ExecutableSegment>& segments,
    const std::vector<ExecutableMapping>& mappings) {
  std::vector<MappedExecutableRange> result;
  const auto kind = object.getELFFile().getHeader().e_type;
  if (kind != llvm::ELF::ET_EXEC && kind != llvm::ELF::ET_DYN) {
    return result;
  }
  for (const auto& segment : segments) {
    const auto segment_end = segment.file_offset + segment.file_size;
    for (const auto& mapping : mappings) {
      if (mapping.path != info.module() || mapping.inode != info.inode()) {
        continue;
      }
#if defined(__linux__)
      if (mapping.device_major != major(static_cast<dev_t>(info.device())) ||
          mapping.device_minor != minor(static_cast<dev_t>(info.device()))) {
        continue;
      }
#else
      continue;
#endif
      const auto mapping_size = mapping.end - mapping.begin;
      if (mapping.offset > std::numeric_limits<std::uint64_t>::max() - mapping_size) {
        continue;
      }
      const auto mapping_end = mapping.offset + mapping_size;
      const auto file_begin = std::max(segment.file_offset, mapping.offset);
      const auto file_end = std::min(segment_end, mapping_end);
      if (file_end <= file_begin) {
        continue;
      }
      const auto virtual_begin =
          segment.virtual_address + (file_begin - segment.file_offset);
      const auto runtime_begin = mapping.begin + (file_begin - mapping.offset);
      if (kind == llvm::ELF::ET_EXEC && runtime_begin != virtual_begin) {
        continue;
      }
      result.push_back(
          {virtual_begin, virtual_begin + (file_end - file_begin)});
    }
  }
  return result;
}

bool contains_code_range(const std::vector<MappedExecutableRange>& ranges,
                         std::uint64_t low, std::uint64_t high) {
  return high > low &&
         std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) {
           return low >= range.begin && high <= range.end;
         });
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

struct CallableShape {
  api::Signature signature;
  DWARFDie receiver;
  bool receiver_const = false;
  bool receiver_volatile = false;
  std::string result_spelling;
  std::string selector;
  std::vector<api::DebugArgument> arguments;
  std::vector<std::string> argument_names;
};

CallableShape describe_callable(DWARFDie die, DWARFDie decl) {
  if (::tracer::compat::starts_with(name(decl), "~") || name(decl) == name(decl.getParent())) {
    throw std::runtime_error("debug constructors/destructors are unsupported");
  }
  if (number(decl, dw::DW_AT_calling_convention, dw::DW_CC_normal) != dw::DW_CC_normal) {
    throw std::runtime_error("nondefault debug calling convention is unsupported");
  }
  if (number(decl, dw::DW_AT_virtuality)) {
    throw std::runtime_error("virtual debug methods are unsupported");
  }

  CallableShape result;
  const auto return_type = read_type(type_of(decl));
  if (return_type.value == api::VALUE_TYPE_UNSPECIFIED || return_type.value == api::OBJECT) {
    throw std::runtime_error("unsupported debug return ABI");
  }
  result.signature.set_result(return_type.value);
  result.result_spelling = return_type.spelling;

  std::vector<DWARFDie> params;
  for (auto child : die.children()) {
    if (child.getTag() == dw::DW_TAG_formal_parameter) {
      params.push_back(child);
    }
  }
  if (params.empty()) {
    for (auto child : decl.children()) {
      if (child.getTag() == dw::DW_TAG_formal_parameter) {
        params.push_back(child);
      }
    }
  }
  std::vector<std::string> types;
  for (auto parameter : params) {
    if (number(parameter, dw::DW_AT_artificial) && name(parameter) == "this") {
      auto type = type_of(parameter);
      while (type && (type.getTag() == dw::DW_TAG_const_type ||
                      type.getTag() == dw::DW_TAG_volatile_type ||
                      type.getTag() == dw::DW_TAG_typedef)) {
        type = type_of(type);
      }
      if (!type || type.getTag() != dw::DW_TAG_pointer_type) {
        throw std::runtime_error("invalid debug this pointer");
      }
      type = type_of(type);
      while (type && (type.getTag() == dw::DW_TAG_const_type ||
                      type.getTag() == dw::DW_TAG_volatile_type ||
                      type.getTag() == dw::DW_TAG_typedef)) {
        result.receiver_const |= type.getTag() == dw::DW_TAG_const_type;
        result.receiver_volatile |= type.getTag() == dw::DW_TAG_volatile_type;
        type = type_of(type);
      }
      result.receiver = type;
      continue;
    }
    if (number(parameter, dw::DW_AT_artificial)) {
      throw std::runtime_error("unsupported artificial debug parameter");
    }
    const auto type = read_type(type_of(parameter));
    if (type.value == api::VALUE_TYPE_UNSPECIFIED || type.value == api::VOID ||
        types.size() == 8) {
      throw std::runtime_error("unsupported debug parameter ABI");
    }
    api::DebugArgument argument;
    argument.set_index(types.size());
    argument.set_cpp_type(type.spelling);
    argument.set_is_reference(type.reference);
    argument.set_is_object_value(type.value == api::OBJECT);
    if (type.record) {
      if (type.value == api::OBJECT &&
          number(type.record, dw::DW_AT_calling_convention) != dw::DW_CC_pass_by_value) {
        throw std::runtime_error(
            "by-value class arguments require explicit DWARF pass-by-value ABI evidence");
      }
      describe_record(type.record, *argument.mutable_object_type());
    }
    result.arguments.push_back(std::move(argument));
    result.signature.add_arguments(type.value);
    types.push_back(type.spelling);
    result.argument_names.push_back(
        name(parameter).empty() ? "arg" + std::to_string(types.size() - 1)
                                : name(parameter));
  }
  for (auto child : decl.children()) {
    if (child.getTag() == dw::DW_TAG_unspecified_parameters) {
      throw std::runtime_error("variadic debug callable is unsupported");
    }
  }
  result.signature.set_kind(result.receiver ? api::METHOD : api::FUNCTION);
  result.selector = qualified(decl) + "(";
  for (std::size_t i = 0; i < types.size(); ++i) {
    if (i) {
      result.selector += ", ";
    }
    result.selector += types[i];
  }
  result.selector += ")";
  if (result.receiver_const) {
    result.selector += " const";
  }
  if (result.receiver_volatile) {
    result.selector += " volatile";
  }
  return result;
}

std::pair<std::uint64_t, std::uint64_t> callable_code_range(DWARFDie die) {
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
  return {low, high};
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
            if (::tracer::compat::starts_with(name(decl), "~") ||
                name(decl) == name(decl.getParent())) {
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
            const auto shape = describe_callable(die, decl);
            if (paren != std::string::npos && compact(shape.selector) != selected) {
              return;
            }
            found.spec.mutable_signature()->CopyFrom(shape.signature);
            for (const auto& argument_name : shape.argument_names) {
              found.spec.add_argument_names(argument_name);
            }
            auto* info = found.spec.mutable_debug_info();
            *info = before;
            const auto [low, high] = callable_code_range(die);
            info->set_entry_virtual_address(low);
            info->set_entry_size(high - low);
            info->set_runtime_entry(runtime_entry(pid, *object, *info, low, high));
            info->set_selector(shape.selector);
            if (shape.receiver) {
              describe_record(shape.receiver, *info);
              info->set_receiver_volatile(shape.receiver_volatile);
            }
            for (const auto& argument : shape.arguments) {
              *info->add_arguments() = argument;
            }
            found.spec.set_symbol(symbol);
            found.declaration = shape.result_spelling + " " + shape.selector;
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

DebugSymbolPage list_debug_symbols(
    std::uint32_t pid, const std::string& query, const std::string& module,
    std::size_t page_size, const std::string& after_selector,
    const std::string& after_linkage_symbol, const std::string& after_module_path,
    const std::string& expected_mapping_fingerprint,
    const std::function<bool()>& is_cancelled) {
  if (page_size == 0 || page_size > 500) {
    throw std::invalid_argument("symbol page size must be between 1 and 500");
  }
  const auto image_paths = modules(pid);
  const auto mappings = read_executable_mappings(pid, image_paths);
  const auto current_fingerprint = mapping_fingerprint(image_paths, mappings);
  if (!expected_mapping_fingerprint.empty() &&
      expected_mapping_fingerprint != current_fingerprint) {
    throw std::invalid_argument("mapped images changed; restart symbol listing");
  }

  using Key = std::tuple<std::string, std::string, std::string>;
  const Key after{after_selector, after_linkage_symbol, after_module_path};
  std::map<Key, DebugSymbol> candidates;
  std::size_t visited = 0;
  constexpr std::size_t kMaxVisitedDies = 2'000'000;
  bool cancelled = false;
  for (const auto& path : image_paths) {
    if (is_cancelled()) {
      cancelled = true;
      break;
    }
    if (!module.empty() && path.find(module) == std::string::npos) {
      continue;
    }
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
    api::DebugInfo module_identity;
    identity(module_identity, path);
    const auto segments = executable_segments(*object);
    std::vector<ExecutableMapping> module_mappings;
    std::copy_if(mappings.begin(), mappings.end(), std::back_inserter(module_mappings),
                 [&](const auto& mapping) { return mapping.path == path; });
    const auto mapped_ranges =
        mapped_executable_ranges(*object, module_identity, segments, module_mappings);
    auto dwarf = llvm::DWARFContext::create(*object);
    std::function<void(DWARFDie, unsigned)> visit = [&](DWARFDie die, unsigned depth) {
      if (!die || cancelled) {
        return;
      }
      if (depth > 128 || ++visited > kMaxVisitedDies) {
        throw std::runtime_error(
            "symbol discovery work limit exceeded; narrow --module and retry");
      }
      if ((visited & 1023U) == 0 && is_cancelled()) {
        cancelled = true;
        return;
      }
      if (die.getTag() == dw::DW_TAG_subprogram) {
        const auto decl = declaration(die);
        try {
          if (qualified(decl).empty()) {
            throw std::runtime_error("callable has no qualified name");
          }
          const auto shape = describe_callable(die, decl);
          const auto [low, high] = callable_code_range(die);
          if (!contains_code_range(mapped_ranges, low, high)) {
            throw std::runtime_error("callable is outside mapped executable image ranges");
          }
          // The resolver validates the receiver layout too; discovery must not
          // advertise a method that the existing probe path cannot admit.
          if (shape.receiver) {
            api::DebugInfo receiver_info;
            describe_record(shape.receiver, receiver_info);
          }
          const auto& selector = shape.selector;
          if (selector.size() > 4096) {
            throw std::runtime_error("callable selector exceeds 4096 bytes");
          }
          if (path.size() > 4096) {
            throw std::runtime_error("mapped module path exceeds 4096 bytes");
          }
          const char* linkage = die.getName(llvm::DINameKind::LinkageName);
          const std::string linkage_symbol = linkage ? linkage : name(decl);
          if (linkage_symbol.empty() || linkage_symbol.size() > 4096) {
            throw std::runtime_error("callable linkage name exceeds 4096 bytes");
          }
          if (selector.find(query) != std::string::npos ||
              linkage_symbol.find(query) != std::string::npos) {
            const Key key{selector, linkage_symbol, path};
            if (key > after) {
              candidates.try_emplace(
                  key, DebugSymbol{selector, linkage_symbol, path});
              if (candidates.size() > page_size + 1) {
                candidates.erase(std::prev(candidates.end()));
              }
            }
          }
        } catch (const std::exception&) {
          // Omit unsupported or incomplete callables; list only selectors
          // that the existing DWARF resolver can accept.
        }
      }
      for (auto child : die.children()) {
        visit(child, depth + 1);
        if (cancelled) {
          return;
        }
      }
    };
    for (const auto& unit : dwarf->compile_units()) {
      if (is_cancelled()) {
        cancelled = true;
        break;
      }
      visit(unit->getUnitDIE(false), 0);
      if (cancelled) {
        break;
      }
    }
    if (cancelled) {
      break;
    }
  }

  if (!cancelled) {
    const auto current_paths = modules(pid);
    const auto current_mappings = read_executable_mappings(pid, current_paths);
    if (current_paths != image_paths ||
        mapping_fingerprint(current_paths, current_mappings) != current_fingerprint) {
      throw std::invalid_argument("mapped images changed during discovery; restart listing");
    }
  }

  DebugSymbolPage page;
  page.mapping_fingerprint = current_fingerprint;
  page.cancelled = cancelled;
  page.has_more = candidates.size() > page_size;
  auto end = candidates.end();
  if (page.has_more) {
    --end;
  }
  for (auto item = candidates.begin(); item != end; ++item) {
    page.symbols.push_back(item->second);
  }
  return page;
}
} // namespace tracer::client

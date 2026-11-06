#include "debug_resolver.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/CRC.h>
#include <llvm/Support/Endian.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/SHA256.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/stat.h>

#include "tracer/compat.hpp"
#include "linux_syscalls.hpp"
#include "mapped_path.hpp"

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
  for (; d; d = d.getParent()) {
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
  if (enc == dw::DW_ATE_boolean && t.size == 1) {
    t.value = api::BOOL;
  }
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
  const auto executable = tracer::detail::canonical_mapped_path(proc + "/exe");
  if (executable.empty()) {
    throw std::runtime_error("cannot resolve target executable path");
  }
  paths.insert(executable);
  std::ifstream maps(proc + "/maps");
  std::string line;
  while (std::getline(maps, line)) {
    std::istringstream in(line);
    std::string range, perms, offset, dev, inode, path;
    in >> range >> perms >> offset >> dev >> inode;
    std::getline(in >> std::ws, path);
    if (perms.find('x') == std::string::npos || path.empty() || path[0] != '/') {
      continue;
    }
    path = tracer::detail::canonical_mapped_path(path);
    if (path.empty()) {
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
  }
  return {paths.begin(), paths.end()};
}

void identity(api::DebugInfo& info, const std::string& path) {
  struct stat st{};
  if (::stat(path.c_str(), &st) != 0) {
    throw std::runtime_error("cannot stat debug image: " + path);
  }
  info.set_module(tracer::detail::canonical_mapped_path(path));
  info.set_file_size(st.st_size);
}

struct ExecutableMapping {
  std::string path;
  std::string raw;
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t offset = 0;
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
    path = tracer::detail::canonical_mapped_path(path);
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
      if (mapping.end > mapping.begin) {
        result.push_back(std::move(mapping));
      }
    } catch (...) {
      continue;
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
  for (const auto& path : image_paths) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
      throw std::runtime_error("cannot stat mapped debug image");
    }
    std::string identity = path;
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
      if (mapping.path != info.module()) {
        continue;
      }
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
                            const api::DebugInfo& info, std::uint64_t low, std::uint64_t high,
                            std::uint64_t* resolved_file_offset = nullptr) {
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
        permissions[2] != 'x') {
      continue;
    }
    std::string path;
    std::getline(in >> std::ws, path);
    if (tracer::detail::canonical_mapped_path(path) != info.module()) {
      continue;
    }
    const auto dash = range.find('-');
    if (dash == std::string::npos) {
      throw std::runtime_error("invalid target mapping range");
    }
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
      if (resolved_file_offset)
        *resolved_file_offset = file_offset;
    }
  }
  if (entries.size() != 1) {
    throw std::runtime_error("DWARF entry has no unique path-matched executable mapping");
  }
  return *entries.begin();
}

bool matches_process_bytes(std::uint32_t pid, std::uint64_t address,
                           std::string_view expected) {
  if (expected.size() > std::numeric_limits<std::uint64_t>::max() - address)
    return false;
  std::array<char, 4096> buffer;
  while (!expected.empty()) {
    const auto size = std::min(expected.size(), buffer.size());
    iovec local{buffer.data(), size};
    iovec remote{reinterpret_cast<void *>(address), size};
    if (linux_api::process_vm_readv(pid, &local, 1, &remote, 1, 0) !=
            static_cast<ssize_t>(size) ||
        std::string_view(buffer.data(), size) != expected.substr(0, size))
      return false;
    address += size;
    expected.remove_prefix(size);
  }
  return true;
}

std::optional<bool> matches_mapped_build_id(
    std::uint32_t pid, const llvm::object::ELF64LEObjectFile &object,
    std::uint64_t load_bias) {
  const auto id = llvm::object::getBuildID(&object);
  if (id.empty())
    return std::nullopt;
  // LLVM returns a view of the note descriptor in this ELF buffer.
  const auto *image = reinterpret_cast<const std::uint8_t *>(
      object.getMemoryBufferRef().getBufferStart());
  const auto offset = static_cast<std::uint64_t>(id.data() - image);
  auto programs = object.getELFFile().program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    return std::nullopt;
  }
  bool loadable = false;
  for (const auto &segment : *programs) {
    if (segment.p_type != llvm::ELF::PT_LOAD || offset < segment.p_offset)
      continue;
    const auto delta = offset - segment.p_offset;
    const auto limit = std::numeric_limits<std::uint64_t>::max() - load_bias;
    if (delta > segment.p_filesz || id.size() > segment.p_filesz - delta ||
        segment.p_vaddr > limit || delta > limit - segment.p_vaddr)
      continue;
    loadable = true;
    if (matches_process_bytes(pid, load_bias + segment.p_vaddr + delta,
                              {reinterpret_cast<const char *>(id.data()), id.size()}))
      return true;
  }
  return loadable ? std::optional<bool>(false) : std::nullopt;
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
  const auto record_calling_convention =
      number(record, dw::DW_AT_calling_convention, 0);
  info.set_record_pass_by_reference(
      record_calling_convention == dw::DW_CC_pass_by_reference);
  info.set_record_calling_convention(
      record_calling_convention == dw::DW_CC_pass_by_value
          ? api::RECORD_PASS_BY_VALUE
          : (record_calling_convention == dw::DW_CC_pass_by_reference
                 ? api::RECORD_PASS_BY_REFERENCE
                 : api::RECORD_CALLING_CONVENTION_UNSPECIFIED));
  info.set_receiver_volatile(false);
  if (info.record_name().empty())
    throw std::runtime_error("incomplete debug record identity");
  bool opaque = !info.record_size();
  for (auto member : record.children()) {
    if (member.getTag() == dw::DW_TAG_inheritance) {
      opaque = true;
      continue;
    }
    if (member.getTag() != dw::DW_TAG_member) {
      continue;
    }
    // Static data members have no object offset. Ignore only declarations
    // explicitly marked external/declaration; a missing location on an
    // ordinary member means the instance layout is incomplete.
    if (member.find(dw::DW_AT_bit_size)) {
      opaque = true;
      continue;
    }
    auto location = member.find(dw::DW_AT_data_member_location);
    if (!location) {
      if (number(member, dw::DW_AT_external) ||
          number(member, dw::DW_AT_declaration))
        continue;
      opaque = true;
      continue;
    }
    auto offset = location->getAsUnsignedConstant();
    if (!offset || name(member).empty() || info.fields_size() == 128) {
      opaque = true;
      continue;
    }
    Type t;
    try {
      t = read_type(type_of(member));
    } catch (const std::exception &) {
      opaque = true;
      continue;
    }
    if (!t.size || *offset > info.record_size() || t.size > info.record_size() - *offset) {
      opaque = true;
      continue;
    }
    if (t.record) {
      opaque = true;
      continue;
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
  if (opaque)
    info.clear_fields();
  std::uint64_t natural_alignment = 1;
  for (const auto& field : info.fields()) {
    natural_alignment = std::max(natural_alignment, std::min<std::uint64_t>(field.byte_size(), 8));
  }
  const auto declared_alignment = number(record, dw::DW_AT_alignment);
  if (!opaque && declared_alignment && declared_alignment != natural_alignment) {
    throw std::runtime_error("nonordinary debug record alignment is unsupported");
  }
  info.set_record_alignment(declared_alignment
                                ? declared_alignment
                                : (opaque ? 0 : natural_alignment));
  info.set_record_is_opaque(opaque || info.fields_size() == 0);
}

void describe_methods(DWARFDie record, api::DebugInfo &info) {
  std::set<std::string> seen;
  for (auto method_die : record.children()) {
    if (method_die.getTag() != dw::DW_TAG_subprogram)
      continue;
    auto method = declaration(method_die);
    if (name(method) == name(record) ||
        ::tracer::compat::starts_with(name(method), "~"))
      continue;
    const auto accessibility = number(
        method, dw::DW_AT_accessibility,
        record.getTag() == dw::DW_TAG_class_type ? dw::DW_ACCESS_private
                                                 : dw::DW_ACCESS_public);
    const char *linkage = method_die.getName(llvm::DINameKind::LinkageName);
    if (accessibility != dw::DW_ACCESS_public || !linkage || !*linkage ||
        info.methods_size() >= 128 || !seen.insert(linkage).second)
      continue;
    api::DebugMethod described;
    described.set_name(name(method));
    described.set_linkage_symbol(linkage);
    const auto calling_convention =
        number(method, dw::DW_AT_calling_convention, dw::DW_CC_normal);
    described.set_calling_convention(calling_convention);
    bool variadic = false;
    for (auto child : method_die.children())
      variadic |= child.getTag() == dw::DW_TAG_unspecified_parameters;
    described.set_is_variadic(variadic);
    described.set_is_virtual(number(method, dw::DW_AT_virtuality) != 0);
    if (auto return_die = type_of(method)) {
      try {
        described.set_cpp_result_type(read_type(return_die).spelling);
      } catch (const std::exception &) {
        continue;
      }
    } else {
      described.set_cpp_result_type("void");
    }
    bool has_this = false;
    bool unsupported = calling_convention != dw::DW_CC_normal || variadic;
    DWARFDie object_pointer = method_die.getAttributeValueAsReferencedDie(
        dw::DW_AT_object_pointer);
    if (!object_pointer)
      object_pointer = method.getAttributeValueAsReferencedDie(
          dw::DW_AT_object_pointer);
    bool first_formal = true;
    for (auto parameter : method_die.children()) {
      if (parameter.getTag() != dw::DW_TAG_formal_parameter)
        continue;
      const bool is_object_pointer =
          (object_pointer &&
           object_pointer.getOffset() == parameter.getOffset()) ||
          (!object_pointer && first_formal &&
           number(parameter, dw::DW_AT_artificial));
      first_formal = false;
      if (is_object_pointer) {
        has_this = true;
        auto this_type = type_of(parameter);
        while (this_type && (this_type.getTag() == dw::DW_TAG_const_type ||
                             this_type.getTag() == dw::DW_TAG_volatile_type ||
                             this_type.getTag() == dw::DW_TAG_typedef)) {
          described.set_is_const(described.is_const() ||
                                 this_type.getTag() == dw::DW_TAG_const_type);
          described.set_is_volatile(
              described.is_volatile() ||
              this_type.getTag() == dw::DW_TAG_volatile_type);
          this_type = type_of(this_type);
        }
        if (!this_type || this_type.getTag() != dw::DW_TAG_pointer_type) {
          unsupported = true;
          break;
        }
        this_type = type_of(this_type);
        while (this_type && (this_type.getTag() == dw::DW_TAG_const_type ||
                             this_type.getTag() == dw::DW_TAG_volatile_type ||
                             this_type.getTag() == dw::DW_TAG_typedef)) {
          described.set_is_const(described.is_const() ||
                                 this_type.getTag() == dw::DW_TAG_const_type);
          described.set_is_volatile(
              described.is_volatile() ||
              this_type.getTag() == dw::DW_TAG_volatile_type);
          this_type = type_of(this_type);
        }
        continue;
      }
      if (number(parameter, dw::DW_AT_artificial) ||
          described.cpp_parameter_types_size() >= 8) {
        unsupported = true;
        break;
      }
      try {
        described.add_cpp_parameter_types(read_type(type_of(parameter)).spelling);
      } catch (const std::exception &) {
        unsupported = true;
        break;
      }
    }
    described.set_is_static(!has_this);
    described.set_ref_qualifier(
        number(method, dw::DW_AT_rvalue_reference)
            ? "&&"
            : (number(method, dw::DW_AT_reference) ? "&" : ""));
    // Keep unsupported declarations in metadata so the agent can fail closed
    // before either a header declaration or a process symbol can bind them.
    (void)unsupported;
    *info.add_methods() = std::move(described);
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
  if (return_type.value == api::VALUE_TYPE_UNSPECIFIED) {
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
      describe_record(type.record, *argument.mutable_object_type());
      describe_methods(type.record, *argument.mutable_object_type());
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

void resolve_method_symbols(const llvm::object::ELF64LEObjectFile &object,
                            std::uint32_t pid,
                            const api::DebugInfo &module_identity,
                            api::DebugInfo &record) {
  for (auto &method : *record.mutable_methods()) {
    if (method.linkage_symbol().empty())
      continue;
    for (const auto &symbol : object.symbols()) {
      auto symbol_name = symbol.getName();
      if (!symbol_name) {
        llvm::consumeError(symbol_name.takeError());
        continue;
      }
      if (*symbol_name != method.linkage_symbol())
        continue;
      auto symbol_type = symbol.getType();
      if (!symbol_type) {
        llvm::consumeError(symbol_type.takeError());
        continue;
      }
      if (*symbol_type != llvm::object::SymbolRef::ST_Function)
        continue;
      auto address = symbol.getAddress();
      auto size = symbol.getSize();
      if (!address) {
        llvm::consumeError(address.takeError());
        continue;
      }
      if (size == 0 || *address > UINT64_MAX - size)
        continue;
      try {
        method.set_runtime_entry(runtime_entry(pid, object, module_identity,
                                               *address, *address + size));
      } catch (const std::exception &) {
        continue;
      }
      break;
    }
  }
}

bool lists_symbol(const llvm::object::ELF64LEObjectFile &object,
                  const std::string &selected, const std::string &selected_name) {
  const auto matches = [&](const llvm::object::SymbolRef &symbol) {
    auto flags = symbol.getFlags();
    if (!flags) {
      llvm::consumeError(flags.takeError());
      return false;
    }
    if (*flags & llvm::object::SymbolRef::SF_Undefined)
      return false;
    auto symbol_name = symbol.getName();
    if (!symbol_name) {
      llvm::consumeError(symbol_name.takeError());
      return false;
    }
    const auto raw = compact(symbol_name->str());
    const auto demangled = compact(llvm::demangle(symbol_name->str()));
    const auto name = demangled.substr(0, demangled.find('('));
    const auto scope = name.rfind("::");
    return raw == selected || demangled == selected || name == selected_name ||
           (scope != std::string::npos && name.substr(scope + 2) == selected_name);
  };
  for (const auto &symbol : object.symbols())
    if (matches(symbol)) return true;
  for (const auto &symbol : object.getDynamicSymbolIterators())
    if (matches(symbol)) return true;
  return false;
}

std::optional<std::string> separate_debug_file(
    const std::string &path, const llvm::object::ELF64LEObjectFile &object) {
  const auto main_build_id = llvm::object::getBuildID(&object);
  std::string debuglink_name;
  std::optional<std::uint32_t> debuglink_crc;
  for (const auto &section : object.sections()) {
    auto section_name = section.getName();
    if (!section_name || *section_name != ".gnu_debuglink") {
      if (!section_name)
        llvm::consumeError(section_name.takeError());
      continue;
    }
    auto contents = section.getContents();
    if (!contents) {
      llvm::consumeError(contents.takeError());
      break;
    }
    const auto nul = contents->find('\0');
    if (nul == llvm::StringRef::npos || nul + 1 >= contents->size())
      break;
    debuglink_name = contents->substr(0, nul).str();
    std::size_t crc_offset = (nul + 1 + 3) & ~std::size_t(3);
    if (crc_offset > contents->size() ||
        contents->size() - crc_offset < sizeof(std::uint32_t))
      break;
    debuglink_crc = llvm::support::endian::read32le(
        reinterpret_cast<const std::uint8_t *>(contents->data() + crc_offset));
    break;
  }

  std::vector<std::pair<std::string, bool>> candidates;
  const auto module_path = std::filesystem::path(path);
  if (!debuglink_name.empty() && debuglink_crc) {
    candidates.emplace_back(
        (module_path.parent_path() / debuglink_name).string(), false);
    candidates.emplace_back(
        (module_path.parent_path() / ".debug" / debuglink_name).string(), false);
    candidates.emplace_back(
        (std::filesystem::path("/usr/lib/debug") /
         module_path.parent_path().relative_path() / debuglink_name)
            .string(),
        false);
  }
  if (!main_build_id.empty()) {
    std::string build_id;
    for (const auto byte : main_build_id) {
      constexpr char digits[] = "0123456789abcdef";
      build_id.push_back(digits[byte >> 4]);
      build_id.push_back(digits[byte & 0x0f]);
    }
    candidates.emplace_back(
        "/usr/lib/debug/.build-id/" + build_id.substr(0, 2) + "/" +
            build_id.substr(2) + ".debug",
        true);
  }

  for (const auto &[candidate, require_build_id] : candidates) {
    std::ifstream input(candidate, std::ios::binary);
    if (!input)
      continue;
    std::string bytes((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
    if (bytes.empty())
      continue;
    if (debuglink_crc) {
      const auto crc = llvm::crc32(llvm::ArrayRef<std::uint8_t>(
          reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
      if (crc != *debuglink_crc)
        continue;
    }
    auto debug_binary = llvm::object::createBinary(candidate);
    if (!debug_binary) {
      llvm::consumeError(debug_binary.takeError());
      continue;
    }
    const auto *debug_object = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(
        debug_binary->getBinary());
    if (!debug_object || !debug_object->isELF() ||
        debug_object->getArch() != llvm::Triple::x86_64 ||
        !debug_object->isLittleEndian())
      continue;
    const auto debug_build_id = llvm::object::getBuildID(debug_object);
    if (require_build_id && debug_build_id.empty())
      continue;
    if (!main_build_id.empty() && !debug_build_id.empty() &&
        !std::equal(main_build_id.begin(), main_build_id.end(),
                    debug_build_id.begin(), debug_build_id.end()))
      continue;
    return candidate;
  }
  return std::nullopt;
}
} // namespace

std::vector<std::string> list_debug_modules(std::uint32_t pid) {
  return modules(pid);
}

ResolvedCallable bind_prepared_callable(std::uint32_t pid,
                                        const api::PreparedInjector &prepared,
                                        const DebugLookupOptions &options,
                                        const std::function<PreparedCodePatch(
                                            std::uint64_t)> &live_patch) {
  const auto scalar = [](api::ValueType type, bool result) {
    return (result && type == api::VOID) || type == api::INT32 ||
           type == api::INT64 || type == api::UINT64 || type == api::FLOAT32 ||
           type == api::FLOAT64;
  };
  const auto &signature = prepared.signature();
  const auto version = prepared.format_version();
  const bool typed = version == 3;
  const bool pinned = (version == 1 || typed) &&
                      prepared.module_sha256().size() == 32 && !prepared.module_name().empty();
  const bool source_only = (version == 2 || typed) &&
                           prepared.module_sha256().empty() && prepared.module_name().empty();
  const auto supported = [&](api::ValueType type, bool result) {
    return scalar(type, result) ||
           (typed && (type == api::BOOL || type == api::POINTER || type == api::OBJECT));
  };
  if (prepared.ByteSizeLong() > 64 * 1024 || (!pinned && !source_only) ||
      prepared.selector().empty() ||
      prepared.selector().size() > 4096 ||
      prepared.selector().find('\0') != std::string::npos ||
      prepared.symbol().empty() || prepared.symbol().size() > 511 ||
      prepared.symbol().find('\0') != std::string::npos ||
      prepared.module_name().size() > 4095 ||
      prepared.module_name().find_first_of("/\0", 0, 2) != std::string::npos ||
      prepared.target_triple().size() > 256 ||
      prepared.target_triple().find('\0') != std::string::npos ||
      !::tracer::compat::starts_with(prepared.target_triple(), "x86_64-") ||
      prepared.target_triple().find("linux") == std::string::npos ||
      prepared.target_triple().find("gnux32") != std::string::npos ||
      (signature.kind() != api::FUNCTION && !(typed && signature.kind() == api::METHOD)) ||
      !supported(signature.result(), true) ||
      signature.arguments_size() > 8 ||
      prepared.argument_names_size() != signature.arguments_size())
    throw std::invalid_argument(
        "invalid or unsupported prepared injector contract");
  for (int i = 0; i < signature.arguments_size(); ++i) {
    const auto &name = prepared.argument_names(i);
    if (!supported(signature.arguments(i), false) || name.empty() ||
        name.size() > 63 || name.find('\0') != std::string::npos)
      throw std::invalid_argument("invalid prepared argument");
  }
  api::DebugInfo types;
  if (typed) {
    // Only accept source layout and signature facts from the artifact.
    // Live addresses, recovered fields and method entrypoints must never
    // be copied from a package.
    const auto &source = prepared.header_types();
    const auto copy_record = [](const api::DebugInfo &from, api::DebugInfo &to) {
      if (from.record_name().empty() || from.record_size() == 0 ||
          from.record_alignment() == 0 || from.prepared_layout_sha256().size() != 32 ||
          !from.record_is_opaque() ||
          from.record_calling_convention() != (from.record_pass_by_reference()
              ? api::RECORD_PASS_BY_REFERENCE : api::RECORD_PASS_BY_VALUE))
        throw std::invalid_argument("invalid prepared record layout");
      to.set_record_name(from.record_name());
      to.set_record_size(from.record_size());
      to.set_record_alignment(from.record_alignment());
      to.set_record_is_opaque(true);
      to.set_record_pass_by_reference(from.record_pass_by_reference());
      to.set_record_calling_convention(from.record_calling_convention());
      to.set_prepared_layout_sha256(from.prepared_layout_sha256());
    };
    const auto validate_type = [](api::ValueType wire, const std::string &spelling,
                                  bool object, bool reference, bool record) {
      if (spelling.empty() || spelling.size() > 4096 ||
          spelling.find('\0') != std::string::npos ||
          object != (wire == api::OBJECT) || (object && (!record || reference)) ||
          (reference && wire != api::POINTER) || (record && !object && !reference))
        throw std::invalid_argument("invalid prepared C++ type");
    };
    if (!prepared.has_header_context() || prepared.include_headers_size() < 1 ||
        prepared.include_headers_size() > 16 ||
        source.arguments_size() != signature.arguments_size() ||
        source.record_name().empty() == (signature.kind() == api::METHOD))
      throw std::invalid_argument("invalid prepared header metadata");
    if (signature.kind() == api::METHOD) {
      copy_record(source, types);
      types.set_receiver_volatile(source.receiver_volatile());
    }
    validate_type(signature.result(), source.result_cpp_type(),
                  source.result_is_object_value(), source.result_is_reference(),
                  source.has_result_object_type());
    types.set_result_cpp_type(source.result_cpp_type());
    types.set_result_is_reference(source.result_is_reference());
    types.set_result_is_object_value(source.result_is_object_value());
    if (source.has_result_object_type()) {
      copy_record(source.result_object_type(), *types.mutable_result_object_type());
      types.set_result_pass_by_reference(source.result_object_type().record_pass_by_reference());
    }
    for (int i = 0; i < source.arguments_size(); ++i) {
      const auto &from = source.arguments(i);
      validate_type(signature.arguments(i), from.cpp_type(), from.is_object_value(),
                    from.is_reference(), from.has_object_type());
      auto &to = *types.add_arguments();
      to.set_index(i);
      to.set_cpp_type(from.cpp_type());
      to.set_is_reference(from.is_reference());
      to.set_is_object_value(from.is_object_value());
      if (from.has_object_type())
        copy_record(from.object_type(), *to.mutable_object_type());
    }
    if (types.SerializeAsString() != source.SerializeAsString())
      throw std::invalid_argument("prepared metadata contains unexpected fields");
  } else if (prepared.has_header_types() || prepared.has_header_context() ||
             !prepared.include_headers().empty()) {
    throw std::invalid_argument("unexpected header metadata in scalar package");
  }
  std::vector<ResolvedCallable> matches;
  for (const auto &path : modules(pid)) {
    if (options.is_cancelled())
      throw std::runtime_error("prepared lookup cancelled");
    if (!options.module.empty()) {
      if (options.module.front() == '/'
              ? path != options.module
              : path.find(options.module) == std::string::npos)
        continue;
    } else if (pinned && std::filesystem::path(path).filename() !=
                            prepared.module_name()) {
      continue;
    }
    auto binary = llvm::object::createBinary(path);
    if (!binary) {
      llvm::consumeError(binary.takeError());
      continue;
    }
    const auto *object =
        llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(binary->getBinary());
    if (!object || object->getArch() != llvm::Triple::x86_64)
      continue;
    const auto bytes = binary->getBinary()->getMemoryBufferRef().getBuffer();
    if (pinned) {
      const auto digest = llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
          reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
      if (std::string(reinterpret_cast<const char *>(digest.data()),
                      digest.size()) != prepared.module_sha256())
        continue;
    }
    std::set<std::pair<std::uint64_t, std::uint64_t>> ranges;
    const auto visit = [&](const llvm::object::SymbolRef &symbol) {
      auto name = symbol.getName();
      if (!name) {
        llvm::consumeError(name.takeError());
        return;
      }
      if (*name != prepared.symbol())
        return;
      auto flags = symbol.getFlags();
      if (!flags) {
        llvm::consumeError(flags.takeError());
        return;
      }
      auto type = symbol.getType();
      if (!type) {
        llvm::consumeError(type.takeError());
        return;
      }
      if ((*flags & llvm::object::SymbolRef::SF_Undefined) ||
          *type != llvm::object::SymbolRef::ST_Function ||
          llvm::object::ELFSymbolRef(symbol).getELFType() !=
              llvm::ELF::STT_FUNC)
        return;
      auto address = symbol.getAddress();
      if (!address) {
        llvm::consumeError(address.takeError());
        return;
      }
      const auto size = llvm::object::ELFSymbolRef(symbol).getSize();
      if (size && size <= std::numeric_limits<std::uint64_t>::max() - *address)
        ranges.emplace(*address, size);
    };
    for (const auto &symbol : object->symbols())
      visit(symbol);
    for (const auto &symbol : object->getDynamicSymbolIterators())
      visit(symbol);
    if (ranges.empty() && source_only)
      continue;
    if (ranges.size() != 1)
      throw std::runtime_error(
          "prepared function has no unique concrete ELF symbol");

    api::DebugInfo info = types;
    info.set_module(path);
    info.set_file_size(bytes.size());
    const auto [entry, size] = *ranges.begin();
    std::uint64_t file_offset = 0;
    const auto address = runtime_entry(pid, *object, info, entry, entry + size,
                                       &file_offset);
    // Verify against bytes in the running process.
    // For images without a build ID the agent checks the function body, using
    // its saved prefix when replacing one of its own detours.
    if (address < entry)
      throw std::runtime_error("invalid prepared image load bias");
    if (const auto matched = matches_mapped_build_id(pid, *object, address - entry)) {
      if (!*matched)
        throw std::runtime_error("prepared module build ID differs from the live image");
    } else if (live_patch) {
      const auto code = std::string_view(bytes.data() + file_offset, size);
      const auto patch = live_patch(address);
      const auto prefix_size = patch.original.size();
      if (code.substr(0, prefix_size) != patch.original ||
          !matches_process_bytes(pid, address, patch.installed) ||
          !matches_process_bytes(pid, address + prefix_size, code.substr(prefix_size)))
        throw std::runtime_error("prepared function code differs from the live image");
    }
    info.set_entry_virtual_address(entry);
    info.set_entry_size(size);
    info.set_runtime_entry(address);
    info.set_selector(prepared.selector());
    const auto spelling = [](api::ValueType type) -> std::string {
      switch (type) {
      case api::VOID: return "void";
      case api::INT32: return "int";
      case api::INT64: return "long long";
      case api::UINT64: return "unsigned long long";
      case api::FLOAT32: return "float";
      case api::FLOAT64: return "double";
      default: throw std::runtime_error("unsupported prepared scalar type");
      }
    };
    if (!typed) {
      info.set_result_cpp_type(spelling(signature.result()));
      for (int i = 0; i < signature.arguments_size(); ++i) {
        auto *argument = info.add_arguments();
        argument->set_index(i);
        argument->set_cpp_type(spelling(signature.arguments(i)));
      }
    }
    ResolvedCallable found;
    *found.spec.mutable_debug_info() = std::move(info);
    *found.spec.mutable_prepared() = prepared;
    *found.spec.mutable_signature() = signature;
    found.spec.set_symbol(prepared.symbol());
    for (const auto &name : prepared.argument_names())
      found.spec.add_argument_names(name);
    found.declaration = found.spec.debug_info().result_cpp_type() + " " + prepared.selector();
    if (typed) {
      *found.spec.mutable_include_headers() = prepared.include_headers();
      *found.spec.mutable_header_context() = prepared.header_context();
    }
    found.source = path;
    matches.push_back(std::move(found));
  }
  if (matches.empty())
    throw std::runtime_error(pinned
        ? "PREPARED_MODULE_MISMATCH: no mapped module matches the prepared image digest"
        : "prepared symbol was not found in the target's mapped modules: " + prepared.symbol());
  if (matches.size() != 1)
    throw std::runtime_error("prepared module is ambiguous; select an exact --module path");
  return std::move(matches.front());
}

ResolvedCallable resolve_debug_callable(std::uint32_t pid, const std::string& selector,
                                       const DebugLookupOptions &options) {
  if (selector.empty() || selector.find('\0') != std::string::npos) {
    throw std::runtime_error("invalid debug selector");
  }
  const auto selected = compact(selector);
  const auto paren = selected.find('(');
  const auto selected_name = selected.substr(0, paren);
  std::map<std::string, ResolvedCallable> matches;
  std::string rejected;
  std::uint64_t visited = 0;
  const auto check_cancelled = [&] {
    if (options.is_cancelled())
      throw std::runtime_error("debug lookup cancelled");
  };
  for (const auto &path : modules(pid)) {
    check_cancelled();
    if (!options.module.empty() &&
        (options.module.front() == '/' ? path != options.module
                                      : path.find(options.module) == std::string::npos))
      continue;
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
    if (options.module.empty() && !lists_symbol(*object, selected, selected_name))
      continue;
    std::optional<llvm::object::OwningBinary<llvm::object::Binary>>
        separate_debug_binary;
    const llvm::object::ELF64LEObjectFile *dwarf_object = object;
    if (const auto debug_path = separate_debug_file(path, *object)) {
      auto loaded_debug = llvm::object::createBinary(*debug_path);
      if (loaded_debug) {
        const auto *candidate = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(
            loaded_debug->getBinary());
        if (candidate && candidate->isELF() &&
            candidate->getArch() == llvm::Triple::x86_64 &&
            candidate->isLittleEndian()) {
          separate_debug_binary.emplace(std::move(*loaded_debug));
          dwarf_object = candidate;
        }
      } else {
        llvm::consumeError(loaded_debug.takeError());
      }
    }
    auto dwarf = llvm::DWARFContext::create(*dwarf_object);
    const auto visit = [&](DWARFDie die, std::size_t depth) {
      if (!die) {
        return;
      }
      // RPC cancellation checks are expensive. Bound unchecked traversal to
      // 128 entries while retaining checks at work boundaries and completion.
      if ((visited % 128) == 0)
        check_cancelled();
      if (options.max_entries && visited >= options.max_entries) {
        check_cancelled();
        throw DebugLookupLimit("debug entry limit exceeded; increase --debug-max-entries or use 0");
      }
      ++visited;
      if (options.max_depth && depth > options.max_depth) {
        check_cancelled();
        throw DebugLookupLimit("debug depth limit exceeded; increase --debug-max-depth or use 0");
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
              describe_methods(shape.receiver, *info);
              info->set_receiver_volatile(shape.receiver_volatile);
            }
            {
              const auto result_type = read_type(type_of(declaration(die)));
              info->set_result_cpp_type(result_type.spelling);
              info->set_result_is_object_value(result_type.value == api::OBJECT);
              info->set_result_is_reference(result_type.reference);
              if (result_type.record) {
                describe_record(result_type.record,
                                *info->mutable_result_object_type());
                describe_methods(result_type.record,
                                 *info->mutable_result_object_type());
                info->set_result_pass_by_reference(
                    info->result_object_type().record_pass_by_reference());
              }
            }
            for (const auto& argument : shape.arguments) {
              *info->add_arguments() = argument;
            }
            resolve_method_symbols(*object, pid, before, *info);
            for (auto &argument : *info->mutable_arguments())
              if (argument.has_object_type())
                resolve_method_symbols(*object, pid, before,
                                       *argument.mutable_object_type());
            if (info->has_result_object_type())
              resolve_method_symbols(*object, pid, before,
                                     *info->mutable_result_object_type());
            found.spec.set_symbol(symbol);
            found.declaration = shape.result_spelling + " " + shape.selector;
            found.source = path;
            const auto key = path + ":" + std::to_string(low);
            matches[key] = std::move(found);
          } catch (const std::exception& e) {
            rejected = e.what();
          }
        }
      }
    };
    for (const auto& unit : dwarf->compile_units()) {
      check_cancelled();
      std::vector<std::pair<DWARFDie, std::size_t>> pending{{unit->getUnitDIE(false), 0}};
      while (!pending.empty()) {
        const auto [die, depth] = pending.back();
        pending.pop_back();
        visit(die, depth);
        if (die) {
          const auto children_begin = pending.size();
          for (auto child : die.children())
            pending.emplace_back(child, depth + 1);
          std::reverse(pending.begin() + children_begin, pending.end());
        }
      }
    }
  }
  check_cancelled();
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
    std::optional<llvm::object::OwningBinary<llvm::object::Binary>>
        separate_debug_binary;
    const llvm::object::ELF64LEObjectFile *dwarf_object = object;
    if (const auto debug_path = separate_debug_file(path, *object)) {
      auto loaded_debug = llvm::object::createBinary(*debug_path);
      if (loaded_debug) {
        const auto *candidate = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(
            loaded_debug->getBinary());
        if (candidate && candidate->isELF() &&
            candidate->getArch() == llvm::Triple::x86_64 &&
            candidate->isLittleEndian()) {
          separate_debug_binary.emplace(std::move(*loaded_debug));
          dwarf_object = candidate;
        }
      } else {
        llvm::consumeError(loaded_debug.takeError());
      }
    }
    auto dwarf = llvm::DWARFContext::create(*dwarf_object);
    const auto visit = [&](DWARFDie die) {
      if (!die || cancelled) {
        return;
      }
      if (is_cancelled()) {
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
          const char* linkage = die.getName(llvm::DINameKind::LinkageName);
          const std::string linkage_symbol = linkage ? linkage : name(decl);
          if (linkage_symbol.empty()) {
            throw std::runtime_error("callable has no linkage name");
          }
          if (selector.find(query) != std::string::npos ||
              linkage_symbol.find(query) != std::string::npos) {
            const Key key{selector, linkage_symbol, path};
            if (key > after) {
              candidates.try_emplace(
                  key, DebugSymbol{selector, linkage_symbol, path});
              if (page_size != 0 && candidates.size() > page_size &&
                  candidates.size() - page_size > 1) {
                candidates.erase(std::prev(candidates.end()));
              }
            }
          }
        } catch (const std::exception&) {
          // Omit unsupported or incomplete callables; list only selectors
          // that the existing DWARF resolver can accept.
        }
      }
    };
    for (const auto& unit : dwarf->compile_units()) {
      if (is_cancelled()) {
        cancelled = true;
        break;
      }
      // Keep traversal storage on the heap so deep DWARF does not impose a
      // recursion limit. Results are ordered by key, independent of visit order.
      std::vector<DWARFDie> pending{unit->getUnitDIE(false)};
      while (!pending.empty() && !cancelled) {
        const auto die = pending.back();
        pending.pop_back();
        visit(die);
        if (die && !cancelled) {
          for (auto child : die.children()) {
            pending.push_back(child);
          }
        }
      }
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
  page.has_more = page_size != 0 && candidates.size() > page_size;
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

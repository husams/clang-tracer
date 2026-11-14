#include "object_watch_plan.hpp"
#include "object_code_cache_protocol.hpp"
#include <llvm/Support/SHA256.h>
#include <llvm/Support/LEB128.h>

#include "debug_resolver.hpp"
#include "proc_identity.hpp"
#include "linux_syscalls.hpp"
#include "native_detour.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <tuple>
#include <vector>

#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/DebugInfo/DWARF/DWARFLocationExpression.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/DataExtractor.h>

namespace tracer::object_watch {
namespace {
namespace dw = llvm::dwarf;
using llvm::DWARFDie;
using llvm::object::ObjectFile;

auto open_elf(const std::string &path) {
  auto binary = llvm::object::createBinary(path);
  if (!binary)
    throw std::runtime_error("cannot open object watch ELF: " + llvm::toString(binary.takeError()));
  if (!llvm::isa<llvm::object::ELF64LEObjectFile>(binary->getBinary()))
    throw std::runtime_error("object watch requires a 64-bit little-endian ELF");
  return std::move(*binary);
}

std::string name(DWARFDie die) {
  const char *value = die.getShortName();
  return value ? value : "";
}

std::string leaf_name(const std::string &qualified_name) {
  const auto scope = qualified_name.rfind("::");
  return scope == std::string::npos ? qualified_name : qualified_name.substr(scope + 2);
}

std::string qualified(DWARFDie die) {
  if (!die)
    return {};
  std::string result = name(die);
  for (auto parent = die.getParent(); parent; parent = parent.getParent()) {
    if (parent.getTag() == dw::DW_TAG_namespace || parent.getTag() == dw::DW_TAG_class_type ||
        parent.getTag() == dw::DW_TAG_structure_type || parent.getTag() == dw::DW_TAG_union_type) {
      result = name(parent) + "::" + result;
    } else if (parent.getTag() != dw::DW_TAG_compile_unit) {
      return {};
    }
  }
  return result;
}

std::optional<std::uint64_t> number(DWARFDie die, dw::Attribute attribute) {
  if (const auto value = die.find(attribute))
    return value->getAsUnsignedConstant();
  return std::nullopt;
}

DWARFDie type(DWARFDie die) {
  auto target = die.getAttributeValueAsReferencedDie(dw::DW_AT_type);
  while (target && (target.getTag() == dw::DW_TAG_typedef ||
                    target.getTag() == dw::DW_TAG_const_type))
    target = target.getAttributeValueAsReferencedDie(dw::DW_AT_type);
  return target;
}

bool record(DWARFDie die) {
  return die && (die.getTag() == dw::DW_TAG_class_type ||
                 die.getTag() == dw::DW_TAG_structure_type ||
                 die.getTag() == dw::DW_TAG_union_type);
}

std::optional<std::uint64_t> size_of(DWARFDie die) {
  if (!die)
    return std::nullopt;
  const auto size = number(die, dw::DW_AT_byte_size);
  if (!size && die.getTag() == dw::DW_TAG_pointer_type)
    return die.getDwarfUnit()->getAddressByteSize();
  return size;
}

DWARFDie find_record(llvm::DWARFContext &dwarf, const std::string &wanted,
                     llvm::DWARFUnit *preferred = nullptr) {
  std::vector<DWARFDie> pending;
  if (preferred) {
    pending.push_back(preferred->getUnitDIE(false));
  } else {
    for (const auto &unit : dwarf.compile_units())
      pending.push_back(unit->getUnitDIE(false));
  }
  DWARFDie result;
  while (!pending.empty()) {
    const auto die = pending.back();
    pending.pop_back();
    for (const auto child : die.children()) {
      if (record(child) && name(child) == leaf_name(wanted) &&
          qualified(child) == wanted && number(child, dw::DW_AT_byte_size)) {
        if (result)
          throw std::runtime_error("ambiguous DWARF record: " + wanted);
        result = child;
      }
      if (child.getTag() == dw::DW_TAG_namespace)
        pending.push_back(child);
    }
  }
  if (!result && preferred)
    return find_record(dwarf, wanted);
  if (!result)
    throw std::runtime_error("no complete DWARF record: " + wanted);
  return result;
}

struct DerivedTypes {
  bool present = false;
  bool may_construct_base_directly = false;
};

bool has_virtual_bases(DWARFDie die, std::set<DWARFDie> &seen) {
  if (!die || !seen.insert(die).second)
    return false;
  for (const auto base : die.children())
    if (base.getTag() == dw::DW_TAG_inheritance &&
        (number(base, dw::DW_AT_virtuality).value_or(dw::DW_VIRTUALITY_none) != dw::DW_VIRTUALITY_none ||
         has_virtual_bases(type(base), seen)))
      return true;
  return false;
}

bool has_virtual_members_or_bases(DWARFDie die, std::set<DWARFDie> &seen) {
  if (!die || !seen.insert(die).second)
    return false;
  for (const auto member : die.children()) {
    const auto tag = member.getTag();
    if ((tag == dw::DW_TAG_subprogram || tag == dw::DW_TAG_inheritance) &&
        number(member, dw::DW_AT_virtuality).value_or(dw::DW_VIRTUALITY_none) != dw::DW_VIRTUALITY_none)
      return true;
    if (tag == dw::DW_TAG_inheritance && has_virtual_members_or_bases(type(member), seen))
      return true;
  }
  return false;
}

bool may_aggregate_initialize_base(DWARFDie derived) {
  const auto class_name = name(derived);
  const auto constructor = class_name.substr(0, class_name.find('<'));
  const auto default_access = derived.getTag() == dw::DW_TAG_class_type ?
      dw::DW_ACCESS_private : dw::DW_ACCESS_public;
  for (const auto member : derived.children()) {
    const auto tag = member.getTag();
    if (tag == dw::DW_TAG_subprogram && name(member) == constructor &&
        !member.findRecursively({dw::DW_AT_artificial}) &&
        number(member, dw::DW_AT_defaulted) != dw::DW_DEFAULTED_in_class)
      return false;
    const bool instance_member = tag == dw::DW_TAG_member && !member.find(dw::DW_AT_external);
    if ((tag == dw::DW_TAG_inheritance || instance_member) &&
        number(member, dw::DW_AT_accessibility).value_or(default_access) != dw::DW_ACCESS_public)
      return false;
  }
  std::set<DWARFDie> seen;
  return !has_virtual_members_or_bases(derived, seen);
}

DerivedTypes derived_types(llvm::DWARFContext &dwarf, const std::string &concrete) {
  DerivedTypes result;
  std::vector<DWARFDie> pending;
  for (const auto &unit : dwarf.compile_units())
    pending.push_back(unit->getUnitDIE(false));
  while (!pending.empty()) {
    const auto die = pending.back();
    pending.pop_back();
    for (const auto child : die.children()) {
      if (child.getTag() == dw::DW_TAG_inheritance && qualified(type(child)) == concrete) {
        result.present = true;
        if (!result.may_construct_base_directly && may_aggregate_initialize_base(die))
          result.may_construct_base_directly = true;
      }
      if (record(child) || (child.getTag() == dw::DW_TAG_namespace && name(child) != "std"))
        pending.push_back(child);
    }
  }
  return result;
}

std::string storage_spelling(std::string_view type, unsigned depth = 0) {
  if (depth > 12)
    return "?";
  const auto first = type.find_first_not_of(" \t");
  if (first == std::string_view::npos)
    return "?";
  type = type.substr(first, type.find_last_not_of(" \t") - first + 1);
  for (const std::string_view qualifier : {"const ", "volatile ", "class ", "struct "})
    if (type.starts_with(qualifier))
      return storage_spelling(type.substr(qualifier.size()), depth + 1);
  for (const std::string_view qualifier : {" const", " volatile"})
    if (type.ends_with(qualifier))
      return storage_spelling(type.substr(0, type.size() - qualifier.size()), depth + 1);
  if (type.ends_with("*"))
    return "*" + storage_spelling(type.substr(0, type.size() - 1), depth + 1);
  const auto angle = type.find('<');
  if (angle != std::string_view::npos && type.ends_with(">")) {
    std::string result(type.substr(0, angle));
    const auto args = type.substr(angle + 1, type.size() - angle - 2);
    std::size_t start = 0;
    unsigned level = 0;
    for (std::size_t index = 0; index < args.size(); ++index) {
      if (args[index] == '<')
        ++level;
      else if (args[index] == '>' && level)
        --level;
      else if (args[index] == ',' && !level) {
        result += "<" + storage_spelling(args.substr(start, index - start), depth + 1) + ">";
        start = index + 1;
      }
    }
    if (level || args.empty())
      return "?";
    return result + "<" + storage_spelling(args.substr(start), depth + 1) + ">";
  }
  if (type == "char") {
#if defined(__aarch64__)
    return "char";
#else
    return "signed-char";
#endif
  }
  if (type == "signed char") return "signed-char";
  if (type == "unsigned char") return "unsigned-char";
  if (type == "int" || type == "signed int") return "signed32";
  if (type == "unsigned int") return "unsigned32";
  if (type == "long" || type == "long int" || type == "long long int") return "signed64";
  if (type == "long unsigned int" || type == "unsigned long" || type == "long long unsigned int")
    return "unsigned64";
  if (type == "short int") return "signed16";
  if (type == "short unsigned int") return "unsigned16";
  if (type == "float") return "float32";
  if (type == "double") return "float64";
  if (type == "long double") return "float128";
  if (std::all_of(type.begin(), type.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
      }))
    return "#" + std::string(type);
  return std::string(type.starts_with("::") ? type.substr(2) : type);
}

std::string storage_type(DWARFDie die, unsigned depth = 0) {
  if (!die)
    return "void";
  if (depth > 12)
    return "?";
  const auto tag = die.getTag();
  switch (tag) {
  case dw::DW_TAG_typedef:
  case dw::DW_TAG_const_type:
  case dw::DW_TAG_volatile_type:
    return storage_type(type(die), depth + 1);
  case dw::DW_TAG_pointer_type:
  case dw::DW_TAG_reference_type:
  case dw::DW_TAG_rvalue_reference_type:
    return std::string(tag == dw::DW_TAG_pointer_type ? "*" :
                       tag == dw::DW_TAG_reference_type ? "&" : "&&") +
        storage_type(type(die), depth + 1);
  case dw::DW_TAG_base_type: {
#if defined(__aarch64__)
    // GCC may encode plain char differently in a split template DIE.
    // The spelling still distinguishes it from the two explicit char types.
    if (name(die) == "char" && number(die, dw::DW_AT_byte_size) == 1)
      return "char";
#endif
    const auto bits = std::to_string(number(die, dw::DW_AT_byte_size).value_or(0) * 8);
    switch (number(die, dw::DW_AT_encoding).value_or(0)) {
    case dw::DW_ATE_boolean: return "bool";
    case dw::DW_ATE_signed: return "signed" + bits;
    case dw::DW_ATE_unsigned: return "unsigned" + bits;
    case dw::DW_ATE_float: return "float" + bits;
    case dw::DW_ATE_signed_char: return "signed-char";
    case dw::DW_ATE_unsigned_char: return "unsigned-char";
    default: return "?";
    }
  }
  case dw::DW_TAG_array_type: {
    std::string extents;
    for (const auto child : die.children()) {
      if (child.getTag() != dw::DW_TAG_subrange_type)
        continue;
      auto count = number(child, dw::DW_AT_count);
      if (!count)
        if (const auto limit = number(child, dw::DW_AT_upper_bound); limit && *limit < UINT64_MAX)
          count = *limit + 1;
      if (!count)
        return "?";
      extents += "[" + std::to_string(*count) + "]";
    }
    return extents + storage_type(type(die), depth + 1);
  }
  default:
    break;
  }
  if (!record(die) && tag != dw::DW_TAG_enumeration_type)
    return "?";
  std::string result = qualified(die);
  if (result.empty())
    return "?";
  if (const auto angle = name(die).find('<'); angle != std::string::npos) {
    const auto spelling = result;
    result.resize(result.size() - name(die).size() + angle);
    const auto prefix = result.size();
    for (const auto child : die.children()) {
      if (child.getTag() == dw::DW_TAG_template_type_parameter)
        result += "<" + storage_type(type(child), depth + 1) + ">";
      else if (child.getTag() == dw::DW_TAG_GNU_template_parameter_pack) {
        for (const auto element : child.children()) {
          if (element.getTag() != dw::DW_TAG_template_type_parameter)
            return "?";
          result += "<" + storage_type(type(element), depth + 1) + ">";
        }
      }
      else if (child.getTag() == dw::DW_TAG_template_value_parameter) {
        const auto value = child.find(dw::DW_AT_const_value);
        if (!value)
          return "?";
        if (const auto integral = value->getAsSignedConstant())
          result += "<#" + std::to_string(*integral) + ">";
        else
          return "?";
      }
    }
    if (result.size() == prefix)
      return storage_spelling(spelling);
  }
  return result;
}

std::optional<std::uint64_t> storage_size(DWARFDie die) {
  if (!die)
    return std::nullopt;
  if (const auto bytes = size_of(die))
    return bytes;
  if (die.getTag() == dw::DW_TAG_reference_type || die.getTag() == dw::DW_TAG_rvalue_reference_type)
    return die.getDwarfUnit()->getAddressByteSize();
  if (die.getTag() != dw::DW_TAG_array_type)
    return std::nullopt;
  auto bytes = storage_size(type(die));
  for (const auto child : die.children()) {
    if (child.getTag() != dw::DW_TAG_subrange_type)
      continue;
    auto count = number(child, dw::DW_AT_count);
    if (!count)
      if (const auto upper = number(child, dw::DW_AT_upper_bound); upper && *upper < UINT64_MAX)
        count = *upper + 1;
    if (!count || !bytes || (*count && *bytes > UINT64_MAX / *count))
      return std::nullopt;
    *bytes *= *count;
  }
  return bytes;
}

void verify_receiver_storage(llvm::DWARFContext &dwarf, llvm::DWARFUnit *unit,
                             const clangtracer::v1::PreparedInjector &prepared) {
  using Storage = clangtracer::v1::PreparedStorageRecord;
  if (prepared.receiver_storage().empty())
    throw std::runtime_error("snapshot adapter has no source layout evidence");
  std::map<std::string, const Storage *> expected;
  for (const auto &layout : prepared.receiver_storage())
    if (layout.name().empty() || layout.name().find('?') != std::string::npos ||
        !layout.bytes() || !expected.emplace(layout.name(), &layout).second)
      throw std::runtime_error("ambiguous source record layout: " + layout.name());
  const auto discover = [&](const std::vector<DWARFDie> &roots) {
    std::map<std::string, std::vector<DWARFDie>> found;
    std::vector<DWARFDie> pending = roots;
    while (!pending.empty()) {
      const auto die = pending.back();
      pending.pop_back();
      for (const auto child : die.children()) {
        if (record(child) && number(child, dw::DW_AT_byte_size)) {
          const auto identity = storage_type(child);
          if (expected.count(identity))
            found[identity].push_back(child);
        }
        if ((child.getTag() == dw::DW_TAG_namespace && name(child) != "std") || record(child))
          pending.push_back(child);
      }
    }
    return found;
  };
  std::map<std::string, std::vector<DWARFDie>> local;
  if (unit)
    local = discover({unit->getUnitDIE(false)});
  std::optional<std::map<std::string, std::vector<DWARFDie>>> all;
  for (const auto &[identity, layout] : expected) {
    const std::vector<DWARFDie> *matches = nullptr;
    if (const auto found = local.find(identity); found != local.end()) {
      matches = &found->second;
    } else {
      if (!all) {
        std::vector<DWARFDie> roots;
        for (const auto &candidate : dwarf.compile_units())
          roots.push_back(candidate->getUnitDIE(false));
        all = discover(roots);
      }
      if (const auto found = all->find(identity); found != all->end())
        matches = &found->second;
    }
    if (!matches || matches->size() != 1)
      throw std::runtime_error("snapshot layout has missing or ambiguous DWARF record: " + identity);
    const auto actual = matches->front();
    if (size_of(actual) != layout->bytes())
      throw std::runtime_error("source layout size differs from DWARF: " + identity);
    std::set<std::pair<bool, std::string>> actual_members;
    for (const auto child : actual.children()) {
      const bool base = child.getTag() == dw::DW_TAG_inheritance;
      if (!base && child.getTag() != dw::DW_TAG_member)
        continue;
      if (!base && (child.find(dw::DW_AT_artificial) || child.find(dw::DW_AT_external) ||
                    child.find(dw::DW_AT_declaration)))
        continue;
      const auto member_type = storage_type(type(child));
      const auto name_key = base ? member_type : name(child);
      if (!actual_members.emplace(base, name_key).second)
        throw std::runtime_error("ambiguous DWARF member layout: " + identity + "::" + name_key);
      const auto found = std::find_if(layout->members().begin(), layout->members().end(),
          [&](const auto &item) {
            return item.base() == base && (base ? item.type() : item.name()) == name_key;
          });
      const auto offset = number(child, dw::DW_AT_data_member_location);
      if (found == layout->members().end() || found->needs_dynamic_location() ||
          child.find(dw::DW_AT_bit_size) || !offset || *offset > UINT64_MAX / 8 ||
          *offset * 8 != found->offset_bits() || found->type().find('?') != std::string::npos ||
          found->type() != member_type || (!base && storage_size(type(child)) != found->bytes()))
        throw std::runtime_error("source layout differs or is unverified for " + identity + "::" +
            name_key + " (DWARF type " + member_type + ", source type " +
            (found == layout->members().end() ? "<absent>" : found->type()) + ")");
    }
    if (actual_members.size() != static_cast<std::size_t>(layout->members_size()))
      throw std::runtime_error("source layout members differ from DWARF: " + identity);
  }
}

struct Member {
  DWARFDie type;
  std::uint64_t offset;
};

std::optional<Member> located_member(DWARFDie parent, DWARFDie child) {
  if (child.find(dw::DW_AT_bit_size))
    return std::nullopt;
  auto offset = number(child, dw::DW_AT_data_member_location);
  if (!offset && parent.getTag() == dw::DW_TAG_union_type &&
      !child.find(dw::DW_AT_external))
    offset = 0;
  const auto child_type = type(child);
  if (!offset || !child_type)
    return std::nullopt;
  const auto size = storage_size(child_type);
  const auto bound = number(parent, dw::DW_AT_byte_size);
  if (!size || !bound || *offset > *bound || *size > *bound - *offset)
    return std::nullopt;
  return Member{child_type, *offset};
}

void find_members(DWARFDie parent, const std::string &wanted, std::uint64_t origin,
                  std::vector<Member> &matches, unsigned depth) {
  if (!record(parent) || depth == 0)
    return;
  for (const auto child : parent.children()) {
    const auto tag = child.getTag();
    if (tag != dw::DW_TAG_inheritance && tag != dw::DW_TAG_member)
      continue;
    if (const auto member = located_member(parent, child)) {
      const auto offset = origin + member->offset;
      if (tag == dw::DW_TAG_inheritance) {
        find_members(member->type, wanted, offset, matches, depth - 1);
      } else if (name(child) == wanted) {
        matches.push_back({member->type, offset});
      }
    }
  }
}

Member member(DWARFDie parent, const std::string &wanted) {
  std::vector<Member> matches;
  find_members(parent, wanted, 0, matches, 32);
  if (matches.size() != 1)
    throw std::runtime_error("missing or ambiguous DWARF member " + name(parent) + "::" + wanted);
  return matches.front();
}

Member nested_member(DWARFDie parent, std::string_view path, bool &opaque_union,
                     DWARFDie *direct_union = nullptr) {
  std::uint64_t offset = 0;
  while (true) {
    opaque_union |= parent.getTag() == dw::DW_TAG_union_type;
    const auto split = path.find('.');
    const auto selected = member(parent, std::string(path.substr(0, split)));
    if (selected.offset > UINT64_MAX - offset)
      throw std::runtime_error("nested object field offset overflows");
    offset += selected.offset;
    if (split == std::string_view::npos) {
      if (direct_union && parent.getTag() == dw::DW_TAG_union_type && selected.offset == 0)
        *direct_union = parent;
      return {selected.type, offset};
    }
    parent = selected.type;
    path.remove_prefix(split + 1);
  }
}

bool pointer(DWARFDie die) {
  return die && die.getTag() == dw::DW_TAG_pointer_type &&
         size_of(die) == 8;
}

void find_pointee(DWARFDie parent, const std::string &base, std::uint64_t origin,
                  std::set<std::uint64_t> &matches, unsigned depth) {
  if (!record(parent) || depth == 0)
    return;
  for (const auto child : parent.children()) {
    if (child.getTag() != dw::DW_TAG_member && child.getTag() != dw::DW_TAG_inheritance)
      continue;
    if (const auto field = located_member(parent, child)) {
      const auto offset = origin + field->offset;
      if (pointer(field->type) && qualified(type(field->type)) == base)
        matches.insert(offset);
      else
        find_pointee(field->type, base, offset, matches, depth - 1);
    }
  }
}

bool writer_receiver(DWARFDie function, std::uint64_t entry, const std::string &concrete,
                     bool scalar_result = false) {
  // The entry instruction can already be attributed to an inline callee.
  // The machine ABI at that address belongs to the enclosing subprogram.
  while (function && function.getTag() != dw::DW_TAG_subprogram)
    function = function.getParent();
  if (!function)
    return false;
  auto ranges = function.getAddressRanges();
  if (!ranges) {
    llvm::consumeError(ranges.takeError());
    return false;
  }
  if (std::none_of(ranges->begin(), ranges->end(), [entry](const auto &range) {
        return range.LowPC == entry;
      }))
    return false;
  if (const auto value = function.findRecursively({dw::DW_AT_type})) {
    const auto result = function.getAttributeValueAsReferencedDie(*value);
    const auto encoding = storage_type(result);
    if (!scalar_result || !(encoding == "bool" || encoding == "signed32" || encoding == "unsigned32" ||
                           encoding == "signed64" || encoding == "unsigned64" ||
                           encoding.starts_with("*") || encoding.starts_with("&")))
      return false; // A hidden result argument can change the receiver register.
  }
  for (const auto parameter : function.children()) {
    if (parameter.getTag() != dw::DW_TAG_formal_parameter)
      continue;
    const auto value = parameter.findRecursively({dw::DW_AT_type});
    auto pointer = value ? parameter.getAttributeValueAsReferencedDie(*value) : DWARFDie{};
    if (pointer && (pointer.getTag() == dw::DW_TAG_const_type ||
                    pointer.getTag() == dw::DW_TAG_typedef))
      pointer = type(pointer);
    if (!pointer || (pointer.getTag() != dw::DW_TAG_pointer_type &&
        pointer.getTag() != dw::DW_TAG_reference_type &&
        pointer.getTag() != dw::DW_TAG_rvalue_reference_type))
      return false;
    return qualified(type(pointer)) == concrete;
  }
  return false;
}

std::optional<std::pair<std::string, std::int64_t>> inline_receiver(
    DWARFDie inlined, std::uint64_t entry, const std::string &container) {
  for (const auto parameter : inlined.children()) {
    if (parameter.getTag() != dw::DW_TAG_formal_parameter)
      continue;
    const auto type_value = parameter.findRecursively({dw::DW_AT_type});
    auto pointer = type_value ? parameter.getAttributeValueAsReferencedDie(*type_value) : DWARFDie{};
    if (pointer && (pointer.getTag() == dw::DW_TAG_const_type || pointer.getTag() == dw::DW_TAG_typedef))
      pointer = type(pointer);
    if (!pointer || (pointer.getTag() != dw::DW_TAG_pointer_type &&
                     pointer.getTag() != dw::DW_TAG_reference_type) || qualified(type(pointer)) != container)
      return std::nullopt;
    auto locations = parameter.getLocations(dw::DW_AT_location);
    if (!locations) {
      llvm::consumeError(locations.takeError());
      return std::nullopt;
    }
    std::set<std::pair<std::string, std::int64_t>> receivers;
    for (const auto &location : *locations) {
      if (location.Range && (entry < location.Range->LowPC || entry >= location.Range->HighPC))
        continue;
      const auto &expression = location.Expr;
      if (expression.empty())
        return std::nullopt;
      unsigned reg = 32;
      std::int64_t offset = 0;
      if (expression.size() == 1 && expression[0] >= dw::DW_OP_reg0 && expression[0] <= dw::DW_OP_reg31) {
        reg = expression[0] - dw::DW_OP_reg0;
      } else if (expression.size() >= 3 && expression[0] >= dw::DW_OP_breg0 &&
                 expression[0] <= dw::DW_OP_breg31 && expression.back() == dw::DW_OP_stack_value) {
        unsigned length = 0;
        const char *error = nullptr;
        offset = llvm::decodeSLEB128(expression.data() + 1, &length, expression.end() - 1, &error);
        if (!error && length + 2 == expression.size())
          reg = expression[0] - dw::DW_OP_breg0;
      }
#if defined(__aarch64__)
      if (reg >= 32)
        return std::nullopt;
      receivers.emplace(reg == 31 ? "SP" : "X" + std::to_string(reg), offset);
#else
      static constexpr std::string_view names[]{"RAX", "RDX", "RCX", "RBX", "RSI", "RDI", "RBP", "RSP",
          "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
      if (reg >= std::size(names))
        return std::nullopt;
      receivers.emplace(names[reg], offset);
#endif
    }
    if (receivers.size() == 1)
      return *receivers.begin();
    return std::nullopt;
  }
  return std::nullopt;
}

struct InlineAnchors {
  std::uint64_t begin;
  std::vector<std::uint64_t> continuations;
};

std::optional<InlineAnchors> verified_inline_region(const native_entry::Preview &body,
    std::span<const InlineMutation::Range> ranges) {
  if (ranges.empty() || body.instructions.empty() || body.decode_failed || !body.diagnostic.empty())
    return std::nullopt;
  const auto containing = [&](std::uint64_t pc) {
    return std::ranges::find_if(ranges, [&](const auto &range) {
      return range.first <= pc && pc < range.second;
    });
  };
  std::set<std::uint64_t> entries, exits, instructions, boundaries;
  std::map<std::uint64_t, std::vector<std::uint64_t>> region;
  const auto first = body.instructions.front().address;
  if (containing(first) != ranges.end())
    entries.insert(first);
  for (const auto &inst : body.instructions) {
    instructions.insert(inst.address);
    boundaries.insert(inst.address);
    boundaries.insert(inst.address + inst.size);
    const auto range = containing(inst.address);
    const bool inside = range != ranges.end();
    if ((inst.flow == native_entry::Flow::branch && !inst.target) ||
        (inside && (inst.size > range->second - inst.address ||
                    inst.flow == native_entry::Flow::returns)) ||
        (inst.flow == native_entry::Flow::call && inst.target && containing(*inst.target) != ranges.end()))
      return std::nullopt;
    if (inside)
      region.try_emplace(inst.address);
    const auto edge = [&](std::uint64_t successor) {
      const bool destination_inside = containing(successor) != ranges.end();
      if (!inside && destination_inside)
        entries.insert(successor);
      else if (inside && !destination_inside)
        exits.insert(successor);
      else if (inside)
        region.at(inst.address).push_back(successor);
    };
    if (inst.falls_through)
      edge(inst.address + inst.size);
    if (inst.flow == native_entry::Flow::branch)
      edge(*inst.target);
  }
  if (entries.size() != 1 || exits.empty() ||
      !std::ranges::includes(instructions, exits) ||
      std::ranges::any_of(ranges, [&](const auto &range) {
        return !instructions.count(range.first) || !boundaries.count(range.second);
      }))
    return std::nullopt;
  std::vector<std::uint64_t> pending{*entries.begin()};
  std::set<std::uint64_t> reached;
  while (!pending.empty()) {
    const auto pc = pending.back();
    pending.pop_back();
    if (!reached.insert(pc).second)
      continue;
    const auto successor = region.find(pc);
    if (successor == region.end())
      return std::nullopt;
    pending.insert(pending.end(), successor->second.begin(), successor->second.end());
  }
  if (reached.size() != region.size())
    return std::nullopt;
  return InlineAnchors{*entries.begin(), {exits.begin(), exits.end()}};
}

bool inline_prefix_is_setup(const native_entry::Preview &body, std::uint64_t begin,
                            std::uint64_t anchor) {
  auto next = begin;
  for (const auto &inst : body.instructions) {
    if (inst.address < begin || inst.address >= anchor)
      continue;
    if (inst.address != next || inst.flow != native_entry::Flow::ordinary || !inst.falls_through ||
        inst.may_store || inst.changes_stack || !(inst.cache_copyable || inst.cache_entry_copyable))
      return false;
    next += inst.size;
  }
  return next == anchor;
}

std::vector<InlineMutation> inline_mutations_in(DWARFDie writer, std::uint64_t bias,
    std::string_view selector, const std::string &container, const native_entry::Preview &body) {
  while (writer && writer.getTag() != dw::DW_TAG_subprogram)
    writer = writer.getParent();
  std::vector<InlineMutation> result;
  if (!writer)
    return result;
  const auto compact = [](std::string_view text) {
    std::string name;
    for (unsigned char ch : text)
      if (!std::isspace(ch))
        name.push_back(static_cast<char>(ch));
    return name;
  };
  const auto normalized = compact(selector);
  const auto visit = [&](const auto &self, DWARFDie die) -> void {
    if (die.getTag() == dw::DW_TAG_inlined_subroutine) {
      const auto *linkage = die.getName(llvm::DINameKind::LinkageName);
      const auto *name = die.getShortName();
      if ((linkage && (linkage == selector || compact(llvm::demangle(linkage)) == normalized)) ||
          (name && name == selector)) {
        auto ranges = die.getAddressRanges();
        if (!ranges) {
          llvm::consumeError(ranges.takeError());
          throw std::runtime_error("cannot read DWARF ranges for inlined collection mutation");
        }
        if (ranges->empty())
          throw std::runtime_error("inlined collection mutation has no verified code range");
        std::ranges::sort(*ranges, {}, &llvm::DWARFAddressRange::LowPC);
        std::vector<InlineMutation::Range> code;
        for (const auto &range : *ranges) {
          if (range.LowPC >= range.HighPC || range.HighPC > UINT64_MAX - bias ||
              (!code.empty() && bias + range.LowPC < code.back().second))
            throw std::runtime_error("inlined collection mutation has overlapping or invalid ranges");
          if (!code.empty() && bias + range.LowPC == code.back().second)
            code.back().second = bias + range.HighPC;
          else
            code.emplace_back(bias + range.LowPC, bias + range.HighPC);
        }
        auto anchors = verified_inline_region(body, code);
        auto receiver = anchors ? inline_receiver(die, anchors->begin - bias, container) : std::nullopt;
        if ((!anchors || !receiver) && code.size() > 1 &&
            inline_prefix_is_setup(body, code.front().first, code.back().first)) {
          code = {code.back()};
          anchors = verified_inline_region(body, code);
          receiver = anchors ? inline_receiver(die, anchors->begin - bias, container) : std::nullopt;
        }
        if (!anchors || !receiver)
          throw std::runtime_error("inlined collection mutation requires a verified receiver and continuation");
        const auto begin = anchors->begin;
        const auto span = std::ranges::find_if(code, [&](const auto &range) {
          return range.first <= begin && begin < range.second;
        });
        result.push_back({begin, span->second, receiver->first, receiver->second,
                          std::move(anchors->continuations), std::move(code)});
      }
    }
    for (const auto child : die.children())
      self(self, child);
  };
  for (const auto child : writer.children())
    visit(visit, child);
  return result;
}

Layout inspect_vector(DWARFDie owner, const clangtracer::v1::ObjectWatchRequest &request) {
  const auto vector = member(owner, request.vector_field());
  if (!qualified(vector.type).starts_with("std::vector<"))
    throw std::runtime_error("logical element capture requires a libstdc++ vector");
  const auto implementation = member(vector.type, "_M_impl");
  const auto begin = member(implementation.type, "_M_start");
  const auto end = member(implementation.type, "_M_finish");
  const auto capacity = member(implementation.type, "_M_end_of_storage");
  const auto element = type(begin.type);
  const auto stride = size_of(element);
  if (!stride || !*stride || !record(element) || !pointer(begin.type) ||
      !pointer(end.type) || !pointer(capacity.type) ||
      storage_type(begin.type) != storage_type(end.type) ||
      storage_type(begin.type) != storage_type(capacity.type))
    throw std::runtime_error("vector storage pointers or element layout could not be verified");
  bool opaque_union = false;
  const auto scalar = nested_member(element, request.field(), opaque_union);
  const auto key = member(element, request.element_key_field());
  if (storage_type(scalar.type) != "signed32" || storage_type(key.type) != "unsigned32" ||
      scalar.offset == key.offset || opaque_union)
    throw std::runtime_error("logical vector requires distinct uint32 key and int32 value members");
  Layout result{};
  result.scalar = scalar.offset;
  result.key_offset = key.offset;
  result.stride = *stride;
  result.vector_object = vector.offset;
  result.vector_begin = vector.offset + implementation.offset + begin.offset;
  result.vector_end = vector.offset + implementation.offset + end.offset;
  result.vector_capacity = vector.offset + implementation.offset + capacity.offset;
  result.logical_vector = true;
  return result;
}

DWARFDie named_type(DWARFDie owner, std::string_view wanted) {
  DWARFDie result;
  for (const auto child : owner.children())
    if ((child.getTag() == dw::DW_TAG_typedef || child.getTag() == dw::DW_TAG_template_type_parameter) &&
        name(child) == wanted) {
      if (result)
        throw std::runtime_error("ambiguous map DWARF type: " + std::string(wanted));
      result = type(child);
    }
  return result;
}

Member pointee_storage(DWARFDie wrapper) {
  if (pointer(wrapper))
    return {type(wrapper), 0};
  const auto identity = qualified(wrapper);
  const auto target = named_type(wrapper, "_Tp");
  if (target && identity.starts_with("std::shared_ptr<")) {
    const auto data = member(wrapper, "_M_ptr");
    if (pointer(data.type) && qualified(type(data.type)) == qualified(target))
      return {target, data.offset};
  } else if (target && identity.starts_with("std::unique_ptr<") && size_of(wrapper) == 8) {
    const auto deleter = named_type(wrapper, "_Dp");
    if (qualified(deleter).starts_with("std::default_delete<") &&
        qualified(named_type(deleter, "_Tp")) == qualified(target)) {
      std::set<std::uint64_t> candidates;
      find_pointee(wrapper, qualified(target), 0, candidates, 32);
      if (candidates == std::set<std::uint64_t>{0})
        return {target, 0};
    }
  }
  throw std::runtime_error("pointee path requires a verified data pointer or libstdc++ smart pointer");
}

Layout inspect_map(DWARFDie owner, const clangtracer::v1::ObjectWatchRequest &request) {
  const auto map = member(owner, request.map_field());
  if (!qualified(map.type).starts_with("std::map<"))
    throw std::runtime_error("logical map capture requires a libstdc++ map");
  const auto tree = member(map.type, "_M_t");
  const auto implementation = member(tree.type, "_M_impl");
  const auto header = member(implementation.type, "_M_header");
  const auto count = member(implementation.type, "_M_node_count");
  const auto parent = member(header.type, "_M_parent");
  const auto left = member(header.type, "_M_left");
  const auto right = member(header.type, "_M_right");
  const auto link = named_type(tree.type, "_Link_type");
  const auto node = type(link);
  const auto pair = named_type(tree.type, "_Val");
  const auto comparator = named_type(tree.type, "_Compare");
  if (!pointer(link) || !record(node) || !record(pair) ||
      !qualified(node).starts_with("std::_Rb_tree_node<") || !qualified(pair).starts_with("std::pair<") ||
      !qualified(comparator).starts_with("std::less<") ||
      !pointer(parent.type) || !pointer(left.type) || !pointer(right.type) ||
      storage_type(count.type) != "unsigned64")
    throw std::runtime_error("cannot verify the libstdc++ map tree, comparator or node layout");
  bool fixed_base = false;
  for (const auto child : node.children())
    if (child.getTag() == dw::DW_TAG_inheritance && type(child) == header.type &&
        number(child, dw::DW_AT_data_member_location) == 0 &&
        number(child, dw::DW_AT_virtuality).value_or(0) == 0)
      fixed_base = true;
  const auto storage = member(node, "_M_storage");
  const auto key = member(pair, "first");
  const auto value = member(pair, "second");
  bool opaque_union = false;
  const auto scalar = nested_member(value.type, request.field(), opaque_union);
  if (!fixed_base || size_of(storage.type) != size_of(pair) ||
      !qualified(key.type).starts_with("std::__cxx11::basic_string<char") ||
      qualified(named_type(key.type, "_Traits")) != "std::char_traits<char>" ||
      qualified(named_type(key.type, "_Alloc")) != "std::allocator<char>" ||
      qualified(named_type(comparator, "_Tp")) != qualified(key.type) ||
      storage_type(scalar.type) != "signed32" || opaque_union)
    throw std::runtime_error("logical map requires verified node storage, string key and int32 value");
  const auto data = member(key.type, "_M_dataplus");
  const auto key_pointer = member(data.type, "_M_p");
  const auto key_length = member(key.type, "_M_string_length");
  if (!pointer(key_pointer.type) || storage_type(key_length.type) != "unsigned64")
    throw std::runtime_error("logical map string key layout is unverified");
  Layout result{};
  result.logical_map = true;
  result.scalar = storage.offset + value.offset + scalar.offset;
  result.vector_object = map.offset;
  const auto tree_offset = map.offset + tree.offset + implementation.offset;
  result.map.header = tree_offset + header.offset;
  result.map.root = result.map.header + parent.offset;
  result.map.count = tree_offset + count.offset;
  result.map.left = left.offset;
  result.map.right = right.offset;
  result.map.parent = parent.offset;
  result.map.key_pointer = storage.offset + key.offset + data.offset + key_pointer.offset;
  result.map.key_length = storage.offset + key.offset + key_length.offset;
  return result;
}

Layout inspect_layout(llvm::DWARFContext &dwarf,
                      const clangtracer::v1::ObjectWatchRequest &request,
                      llvm::DWARFUnit *writer_unit, llvm::DWARFContext &owner_dwarf) {
  const auto concrete = find_record(dwarf, request.concrete_type(), writer_unit);
  if (!request.vector_field().empty())
    return inspect_vector(concrete, request);
  if (!request.map_field().empty())
    return inspect_map(concrete, request);
  if (!request.pointee_field().empty()) {
    Layout result{};
    auto current = concrete;
    for (auto path = std::string_view(request.pointee_field());;) {
      const auto next = path.find("->");
      bool opaque = false;
      const auto field = nested_member(current, path.substr(0, next), opaque);
      if (opaque || result.pointer_offsets.size() == max_pointee_hops)
        throw std::runtime_error("pointee path exceeds its verified range");
      const auto data = pointee_storage(field.type);
      if (data.offset > UINT64_MAX - field.offset)
        throw std::runtime_error("pointee path field offset overflows");
      current = data.type;
      if (!record(current))
        throw std::runtime_error("pointee path must select a complete record");
      result.pointer_offsets.push_back(field.offset + data.offset);
      if (next == std::string_view::npos)
        break;
      path.remove_prefix(next + 2);
    }
    bool opaque = false;
    const auto field = nested_member(current, request.field(), opaque);
    if (opaque || storage_type(field.type) != "signed32")
      throw std::runtime_error("pointee write capture requires a signed 32-bit field");
    result.scalar = field.offset;
    return result;
  }
  bool opaque_union = false;
  DWARFDie direct_union;
  const auto scalar = nested_member(concrete, request.field(), opaque_union, &direct_union);
  if (opaque_union) {
    if (!request.owner_type().empty() || size_of(scalar.type) != 4)
      throw std::runtime_error("union watches require a standalone four-byte storage range");
    Layout result{};
    result.scalar = scalar.offset;
    result.opaque_union = true;
    if (!request.union_tag().empty()) {
      bool tag_in_union = false;
      const auto discriminator = nested_member(concrete, request.union_tag(), tag_in_union);
      const auto tag_type = discriminator.type.getTag() == dw::DW_TAG_enumeration_type
          ? type(discriminator.type) : discriminator.type;
      if (!direct_union || tag_in_union || storage_type(tag_type) != "unsigned32" ||
          std::max(discriminator.offset, scalar.offset) -
              std::min(discriminator.offset, scalar.offset) < 4)
        throw std::runtime_error("union tag must be a verified nonoverlapping uint32 field");
      result.union_tag_offset = discriminator.offset;
      for (const auto &entry : request.union_cases()) {
        if (entry.member().empty() || entry.member().size() > 512 ||
            entry.member().find('\0') != std::string::npos)
          throw std::runtime_error("invalid union case member name");
        const auto alternative = member(direct_union, entry.member());
        const auto encoding = storage_type(alternative.type);
        UnionCase::Type kind;
        if (encoding == "signed32")
          kind = UnionCase::Type::signed_integer;
        else if (encoding == "unsigned32")
          kind = UnionCase::Type::unsigned_integer;
        else if (encoding == "float32")
          kind = UnionCase::Type::real;
        else
          throw std::runtime_error("union case requires a signed, unsigned or floating 32-bit member");
        if (alternative.offset || !result.union_cases.emplace(entry.tag(),
            UnionCase{entry.member(), kind}).second)
          throw std::runtime_error("overlapping union cases require distinct discriminator values");
      }
    }
    return result;
  }
  if (!request.union_tag().empty())
    throw std::runtime_error("tagged capture requires --field to select a union member");
  const auto signed_int32 = [](DWARFDie type) {
    return type.getTag() == dw::DW_TAG_base_type &&
        number(type, dw::DW_AT_byte_size) == 4 &&
        number(type, dw::DW_AT_encoding) == dw::DW_ATE_signed;
  };
  const bool atomic = record(scalar.type) && qualified(scalar.type) == "std::atomic<int>";
  if (atomic) {
    const auto backing = member(scalar.type, "_M_i");
    if (number(scalar.type, dw::DW_AT_byte_size) != 4 ||
        backing.offset != 0 || !signed_int32(backing.type))
      throw std::runtime_error("object watch cannot verify std::atomic<int> storage");
  } else if (!signed_int32(scalar.type)) {
    throw std::runtime_error("object watch requires a signed 32-bit field or std::atomic<int>");
  }
  if (request.owner_type().empty()) {
    bool primary_vptr = false, virtual_member = false;
    for (const auto child : concrete.children()) {
      if (child.getTag() == dw::DW_TAG_inheritance)
        throw std::runtime_error("standalone scalar watches require a record without bases");
      if (child.getTag() == dw::DW_TAG_subprogram)
        virtual_member |= number(child, dw::DW_AT_virtuality).value_or(0) != 0;
      if (child.getTag() == dw::DW_TAG_member && child.find(dw::DW_AT_artificial) &&
          (name(child).starts_with("_vptr.") || name(child).starts_with("_vptr$")) &&
          number(child, dw::DW_AT_data_member_location) == 0 &&
          pointer(type(child)))
        primary_vptr = true;
    }
    if (virtual_member && !primary_vptr)
      throw std::runtime_error("cannot verify a primary vptr for the standalone scalar receiver");
    Layout direct{};
    direct.scalar = scalar.offset;
    direct.atomic = atomic;
    direct.primary_rtti = primary_vptr;
    return direct;
  }
  const auto base = find_record(dwarf, request.base_type(), writer_unit);
  const auto owner = find_record(owner_dwarf, request.owner_type(),
      &owner_dwarf == &dwarf ? writer_unit : nullptr);
  std::optional<std::uint64_t> base_offset;
  for (const auto child : concrete.children()) {
    if (child.getTag() != dw::DW_TAG_inheritance || type(child) != base)
      continue;
    const auto inheritance = located_member(concrete, child);
    if (base_offset || !inheritance || number(child, dw::DW_AT_virtuality).value_or(0) != 0)
      throw std::runtime_error("object watch requires an unambiguous fixed base adjustment");
    base_offset = inheritance->offset;
  }
  if (!base_offset)
    throw std::runtime_error("object watch requires a directly verified base subobject");
  Layout result{};
  result.scalar = scalar.offset;
  result.atomic = atomic;
  result.base_offset = *base_offset;
  if (!request.snapshot_field().empty()) {
    const auto string = member(base, request.snapshot_field());
    const auto string_name = qualified(string.type);
    if (!string_name.starts_with("std::__cxx11::basic_string<char,") &&
        string_name != "std::__cxx11::basic_string<char>")
      throw std::runtime_error("snapshot field must use the libstdc++ C++11 std::string ABI");
    const auto data = member(string.type, "_M_dataplus");
    const auto string_pointer = member(data.type, "_M_p");
    const auto string_length = member(string.type, "_M_string_length");
    if (!pointer(string_pointer.type) ||
        number(type(string_pointer.type), dw::DW_AT_byte_size) != 1 ||
        number(string_length.type, dw::DW_AT_byte_size) != 8 ||
        number(string_length.type, dw::DW_AT_encoding) != dw::DW_ATE_unsigned)
      throw std::runtime_error("unsupported DWARF std::string layout");
    result.string_pointer = *base_offset + string.offset + data.offset + string_pointer.offset;
    result.string_length = *base_offset + string.offset + string_length.offset;
  }

  const auto vector = member(owner, request.collection());
  if (!qualified(vector.type).starts_with("std::vector<"))
    throw std::runtime_error("owner field must be a libstdc++ vector");
  const auto implementation = member(vector.type, "_M_impl");
  const auto begin = member(implementation.type, "_M_start");
  const auto end = member(implementation.type, "_M_finish");
  const auto capacity = member(implementation.type, "_M_end_of_storage");
  const auto element = type(begin.type);
  if (!pointer(begin.type) || begin.type != end.type || begin.type != capacity.type ||
      !qualified(element).starts_with("std::unique_ptr<"))
    throw std::runtime_error("owner field must be a vector of libstdc++ unique_ptr");
  const auto stride = number(element, dw::DW_AT_byte_size);
  std::set<std::uint64_t> pointee;
  find_pointee(element, request.base_type(), 0, pointee, 32);
  if (stride != 8 || pointee != std::set<std::uint64_t>{0})
    throw std::runtime_error("cannot verify unique_ptr pointee layout");
  result.vector_begin = vector.offset + implementation.offset + begin.offset;
  result.vector_end = vector.offset + implementation.offset + end.offset;
  result.vector_capacity = vector.offset + implementation.offset + capacity.offset;
  result.stride = *stride;
  result.pointee = *pointee.begin();
  return result;
}

using Range = std::pair<std::uint64_t, std::uint64_t>;
struct Symbol {
  std::string linkage, demangled;
  Range range;
  unsigned kind;
};

bool operator_delete_name(std::string_view linkage) {
  const auto name = llvm::demangle(linkage);
  return name.starts_with("operator delete(") || name.find("::operator delete(") != std::string::npos;
}

void append_symbols(const ObjectFile &object, std::vector<Symbol> &symbols) {
  const auto visit = [&](const auto &symbol) {
    const llvm::object::ELFSymbolRef elf(symbol);
    const unsigned kind = elf.getELFType();
    if (kind != llvm::ELF::STT_FUNC && kind != llvm::ELF::STT_OBJECT)
      return;
    auto linkage = symbol.getName();
    auto address = symbol.getAddress();
    auto flags = symbol.getFlags();
    if (!linkage || !address || !flags) {
      if (!linkage) llvm::consumeError(linkage.takeError());
      if (!address) llvm::consumeError(address.takeError());
      if (!flags) llvm::consumeError(flags.takeError());
      return;
    }
    if (!*address || !elf.getSize() || (*flags & llvm::object::SymbolRef::SF_Undefined))
      return;
    const auto mangled = linkage->str();
    symbols.push_back({mangled, llvm::demangle(mangled), {*address, elf.getSize()}, kind});
  };
  for (const auto &symbol : object.symbols())
    visit(symbol);
  for (const auto &symbol : llvm::cast<llvm::object::ELF64LEObjectFile>(object).getDynamicSymbolIterators())
    visit(symbol);
}

Range select(const std::vector<Symbol> &symbols, const std::string &name,
             unsigned kind, const auto &matches) {
  std::set<Range> candidates;
  for (const auto &symbol : symbols) {
    if (symbol.kind == kind && matches(symbol))
      candidates.insert(symbol.range);
  }
  if (candidates.size() != 1)
    throw std::runtime_error("missing or ambiguous object watch ELF symbol: " + name);
  return *candidates.begin();
}

std::string writer_module(std::uint32_t pid, const std::string &executable,
                          const std::string &selector) {
  if (selector.empty() || selector == executable)
    return executable;
  if (!pid)
    return std::filesystem::canonical(selector).string();
  std::vector<std::string> candidates;
  for (const auto &path : client::list_debug_modules(pid))
    if (selector == path || selector == std::filesystem::path(path).filename())
      candidates.push_back(path);
  if (candidates.size() != 1)
    throw std::runtime_error("--module must identify one live ELF by path or basename");
  return candidates.front();
}
// Read the call-site table used by the Itanium C++ unwinder on ELF. The
// common GCC and Clang form uses the FDE entry as LPStart and ULEB128 or
// fixed-width unsigned call-site offsets. Leave other encodings unverified.
std::vector<std::uint64_t> landing_pad_offsets(llvm::StringRef lsda, std::uint64_t function_size) {
  const llvm::DataExtractor header(lsda, true, 8);
  llvm::DataExtractor::Cursor position(0);
  const auto base_encoding = header.getU8(position);
  if (header.getU8(position) != dw::DW_EH_PE_omit)
    header.getULEB128(position); // Type table offset.
  const auto encoding = header.getU8(position);
  const auto size = header.getULEB128(position);
  if (auto error = position.takeError()) {
    llvm::consumeError(std::move(error));
    return {};
  }
  if (base_encoding != dw::DW_EH_PE_omit || size > lsda.size() - position.tell())
    return {};
  unsigned width = 0;
  switch (encoding) {
  case dw::DW_EH_PE_uleb128: break;
  case dw::DW_EH_PE_udata2: width = 2; break;
  case dw::DW_EH_PE_udata4: width = 4; break;
  case dw::DW_EH_PE_udata8: width = 8; break;
  default: return {};
  }

  const llvm::DataExtractor table(lsda.substr(position.tell(), size), true, 8);
  llvm::DataExtractor::Cursor entry(0);
  const auto read_offset = [&] {
    return width ? table.getUnsigned(entry, width) : table.getULEB128(entry);
  };
  std::vector<std::uint64_t> offsets;
  while (entry.tell() < size) {
    const auto start = read_offset();
    const auto length = read_offset();
    const auto pad = read_offset();
    table.getULEB128(entry); // Action index.
    if (auto error = entry.takeError()) {
      llvm::consumeError(std::move(error));
      return {};
    }
    if (start > function_size || length > function_size - start || pad >= function_size)
      return {};
    if (pad)
      offsets.push_back(pad);
  }
  return offsets;
}
} // namespace

struct Plan::Image {
  std::string path;
  std::string build_id, sha256;
  bool primary;
  std::optional<std::uint64_t> bias;
  std::uint64_t offline_bias;
  llvm::object::OwningBinary<llvm::object::Binary> executable;
  llvm::object::OwningBinary<llvm::object::Binary> debug;
  std::unique_ptr<llvm::DWARFContext> dwarf;
  std::vector<std::pair<std::uint64_t, std::string_view>> text;
  std::set<Range> functions;
  std::vector<Symbol> symbols;
  mutable std::optional<std::set<std::uint64_t>> delete_plt;
  mutable std::optional<std::map<std::uint64_t, std::set<std::uint64_t>>> exception_entries;

  void index_exception_entries() const {
    exception_entries.emplace();
    const auto &object = llvm::cast<ObjectFile>(*executable.getBinary());
    for (const auto &section : object.sections()) {
      auto name = section.getName();
      if (!name) {
        llvm::consumeError(name.takeError());
        continue;
      }
      if (*name != ".gcc_except_table")
        continue;
      auto table = section.getContents();
      if (!table) {
        llvm::consumeError(table.takeError());
        continue;
      }
      // A separate debug file may not carry the executable's .eh_frame.
      auto unwind = debug.getBinary() ? llvm::DWARFContext::create(object) : nullptr;
      auto frames = (unwind ? *unwind : *dwarf).getEHFrame();
      if (!frames) {
        llvm::consumeError(frames.takeError());
        return;
      }
      if (!*frames)
        return;
      for (const auto &entry : (*frames)->entries()) {
        const auto *fde = llvm::dyn_cast<dw::FDE>(&entry);
        if (!fde || !fde->getLSDAAddress())
          continue;
        const auto lsda = *fde->getLSDAAddress();
        const auto base = section.getAddress();
        if (lsda < base || lsda - base >= table->size())
          continue;
        const auto start = fde->getInitialLocation();
        for (const auto offset : landing_pad_offsets(table->substr(lsda - base), fde->getAddressRange()))
          if (offset <= std::numeric_limits<std::uint64_t>::max() - start)
            (*exception_entries)[start].insert(start + offset);
      }
    }
  }

  std::vector<std::uint64_t> cold_entries(std::uint64_t start, std::size_t size) const {
    std::set<std::string_view> hot_names;
    for (const auto &symbol : symbols) {
      if (symbol.kind != llvm::ELF::STT_FUNC || symbol.range.first != start - *bias)
        continue;
      const std::string_view name = symbol.linkage;
      const auto suffix = name.rfind(".cold");
      if (suffix == name.npos)
        continue;
      const auto number = name.substr(suffix + 5);
      if (number.empty() || (number.size() > 1 && number.front() == '.' &&
          std::ranges::all_of(number.substr(1), [](char ch) { return ch >= '0' && ch <= '9'; })))
        hot_names.insert(name.substr(0, suffix));
    }
    if (hot_names.empty())
      return {};

    std::set<Range> hot;
    for (const auto &symbol : symbols)
      if (symbol.kind == llvm::ELF::STT_FUNC && hot_names.count(symbol.linkage))
        hot.insert(symbol.range);
    if (hot.size() != 1)
      return {};
    const auto [address, length] = *hot.begin();
    const auto hot_start = *bias + address;
    const auto bytes = code(hot_start);
    if (length > bytes.size())
      return {};
    // A named partition alone proves no instruction boundary. Decode the
    // hot range and its LSDA roots, then admit only actual branches to it.
    const auto pads = local_landing_pads(hot_start, length);
    const auto body = native_entry::scan(hot_start, bytes.substr(0, length), pads);
    if (!body.diagnostic.empty() || body.decode_failed)
      return {};
    std::vector<std::uint64_t> entries;
    std::uint64_t end = 0;
    for (const auto &inst : body.instructions) {
      if (inst.address < end)
        return {};
      end = inst.address + inst.size;
      if (inst.flow == native_entry::Flow::branch && inst.target &&
          *inst.target >= start && *inst.target - start < size)
        entries.push_back(*inst.target);
    }
    return entries;
  }

  std::vector<std::uint64_t> local_landing_pads(std::uint64_t start, std::size_t size) const {
    if (!exception_entries)
      index_exception_entries();
    std::vector<std::uint64_t> entries;
    if (const auto found = exception_entries->find(start - *bias); found != exception_entries->end())
      for (const auto address : found->second)
        if (address - found->first < size)
          entries.push_back(*bias + address);
    return entries;
  }

  std::vector<std::uint64_t> exception_roots(std::uint64_t start, std::size_t size) const {
    auto roots = local_landing_pads(start, size);
    const auto cold = cold_entries(start, size);
    roots.insert(roots.end(), cold.begin(), cold.end());
    return roots;
  }

#if defined(__x86_64__)
  void index_ibt_delete_entries(const llvm::object::ELFObjectFileBase &object) const {
    std::set<std::uint64_t> delete_slots;
    for (const auto &section : object.sections()) {
      auto name = section.getName();
      if (!name) {
        llvm::consumeError(name.takeError());
        continue;
      }
      if (*name != ".rela.plt")
        continue;
      for (const auto &relocation : section.relocations()) {
        if (relocation.getType() != llvm::ELF::R_X86_64_JUMP_SLOT)
          continue;
        const auto symbol = relocation.getSymbol();
        if (symbol == object.symbol_end())
          continue;
        auto target = symbol->getName();
        if (!target) {
          llvm::consumeError(target.takeError());
          continue;
        }
        if (operator_delete_name(*target))
          delete_slots.insert(relocation.getOffset());
      }
    }
    if (delete_slots.empty())
      return;

    for (const auto &section : object.sections()) {
      auto name = section.getName();
      if (!name) {
        llvm::consumeError(name.takeError());
        continue;
      }
      if (*name != ".plt.sec" || !section.isText())
        continue;
      auto contents = section.getContents();
      if (!contents) {
        llvm::consumeError(contents.takeError());
        continue;
      }
      // GNU IBT PLT entries are 16-byte ENDBR64 + JMP *disp32(%rip)
      // stubs. Match their GOT slots by relocation, independent of order.
      for (std::size_t offset = 0; offset + 16 <= contents->size(); offset += 16) {
        const auto stub = contents->substr(offset, 16);
        if (!stub.starts_with("\xf3\x0f\x1e\xfa"))
          continue;
        const auto jump = stub[4] == '\xf2' ? 5 : 4; // Optional BND prefix.
        if (!stub.substr(jump).starts_with("\xff\x25"))
          continue;
        std::int32_t displacement;
        std::memcpy(&displacement, stub.data() + jump + 2, sizeof(displacement));
        const auto entry = section.getAddress() + offset;
        if (delete_slots.count(entry + jump + 6 + displacement))
          delete_plt->insert(entry);
      }
    }
  }
#endif

  bool is_delete_target(std::uint64_t address) const {
    if (std::ranges::any_of(symbols, [&](const auto &symbol) {
          return symbol.kind == llvm::ELF::STT_FUNC && symbol.range.first == address &&
              operator_delete_name(symbol.linkage);
        }))
      return true;
    if (!delete_plt) {
      delete_plt.emplace();
      const auto &object = llvm::cast<llvm::object::ELFObjectFileBase>(*executable.getBinary());
#if defined(__x86_64__)
      index_ibt_delete_entries(object);
#endif
      const auto triple = llvm::sys::getDefaultTargetTriple();
      std::string error;
      if (const auto *target = llvm::TargetRegistry::lookupTarget(triple, error)) {
        const std::unique_ptr<llvm::MCSubtargetInfo> sub(target->createMCSubtargetInfo(triple, "generic", ""));
        if (sub)
          for (const auto &entry : object.getPltEntries(*sub)) {
            if (!entry.Symbol)
              continue;
            auto name = llvm::object::SymbolRef(*entry.Symbol, &object).getName();
            if (name && operator_delete_name(*name))
              delete_plt->insert(entry.Address);
            if (!name)
              llvm::consumeError(name.takeError());
          }
      }
    }
    return delete_plt->count(address) != 0;
  }

  Image(std::uint32_t pid, const std::string &module, bool main, bool require_build_id = true)
      : path(module), primary(main), offline_bias(main ? 0 : (1ULL << 48)),
        executable(open_elf(main && pid ? (proc::directory(pid) / "exe").string() : module)) {
    const auto &object = llvm::cast<ObjectFile>(*executable.getBinary());
    build_id = llvm::toHex(llvm::object::getBuildID(&object), true);
#if defined(__aarch64__)
    constexpr auto native_arch = llvm::Triple::aarch64;
#else
    constexpr auto native_arch = llvm::Triple::x86_64;
#endif
    if (object.getArch() != native_arch)
      throw std::runtime_error("object watch requires ELF for the native architecture");
    if (build_id.empty()) {
      if (require_build_id && !main)
        throw std::runtime_error("shared writer requires a GNU build ID");
      const auto image = object.getMemoryBufferRef().getBuffer();
      sha256 = llvm::toHex(llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
          reinterpret_cast<const std::uint8_t *>(image.data()), image.size())), true);
    }
    dwarf = llvm::DWARFContext::create(object);
    if (dwarf->compile_units().empty()) {
      if (const auto matching = client::find_matching_debug_file(path, object)) {
        debug = open_elf(*matching);
        dwarf = llvm::DWARFContext::create(llvm::cast<ObjectFile>(*debug.getBinary()));
      }
    }
    for (const auto &section : object.sections()) {
      if (!section.isText())
        continue;
      auto bytes = section.getContents();
      if (!bytes)
        throw std::runtime_error(llvm::toString(bytes.takeError()));
      text.emplace_back(section.getAddress(), std::string_view(bytes->data(), bytes->size()));
    }
    append_symbols(object, symbols);
    if (debug.getBinary())
      append_symbols(llvm::cast<ObjectFile>(*debug.getBinary()), symbols);
    for (const auto &symbol : symbols)
      if (symbol.kind == llvm::ELF::STT_FUNC)
        functions.insert(symbol.range);
  }

  Range function(std::uint32_t pid, std::string selector, bool allow_virtual = false,
                 bool exclude_clones = false) const {
#if defined(__aarch64__)
    constexpr bool use_elf_selector = true;
#else
    const bool use_elf_selector = !pid;
#endif
    if (use_elf_selector) {
      const auto compact = [](std::string_view text) {
        std::string result;
        for (unsigned char c : text)
          if (!std::isspace(c))
            result += static_cast<char>(c);
        return result;
      };
      const auto normalized = compact(selector);
      return select(symbols, selector, llvm::ELF::STT_FUNC, [&](const auto &s) {
        if (s.linkage == selector)
          return true;
        if (exclude_clones && s.linkage.find('.') != std::string::npos)
          return false;
        const auto symbol = compact(s.demangled);
        return symbol == normalized ||
            (normalized.find('(') == std::string::npos && symbol.starts_with(normalized + "("));
      });
    }
    if (selector.find('(') != std::string::npos || selector.find("::") != std::string::npos) {
      client::DebugLookupOptions options;
      options.module = path;
      options.allow_virtual_entry = allow_virtual;
      selector = client::resolve_debug_callable(pid, selector, options).spec.symbol();
    }
    return select(symbols, selector, llvm::ELF::STT_FUNC,
                  [&](const auto &s) { return s.linkage == selector; });
  }

  std::uint64_t bind(std::uint32_t pid, Range range) {
    const auto &object = llvm::cast<ObjectFile>(*executable.getBinary());
    const auto live = pid ? client::bind_live_executable(pid, path, object, range.first, range.second, primary)
                          : offline_bias;
    if (bias && live != *bias)
      throw std::runtime_error("object watch anchors disagree on the live load bias");
    bias = live;
    return live + range.first;
  }

  Source source(std::uint64_t pc) const;
  void describe(std::uint64_t pc, const Source &source,
                clangtracer::v1::ObjectObservation &event) const;

  std::string_view code(std::uint64_t pc) const {
    if (bias && pc >= *bias) {
      const auto address = pc - *bias;
      for (const auto &[start, bytes] : text)
        if (address >= start && address - start < bytes.size())
          return bytes.substr(address - start);
    }
    return {};
  }
};

std::pair<std::uint64_t, std::size_t> Plan::application_range(
    std::uint32_t pid, const std::string &selector) const {
  auto &image = main_ ? *main_ : *image_;
  const auto range = image.function(pid, selector);
  return {image.bind(pid, range), range.second};
}

Plan::Plan(std::uint32_t pid, std::string executable,
           const clangtracer::v1::ObjectWatchRequest &request)
    : module(writer_module(pid, executable, request.code_module())) {
  using Request = clangtracer::v1::ObjectWatchRequest;
  if (request.follow_calls() && (request.backend() != Request::CODE_CACHE ||
                                request.mode() != Request::WRITES))
    throw std::invalid_argument("runtime call following requires code-cache write capture");
  if (!request.pointee_field().empty() &&
      (request.pointee_field().size() > 512 || request.pointee_field().find('\0') != std::string::npos ||
       !request.first_seen() || request.mutation_symbol().empty() || request.writer_entry() ||
       !request.vector_field().empty() || !request.map_field().empty() ||
       !request.owner_type().empty() || !request.code_module().empty() || !request.union_tag().empty() ||
       (request.synchronization() != Request::THREAD_CONFINED &&
        request.synchronization() != Request::CALLER_LOCK)))
    throw std::invalid_argument("pointee capture requires a standalone writer, mutation and synchronization policy");
  source_writers = request.source_writers();
  image_ = std::make_unique<Image>(pid, module, module == executable);
  if (module != executable)
    main_ = std::make_unique<Image>(pid, executable, true);
  const auto writer = image_->function(pid, request.code_symbol(),
      request.backend() == clangtracer::v1::ObjectWatchRequest::CODE_CACHE);
  writer_start = image_->bind(pid, writer);
  bias = *image_->bias;
  const auto writer_debug = image_->dwarf->getDIEsForAddress(writer.first);
  const auto &concrete = request.concrete_type();
  const auto leaf = leaf_name(concrete);
  const auto owns_lifetime = [&](const Image &image) {
    return std::ranges::any_of(image.symbols, [&](const auto &symbol) {
      return symbol.kind == llvm::ELF::STT_FUNC &&
          symbol.demangled.starts_with(concrete + "::" + leaf + "(") &&
          (symbol.linkage.contains("C1E") || symbol.linkage.contains("C2E"));
    });
  };
  const bool standalone_shared = main_ && request.borrow_caller().empty() &&
      request.owner_type().empty() && !request.first_seen();
  const bool shared_lifetime = standalone_shared && owns_lifetime(*image_) && !owns_lifetime(*main_);
  auto &lifetime = main_ && !shared_lifetime ? *main_ : *image_;
  llvm::DWARFUnit *layout_unit = writer_debug.CompileUnit;
  if (!layout_unit) {
    for (const auto variant : {"C1E", "C2E"}) {
      std::set<llvm::DWARFUnit *> constructor_units;
      for (const auto &symbol : image_->symbols) {
        if (symbol.kind != llvm::ELF::STT_FUNC ||
            !symbol.demangled.starts_with(concrete + "::" + leaf + "(") ||
            !symbol.linkage.contains(variant))
          continue;
        if (const auto entry = image_->dwarf->getDIEsForAddress(symbol.range.first); entry.CompileUnit)
          constructor_units.insert(entry.CompileUnit);
      }
      if (!constructor_units.empty()) {
        if (constructor_units.size() == 1)
          layout_unit = *constructor_units.begin();
        break;
      }
    }
  }
  // Standalone shared code can reach the field through an alias without
  // defining the record. The executable establishes its lifetime and layout.
  if (standalone_shared && request.follow_calls() && !shared_lifetime)
    layout = inspect_layout(*lifetime.dwarf, request, nullptr, *lifetime.dwarf);
  else
    layout = inspect_layout(*image_->dwarf, request, layout_unit, *lifetime.dwarf);
  if (!request.shared_file().empty() &&
      (request.backend() != Request::CODE_CACHE || request.mode() != Request::WRITES ||
       (!layout.atomic && request.synchronization() != Request::CALLER_LOCK) ||
       layout.opaque_union || request.first_seen() || !request.borrow_caller().empty() ||
       !request.owner_type().empty()))
    throw std::invalid_argument("shared-file capture requires a native atomic int or a scalar under caller-lock");
  if (!request.additional_mutation_symbols().empty() &&
      (!layout.logical_collection() || request.mutation_symbol().empty()))
    throw std::invalid_argument("multiple mutation selectors require a logical collection");
  if (request.serialize_wait_us() &&
      (request.backend() != clangtracer::v1::ObjectWatchRequest::CODE_CACHE || request.writer_tid() ||
       layout.atomic || layout.opaque_union || layout.union_tag_offset || request.first_seen()))
    throw std::invalid_argument("write ordering requires code-cache capture of a standalone scalar and all watched threads");
  if (request.syscall_output() &&
      (request.backend() != clangtracer::v1::ObjectWatchRequest::PTRACE || layout.atomic ||
       layout.opaque_union || layout.union_tag_offset || request.first_seen() ||
       !request.owner_type().empty() ||
       (request.synchronization() != clangtracer::v1::ObjectWatchRequest::THREAD_CONFINED &&
        request.synchronization() != clangtracer::v1::ObjectWatchRequest::CALLER_LOCK)))
    throw std::invalid_argument("syscall output requires diagnostic capture of a synchronized standalone scalar field");
  if (!request.additional_code_symbols().empty() &&
      (request.first_seen() || !request.borrow_caller().empty() ||
       !request.owner_type().empty() || shared_writer()))
    throw std::invalid_argument("multiple writers require a standalone main-image field");
  if (!request.owner_type().empty() || layout.primary_rtti) {
    const auto rtti_name = "typeinfo for " + request.concrete_type();
    const auto rtti = select(image_->symbols, rtti_name, llvm::ELF::STT_OBJECT,
        [&](const auto &s) { return s.demangled == rtti_name; });
    if (rtti.first > std::numeric_limits<std::uint64_t>::max() - bias)
      throw std::runtime_error("object watch typeinfo address overflow");
    typeinfo = bias + rtti.first;
  }

  const auto writer_derived = derived_types(*image_->dwarf, concrete);
  const auto main_derived = main_ ? derived_types(*main_->dwarf, concrete) : DerivedTypes{};
  const bool known_derived = writer_derived.present || main_derived.present;
  const auto &owner = request.owner_type();
  for (const auto *selector : {&request.owner_add(), &request.owner_get(), &request.owner_remove(),
                               &request.owner_snapshot()})
    if (!selector->empty() && (owner.empty() || selector->size() > 512 || selector->find('\0') != selector->npos))
      throw std::invalid_argument("ownership selectors require an owner and a valid callable selector");
  if ((!request.owner_snapshot().empty() && request.snapshot_field().empty()) ||
      (!request.owner_remove().empty() && !request.borrow_caller().empty()))
    throw std::invalid_argument("ownership selector has no enabled boundary in this watch");
  const auto install_boundary = [&](Image &boundary_image, Boundary role, const Range &range,
                                    const std::string &prefix, bool uncertain) {
    const auto pc = boundary_image.bind(pid, range);
    if (const auto prior = boundaries.find(pc); prior != boundaries.end()) {
      if (prior->second == range.second && sites.at(pc).boundary == role)
        return pc;
      throw std::runtime_error("conflicting object watch boundaries share an ELF entry");
    }
    boundaries.emplace(pc, range.second);
    const auto decoded = native_entry::scan(pc, code(pc).substr(0, std::min<std::uint64_t>(range.second, 15)));
    if (decoded.instructions.empty() ||
        !sites.try_emplace(pc, Site{std::string(code(pc).substr(0, decoded.instructions.front().size)),
                                   role, std::nullopt, uncertain}).second)
      throw std::runtime_error("cannot admit object watch boundary: " + prefix);
    return pc;
  };
  const auto add_boundary = [&](Image &boundary_image, Boundary role, const std::string &prefix,
                                std::string_view variant = {}, bool required = true) {
    const auto matches_prefix = [&](const Symbol &symbol) {
      return symbol.kind == llvm::ELF::STT_FUNC && symbol.demangled.starts_with(prefix + "(") &&
          symbol.linkage.find('.') == std::string::npos;
    };
    const auto base_variant = variant == "C1E" ? "C2E" : variant == "D1Ev" ? "D2Ev" : "";
    std::set<Range> ranges, base_aliases;
    std::set<std::string_view> complete_overloads;
    for (const auto &symbol : boundary_image.symbols) {
      if (!matches_prefix(symbol))
        continue;
      if (variant.empty() || symbol.linkage.contains(variant)) {
        ranges.insert(symbol.range);
        complete_overloads.insert(symbol.demangled);
      }
      if (known_derived && role == Boundary::construct &&
          symbol.linkage.find("C2E") != std::string::npos)
        base_aliases.insert(symbol.range);
    }
    if (*base_variant) {
      // Clang may emit only the base variant for an inline definition. It
      // has the complete-object ABI only when there is no virtual base/VTT.
      const auto missing_complete = [&](const Symbol &symbol) {
        return matches_prefix(symbol) && symbol.linkage.contains(base_variant) &&
            !complete_overloads.contains(symbol.demangled);
      };
      if (std::ranges::any_of(boundary_image.symbols, missing_complete)) {
        std::set<DWARFDie> seen;
        auto *unit = &boundary_image == image_.get() ? layout_unit : nullptr;
        if (!has_virtual_bases(find_record(*boundary_image.dwarf, concrete, unit), seen))
          for (const auto &symbol : boundary_image.symbols)
            if (missing_complete(symbol))
              ranges.insert(symbol.range);
      }
    }
    if ((required && ranges.empty()) || (role != Boundary::construct && ranges.size() > 1))
      throw std::runtime_error("object watch boundary is missing or ambiguous: " + prefix);
    for (const auto &range : ranges) {
      const auto pc = install_boundary(boundary_image, role, range, prefix, base_aliases.count(range) != 0);
      if (variant == "D0Ev") {
        virtual_destructor = true;
        sites.at(pc).deleting_destructor = true;
      }
    }
  };
  const auto ownership_boundary = [&](Boundary role, const std::string &selector, const char *default_name) {
    const auto receiver = role == Boundary::snapshot ? request.base_type() : owner;
    const auto name = selector.empty() ? receiver + "::" + default_name : selector;
    const auto range = lifetime.function(0, name, false, true);
    auto function = lifetime.dwarf->getDIEsForAddress(range.first).FunctionDIE;
    while (function && function.getTag() != dw::DW_TAG_subprogram)
      function = function.getParent();
    const auto referenced_type = [](DWARFDie die) {
      const auto value = die.findRecursively({dw::DW_AT_type});
      auto result = value ? die.getAttributeValueAsReferencedDie(*value) : DWARFDie{};
      while (result && (result.getTag() == dw::DW_TAG_const_type || result.getTag() == dw::DW_TAG_typedef))
        result = type(result);
      return result;
    };
    const auto refers_to = [&](DWARFDie die, const std::string &record_name) {
      return die && (pointer(die) || die.getTag() == dw::DW_TAG_reference_type ||
          die.getTag() == dw::DW_TAG_rvalue_reference_type) && qualified(type(die)) == record_name;
    };
    const auto result = referenced_type(function);
    const auto result_is_base = refers_to(result, request.base_type());
    bool valid = writer_receiver(function, range.first, receiver, role != Boundary::snapshot);
    const auto convention = function.findRecursively({dw::DW_AT_calling_convention});
    valid &= !convention || convention->getAsUnsignedConstant() == dw::DW_CC_normal;
    if (role == Boundary::get)
      valid &= result_is_base;
    if (role == Boundary::remove)
      valid &= storage_type(result) == "bool";
    if (role == Boundary::add) {
      valid &= !result || result_is_base;
      unsigned index = 0;
      bool ownership_argument = false;
      for (const auto parameter : function.children()) {
        if (parameter.getTag() != dw::DW_TAG_formal_parameter || ++index != 2)
          continue;
        auto argument = referenced_type(parameter);
        ownership_argument = pointer(argument) && refers_to(argument, request.base_type()) &&
            request.backend() == Request::CODE_CACHE;
        if (argument && (argument.getTag() == dw::DW_TAG_reference_type ||
                         argument.getTag() == dw::DW_TAG_rvalue_reference_type))
          argument = type(argument);
        if (record(argument) && qualified(argument).starts_with("std::unique_ptr<")) {
          std::set<std::uint64_t> offsets;
          find_pointee(argument, request.base_type(), 0, offsets, 32);
          ownership_argument |= offsets == std::set<std::uint64_t>{layout.pointee} &&
              size_of(argument) == sizeof(std::uint64_t) &&
              number(argument, dw::DW_AT_calling_convention) != dw::DW_CC_pass_by_value;
        }
        break;
      }
      valid &= ownership_argument;
    }
    if (!valid)
      throw std::runtime_error("ownership boundary has an incompatible receiver or callable ABI: " + name);
    install_boundary(lifetime, role, range, name, false);
  };
  if (!request.borrow_caller().empty()) {
    add_boundary(*image_, Boundary::construct, concrete + "::" + leaf, "C1E");
    add_boundary(*image_, Boundary::destroy, concrete + "::~" + leaf, "D1Ev");
    add_boundary(*image_, Boundary::destroy, concrete + "::~" + leaf, "D0Ev");
    ownership_boundary(Boundary::add, request.owner_add(), "add");
    ownership_boundary(Boundary::get, request.owner_get(), "get");
    const auto caller = lifetime.function(pid, request.borrow_caller());
    caller_start = lifetime.bind(pid, caller);
    caller_bytes = code(caller_start).substr(0, caller.second);
    if (caller_bytes.size() != caller.second)
      throw std::runtime_error("borrow caller exceeds its verified ELF text");
  } else if (!request.first_seen() || request.backend() == clangtracer::v1::ObjectWatchRequest::PTRACE) {
    add_boundary(lifetime, Boundary::construct, concrete + "::" + leaf, "C1E", !request.first_seen());
    add_boundary(lifetime, Boundary::destroy, concrete + "::~" + leaf, "D1Ev", !request.first_seen());
    add_boundary(lifetime, Boundary::destroy, concrete + "::~" + leaf, "D0Ev", false);
    if (!request.owner_type().empty()) {
      ownership_boundary(Boundary::add, request.owner_add(), "add");
      ownership_boundary(Boundary::get, request.owner_get(), "get");
      ownership_boundary(Boundary::remove, request.owner_remove(), "remove");
      if (!request.no_values() && !request.snapshot_field().empty())
        ownership_boundary(Boundary::snapshot, request.owner_snapshot(), "set_description");
    }
  }
  for (const auto &[pc, size] : boundaries) {
    if (sites.at(pc).boundary != Boundary::construct)
      continue;
    const auto body = native_entry::scan(pc, code(pc).substr(0, size));
    if (body.decode_failed || !body.diagnostic.empty() || body.instructions.empty())
      continue;
    const auto &tail = body.instructions.back();
    if (tail.flow != native_entry::Flow::branch || tail.falls_through ||
        !tail.target || *tail.target == pc)
      continue;
    const auto target = sites.find(*tail.target);
    if (target == sites.end() || target->second.boundary != Boundary::construct)
      continue;
    if (sites.at(pc).uncertain_complete && !target->second.uncertain_complete)
      continue;
    // A straight-line delegate preserves the receiver and original return
    // context. A call to the thunk supplies the return PC at its target.
    auto begin = body.instructions.begin();
    auto end = std::prev(body.instructions.end());
#if defined(__x86_64__)
    // Clang can wrap the argument setup in a frame-pointer prologue and
    // matching pop. The tail still inherits the caller's stack and return PC.
    if (end - begin >= 3 && code(begin->address).substr(0, begin->size) == "\x55" &&
        code((begin + 1)->address).substr(0, (begin + 1)->size) == "\x48\x89\xe5" &&
        code((end - 1)->address).substr(0, (end - 1)->size) == "\x5d") {
      begin += 2;
      --end;
    }
#endif
    if (std::all_of(begin, end, [](const auto &inst) {
          return inst.flow == native_entry::Flow::ordinary && inst.falls_through &&
              inst.cache_copyable && !inst.changes_receiver && !inst.changes_stack && !inst.may_store;
        }))
      delegating_constructor_tails.emplace(pc, *tail.target);
  }
  if (known_derived && !writer_derived.may_construct_base_directly &&
      !main_derived.may_construct_base_directly) {
    std::vector<std::uint64_t> aliases;
    for (const auto &[pc, site] : sites)
      if (site.boundary == Boundary::construct &&
          (site.uncertain_complete || delegating_constructor_tails.count(pc)))
        aliases.push_back(pc);
    // This lookup only adds evidence for ambiguous constructors. It does
    // not install redirects or require callers to be exhaustively known.
    for (const auto &[entry, calls] : direct_redirects(aliases, true, false)) {
      for (const auto call : calls) {
        const auto *image = image_for(call);
        const auto address = call - *image->bias;
        const auto debug = image->dwarf->getDIEsForAddress(address);
        if (!debug.CompileUnit)
          continue;
        llvm::SmallVector<DWARFDie, 4> chain;
        debug.CompileUnit->getInlinedChainForAddress(address, chain);
        // Aggregate initialization can call a base constructor directly. Only
        // classify callers when DWARF also rules out known aggregate candidates.
        if (chain.empty() || std::ranges::any_of(chain, [](const auto die) {
              const auto *linkage = die.getName(llvm::DINameKind::LinkageName);
              if (!linkage)
                return true;
              llvm::ItaniumPartialDemangler parsed;
              return parsed.partialDemangle(linkage) ? std::string_view(linkage).starts_with("_Z") :
                  parsed.isCtorOrDtor();
            }))
          continue;
        if (const auto target = constructor_tail_target(entry))
          complete_constructor_returns.emplace(target, call + object_cache::direct_call_width);
        complete_constructor_returns.emplace(entry, call + object_cache::direct_call_width);
      }
    }
  }
  if (request.first_seen()) {
    if (!writer_receiver(writer_debug.FunctionDIE, writer.first, concrete))
      throw std::runtime_error("first-seen writer requires a DWARF-proven receiver in the first argument");
    install_boundary(*image_, Boundary::writer_scope, writer, request.code_symbol(), false);
    if (layout.logical_collection()) {
      auto container_name = concrete;
      if (layout.pointer_offsets.empty()) {
        const auto container = member(find_record(*image_->dwarf, concrete, writer_debug.CompileUnit),
                                      layout.logical_map ? request.map_field() : request.vector_field());
        container_name = qualified(container.type);
      }
      const auto writer_body = native_entry::scan(writer_start, code(writer_start).substr(0, writer.second));
      std::set<Range> mutations;
      const auto add_mutation = [&](const std::string &selector) {
        if (selector.empty() || selector.size() > 512 || selector.find('\0') != std::string::npos)
          throw std::invalid_argument("invalid collection mutation selector");
        std::optional<Range> mutation;
        try {
          mutation = image_->function(0, selector);
        } catch (const std::runtime_error &) {
          // A selected mutation may exist only as DWARF inline instances.
        }
        const bool direct = mutation && std::ranges::any_of(writer_body.instructions, [&](const auto &inst) {
          return inst.flow == native_entry::Flow::call && inst.target == bias + mutation->first;
        });
        if (!direct) {
          const auto inlined = inline_mutations_in(writer_debug.FunctionDIE, bias, selector,
                                                   container_name, writer_body);
          for (const auto &region : inlined) {
            if (std::ranges::any_of(inline_mutations, [&](const auto &prior) {
                  const auto &other = prior.second;
                  const auto reaches = [](const auto &range, const auto &continuations) {
                    const auto next = std::ranges::lower_bound(continuations, range.first);
                    return next != continuations.end() && *next < range.second;
                  };
                  return std::ranges::any_of(region.continuations, [&](const auto pc) {
                    return std::ranges::binary_search(other.continuations, pc);
                  }) || std::ranges::any_of(other.ranges, [&](const auto &previous) {
                    return reaches(previous, region.continuations);
                  }) || std::ranges::any_of(region.ranges, [&](const auto &range) {
                    return reaches(range, other.continuations) ||
                        std::ranges::any_of(other.ranges, [&](const auto &previous) {
                          return range.first < previous.second && previous.first < range.second;
                        });
                  });
                }))
              throw std::runtime_error("inlined collection mutation boundaries overlap");
            inline_mutations.emplace(region.begin, region);
            const auto install_anchor = [&](std::uint64_t pc) {
              const auto inst = std::ranges::find_if(writer_body.instructions, [&](const auto &candidate) {
                return candidate.address == pc;
              });
              auto &site = sites.try_emplace(pc, Site{std::string(code(pc).substr(0, inst->size)),
                                                     std::nullopt, std::nullopt}).first->second;
              site.inline_mutation = region.begin;
            };
            install_anchor(region.begin);
            for (const auto pc : region.continuations)
              install_anchor(pc);
          }
          if (!inlined.empty())
            return;
        }
        if (!mutation)
          throw std::runtime_error("no verified collection mutation function or inline range: " + selector);
        if (!mutations.insert(*mutation).second)
          throw std::invalid_argument("collection mutation selectors resolve to the same function");
        const auto debug = image_->dwarf->getDIEsForAddress(mutation->first);
        if (!writer_receiver(debug.FunctionDIE, mutation->first, container_name,
                             layout.logical_map))
          throw std::runtime_error("mutation must take the same verified container as its first argument: " +
                                   selector);
        install_boundary(*image_, Boundary::collection_mutation, *mutation, selector, false);
      };
      add_mutation(request.mutation_symbol());
      for (const auto &selector : request.additional_mutation_symbols())
        add_mutation(selector);
    }
  }

  const auto add_writer = [&](std::uint64_t start, std::size_t length) {
    const auto bytes = code(start).substr(0, length);
    if (bytes.size() != length || length > UINT64_MAX - start)
      throw std::runtime_error("writer function exceeds its ELF text section");
    if (std::ranges::any_of(writer_ranges, [&](const auto &prior) {
          return prior.first < start + length && start < prior.first + prior.second.size();
        }))
      throw std::invalid_argument("selected writer functions overlap");
    writer_ranges.emplace_back(start, bytes);
    const auto decoded = native_entry::scan(start, bytes);
    if (decoded.decode_failed || !decoded.diagnostic.empty())
      throw std::runtime_error("cannot decode the selected writer function");
    auto previous_end = start;
    const auto indirect = std::ranges::find_if(decoded.instructions, [](const auto &instruction) {
      return instruction.flow == native_entry::Flow::branch && !instruction.target;
    });
    const bool has_indirect = indirect != decoded.instructions.end();
    bool unclassified_bytes = false;
    const auto is_padding = [&](std::uint64_t begin, std::uint64_t end) {
      if (begin == end)
        return true;
#if defined(__x86_64__)
      const auto size = end - begin;
      return native_entry::nop_prefix(begin, bytes.substr(begin - start, size), size) == size;
#else
      constexpr std::string_view nop{"\x1f\x20\x03\xd5", 4};
      for (; begin < end && bytes.substr(begin - start, 4) == nop; begin += 4) {}
      return begin == end;
#endif
    };
    const auto prior_count = stores;
    for (const auto &instruction : decoded.instructions) {
      if (instruction.address < previous_end)
        throw std::runtime_error("overlapping instruction boundaries in writer function");
      if (has_indirect && request.backend() == Request::PTRACE && !unclassified_bytes)
        unclassified_bytes = !is_padding(previous_end, instruction.address);
      previous_end = instruction.address + instruction.size;
      if (!instruction.store32 || !instruction.store32->supports(layout.atomic)) {
        if (instruction.may_store && instruction.flow == native_entry::Flow::ordinary)
          ++unsupported;
        continue;
      }
      if (auto existing = sites.find(instruction.address); existing != sites.end()) {
        if ((existing->second.boundary && existing->second.boundary != Boundary::writer_scope) ||
            existing->second.bytes.size() != instruction.size)
          throw std::runtime_error("store overlaps an object watch boundary");
        existing->second.store = instruction.store32;
      } else {
        sites.emplace(instruction.address,
            Site{std::string(code(instruction.address).substr(0, instruction.size)),
                 std::nullopt, instruction.store32});
      }
      ++stores;
    }
    // Ptrace leaves the indirect transfer in place. Unreached bytes must be
    // verified alignment NOPs so that a jump table cannot hide writer blocks.
    if (has_indirect && (request.backend() != Request::PTRACE || unclassified_bytes ||
                         !is_padding(previous_end, start + length)))
      throw std::runtime_error("object watch cannot verify an indirect branch in " +
                               source(start).function + ": " + indirect->text);
    if (stores == prior_count && !request.follow_calls())
      throw std::runtime_error("no supported 32-bit store in " + source(start).function);
  };
  add_writer(writer_start, writer.second);
  writer_bytes = writer_ranges.front().second;
  for (const auto &selector : request.additional_code_symbols()) {
    if (selector.empty() || selector.size() > 512 || selector.find('\0') != std::string::npos)
      throw std::invalid_argument("invalid additional writer selector");
    const auto extra = image_->function(pid, selector);
    if (auto *unit = image_->dwarf->getDIEsForAddress(extra.first).CompileUnit) {
      const auto other = inspect_layout(*image_->dwarf, request, unit, *lifetime.dwarf);
      if (other.scalar != layout.scalar || other.atomic != layout.atomic ||
          other.opaque_union != layout.opaque_union || other.union_tag_offset != layout.union_tag_offset ||
          other.union_cases != layout.union_cases)
        throw std::runtime_error("selected writers disagree on the verified field layout");
    }
    add_writer(image_->bind(pid, extra), extra.second);
  }
  if (request.follow_calls()) {
    using Flow = native_entry::Flow;
    const auto limit = request.runtime_limit() ? request.runtime_limit() : object_cache::default_runtime_limit;
    std::set<std::uint64_t> inspected;
    for (const auto &[start, bytes] : writer_ranges)
      inspected.insert(start);
    // Leave known nonleaf functions in their original mappings. They can
    // have independent store and call-site redirects with real return PCs.
    const auto explicit_scopes = writer_ranges.size();
    for (std::size_t index = 0; index < writer_ranges.size(); ++index) {
      const auto [start, bytes] = writer_ranges[index];
      const auto body = native_entry::scan(start, bytes);
      for (const auto &call : body.instructions) {
        if (call.flow != Flow::call)
          continue;
        static_call_scopes.insert(start);
        if (!call.target || image_for(*call.target) != image_.get() ||
            writer_ranges.size() - explicit_scopes >= limit ||
            !inspected.insert(*call.target).second)
          continue;
        const auto [callee, size] = containing_function(*call.target);
        if (callee != *call.target || !size || size > UINT64_MAX - callee)
          continue;
        const auto overlaps = [&](std::uint64_t pc, std::size_t width) {
          return pc < callee + size && callee < pc + width;
        };
        if (std::ranges::any_of(boundaries, [&](const auto &item) {
              return overlaps(item.first, item.second);
            }) || std::ranges::any_of(writer_ranges, [&](const auto &item) {
              return overlaps(item.first, item.second.size());
            }))
          continue;
        const auto callee_bytes = code(callee).substr(0, size);
        if (callee_bytes.size() != size)
          continue;
        const auto child = native_entry::scan(callee, callee_bytes);
        if (child.decode_failed || !child.diagnostic.empty() ||
            std::ranges::any_of(child.instructions, [](const auto &inst) {
              return inst.flow == Flow::branch && !inst.target;
            }) || std::ranges::none_of(child.instructions, [](const auto &inst) {
              return inst.flow == Flow::call;
            }))
          continue;
        try {
          image_->bind(pid, {callee - bias, size});
        } catch (const std::runtime_error &) {
          continue;
        }
        add_writer(callee, size);
      }
    }
  }
  std::uint64_t previous_end = 0;
  for (const auto &[pc, site] : sites) {
    if (pc < previous_end)
      throw std::runtime_error("object watch sites overlap");
    previous_end = pc + site.bytes.size();
  }
}

Plan::~Plan() = default;

namespace {
using Redirects = std::map<std::uint64_t, std::vector<std::uint64_t>>;
enum class DirectEdges { redirects, calls, branches };

void find_direct_redirects(const auto &callers, Redirects &result, DirectEdges edges,
                           bool require_complete = true) {
  if (result.empty() || !callers.bias)
    return;
  for (const auto &[address, size] : callers.functions) {
    const auto start = *callers.bias + address;
    const auto bytes = callers.code(start);
    if (size > bytes.size())
      continue;
    // Keep the raw matches until after decoding. A matching opcode outside
    // the decoded graph could be a call in a switch arm or an EH landing pad.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> candidates;
    const auto record_candidate = [&](std::size_t offset, std::uint64_t target, bool call) {
      if ((call ? edges != DirectEdges::branches : edges != DirectEdges::calls) && result.count(target))
        candidates.emplace_back(start + offset, target);
    };
#if defined(__aarch64__)
    for (std::size_t i = 0; i + 4 <= size; i += 4) {
      std::uint32_t word;
      std::memcpy(&word, bytes.data() + i, sizeof(word));
      unsigned bits = 0, shift = 5;
      if ((word & 0x7c000000U) == 0x14000000U) {
        bits = 26;
        shift = 0;
      } else if ((word & 0xff000010U) == 0x54000000U ||
                 (word & 0x7e000000U) == 0x34000000U) {
        bits = 19;
      } else if ((word & 0x7e000000U) == 0x36000000U) {
        bits = 14;
      }
      if (bits) {
        const auto sign = std::int64_t{1} << (bits - 1);
        const auto offset = (word >> shift) & (2 * sign - 1);
        const bool call = (word & 0xfc000000U) == 0x94000000U;
        record_candidate(i, start + i + 4 * ((offset ^ sign) - sign), call);
      }
    }
#else
    for (std::size_t i = 0; i + 2 <= size; ++i) {
      const auto opcode = static_cast<unsigned char>(bytes[i]);
      const auto second = static_cast<unsigned char>(bytes[i + 1]);
      if (opcode == 0xeb || (opcode >= 0x70 && opcode <= 0x7f) ||
          (opcode >= 0xe0 && opcode <= 0xe3)) {
        record_candidate(i, start + i + 2 + static_cast<std::int8_t>(second), false);
      } else {
        const bool near_cond = opcode == 0x0f && second >= 0x80 && second <= 0x8f;
        const auto end = i + (near_cond ? 6 : 5);
        if (end <= size && (near_cond || opcode == 0xe8 || opcode == 0xe9)) {
          std::int32_t displacement;
          std::memcpy(&displacement, bytes.data() + end - 4, sizeof(displacement));
          record_candidate(i, start + end + displacement, opcode == 0xe8);
        }
      }
    }
#endif
    if (candidates.empty())
      continue;
    auto decoded = native_entry::scan(start, bytes.substr(0, size));
    if (require_complete) {
      const auto unverified_site = [&]() -> std::optional<std::uint64_t> {
        auto inst = decoded.instructions.begin();
        for (const auto &[pc, target] : candidates) {
          while (inst != decoded.instructions.end() && inst->address + inst->size <= pc)
            ++inst;
          // An opcode embedded in a verified instruction is not a call site.
          if (inst == decoded.instructions.end() || inst->address > pc ||
              (inst->address == pc && inst->target != target))
            return pc;
        }
        return std::nullopt;
      };
      auto unverified = unverified_site();
      if (unverified) {
        const auto landing_pads = callers.exception_roots(start, size);
        if (!landing_pads.empty()) {
          decoded = native_entry::scan(start, bytes.substr(0, size), landing_pads);
          unverified = unverified_site();
        }
      }
      if (unverified) {
        const auto function = callers.source(start).function;
        std::ostringstream message;
        message << "cannot verify a potential code-cache call or branch at ELF address 0x"
                << std::hex << *unverified - *callers.bias;
        if (!function.empty())
          message << " in " << function;
        throw std::runtime_error(message.str());
      }
    }
    std::uint64_t previous_end = 0;
    for (const auto &inst : decoded.instructions) {
      if (inst.address < previous_end)
        throw std::runtime_error("overlapping decoded instructions in a cache redirect caller");
      previous_end = inst.address + inst.size;
      if (!inst.target || !result.count(*inst.target))
        continue;
      if (edges == DirectEdges::branches) {
        if (inst.flow == native_entry::Flow::branch)
          result.at(*inst.target).push_back(inst.address);
        continue;
      }
      if (edges == DirectEdges::calls && inst.flow != native_entry::Flow::call)
        continue;
      if (inst.flow == native_entry::Flow::branch && *inst.target == start)
        continue; // A back edge within the selected function is translated.
#if defined(__aarch64__)
      if (inst.flow == native_entry::Flow::call || inst.flow == native_entry::Flow::branch)
        result.at(*inst.target).push_back(inst.address);
#else
      const char opcode = bytes[inst.address - start];
      if ((inst.flow == native_entry::Flow::call && opcode == '\xe8' && inst.size == 5) ||
          (inst.flow == native_entry::Flow::branch && opcode == '\xe9' && inst.size == 5))
        result.at(*inst.target).push_back(inst.address);
      else if (inst.flow == native_entry::Flow::branch || inst.flow == native_entry::Flow::call)
        throw std::runtime_error("unsupported direct edge into a code-cache function");
#endif
    }
  }
}
}

std::map<std::uint64_t, std::vector<std::uint64_t>>
Plan::direct_redirects(const std::vector<std::uint64_t> &entries, bool calls_only,
                       bool require_complete) const {
  Redirects result;
  for (auto entry : entries)
    result.try_emplace(entry);
  const auto edges = calls_only ? DirectEdges::calls : DirectEdges::redirects;
  find_direct_redirects(*image_, result, edges, require_complete);
  if (main_)
    find_direct_redirects(*main_, result, edges, require_complete);
  for (auto &[entry, sites] : result) {
    std::sort(sites.begin(), sites.end());
    sites.erase(std::unique(sites.begin(), sites.end()), sites.end());
  }
  return result;
}

std::string Plan::runtime_scope_code(const std::string &path,
    std::uint64_t expected_bias, std::uint64_t start, const std::array<std::uint8_t, 32> &sha256,
    std::size_t width,
    const std::function<std::vector<std::uint64_t>(std::string_view)> &inspect) const {
  if (start < expected_bias)
    throw std::runtime_error("runtime scope entry is outside its mapped image");
  Image runtime(0, path, false, false);
  const auto bytes = runtime.executable.getBinary()->getMemoryBufferRef().getBuffer();
  if (llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
          reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size())) != sha256)
    throw std::runtime_error("runtime scope ELF identity changed");
  const auto range = select(runtime.symbols, "runtime scope entry", llvm::ELF::STT_FUNC,
      [&](const auto &symbol) { return symbol.range.first == start - expected_bias; });
  runtime.bias = expected_bias;
  if (range.second != width || range.second > runtime.code(start).size())
    throw std::runtime_error("runtime scope is outside verified executable text");
  Redirects entries;
  const auto original = runtime.code(start).substr(0, range.second);
  for (const auto pc : inspect(original))
    entries.try_emplace(pc);
  find_direct_redirects(runtime, entries, DirectEdges::redirects);
  if (std::ranges::any_of(entries, [](const auto &item) { return !item.second.empty(); }))
    throw std::runtime_error("an application edge enters a runtime scope redirect");
  return std::string(original);
}

std::string_view Plan::code(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  return image ? image->code(pc) : std::string_view{};
}

#if defined(__x86_64__)
bool Plan::writer_address_taken(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  if (!image)
    return false;
  const auto address = pc - *image->bias;
  const auto function = image->dwarf->getDIEsForAddress(address).FunctionDIE;
  if (function) {
    const auto virtuality = function.findRecursively({dw::DW_AT_virtuality});
    if (virtuality && virtuality->getAsUnsignedConstant().value_or(0))
      return true;
  }

  const auto &elf = llvm::cast<llvm::object::ELF64LEObjectFile>(*image->executable.getBinary());
  // In an executable with fixed addresses, the linker can resolve pointer
  // constants completely (including immediates and read-only vtable slots).
  const auto pointer = std::string_view(reinterpret_cast<const char *>(&address), sizeof(address));
  const auto &object = static_cast<const llvm::object::ELFObjectFileBase &>(elf);
  const bool fixed = object.getEType() == llvm::ELF::ET_EXEC;
  for (const auto &[start, bytes] : image->text) {
    // Absolute immediates can materialize an entry address in registers or
    // memory. A byte match only requests an independently verified entry
    // window. It never authorizes modifying the instruction with the match.
    if (fixed && bytes.find(pointer.substr(0, address <= UINT32_MAX ? 4 : 8)) != bytes.npos)
      return true;
    for (std::size_t i = 0; i + 6 <= bytes.size(); ++i) {
      if (bytes[i] != '\x8d' || (bytes[i + 1] & 0xc7) != 0x05)
        continue;
      std::int32_t displacement;
      std::memcpy(&displacement, bytes.data() + i + 2, sizeof(displacement));
      if (start + i + 6 + displacement == address)
        return true;
    }
  }

  for (const auto &section : elf.sections()) {
    const llvm::object::ELFSectionRef metadata(section);
    if (!(metadata.getFlags() & llvm::ELF::SHF_ALLOC))
      continue;
    if (metadata.getType() == llvm::ELF::SHT_RELA) {
      for (const auto &relocation : section.relocations()) {
        const auto kind = relocation.getType();
        if (kind != llvm::ELF::R_X86_64_RELATIVE && kind != llvm::ELF::R_X86_64_64)
          continue;
        auto addend = llvm::object::ELFRelocationRef(relocation).getAddend();
        if (!addend) {
          llvm::consumeError(addend.takeError());
          continue;
        }
        std::uint64_t target = *addend;
        if (kind == llvm::ELF::R_X86_64_64) {
          const auto symbol = relocation.getSymbol();
          if (symbol == elf.symbol_end())
            continue;
          auto value = symbol->getAddress();
          if (!value) {
            llvm::consumeError(value.takeError());
            continue;
          }
          target += *value;
        }
        if (target == address)
          return true;
      }
    } else if (metadata.getType() == llvm::ELF::SHT_PROGBITS && !section.isText()) {
      auto bytes = section.getContents();
      if (!bytes)
        llvm::consumeError(bytes.takeError());
      else if (std::string_view(bytes->data(), bytes->size()).find(pointer) != std::string_view::npos)
        return true; // Fixed pointers and in-place addends for packed relative relocations.
    }
  }
  return false;
}

bool Plan::writer_exported(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  if (!image)
    return false;
  const auto address = pc - *image->bias;
  const auto &elf = llvm::cast<llvm::object::ELF64LEObjectFile>(*image->executable.getBinary());
  for (const auto &symbol : elf.getDynamicSymbolIterators()) {
    auto value = symbol.getAddress();
    auto flags = symbol.getFlags();
    const bool exported = value && flags && *value == address &&
        !(*flags & llvm::object::SymbolRef::SF_Undefined) &&
        (*flags & llvm::object::SymbolRef::SF_Exported);
    if (!value)
      llvm::consumeError(value.takeError());
    if (!flags)
      llvm::consumeError(flags.takeError());
    if (exported)
      return true;
  }
  return false;
}

std::size_t Plan::redirect_padding(std::uint64_t function_start, std::uint64_t patch,
                                  std::uint64_t end) const {
  const auto *image = image_for(function_start);
  if (!image || image_for(end) != image || end - patch >= 5)
    return 0;
  const auto minimum = 5 - (end - patch);
  const auto elf_end = end - *image->bias;
  auto available = std::min<std::size_t>(image->code(end).size(), minimum + 14);
  // Include labels and zero-sized symbols, which aren't in Image::symbols.
  const auto constrain = [&](const ObjectFile &object) {
    for (const auto &symbol : object.symbols()) {
      auto address = symbol.getAddress();
      if (!address) {
        llvm::consumeError(address.takeError());
        continue;
      }
      const auto size = llvm::object::ELFSymbolRef(symbol).getSize();
      if (*address >= elf_end)
        available = std::min<std::uint64_t>(available, *address - elf_end);
      else if (size > elf_end - *address)
        available = 0;
    }
  };
  const auto &elf = llvm::cast<ObjectFile>(*image->executable.getBinary());
  constrain(elf);
  if (image->debug.getBinary())
    constrain(llvm::cast<ObjectFile>(*image->debug.getBinary()));
  auto unwind = llvm::DWARFContext::create(elf);
  auto frames = unwind->getEHFrame();
  if (!frames) {
    llvm::consumeError(frames.takeError());
    return 0;
  }
  if (!*frames)
    return 0;
  bool function_end = false;
  for (const auto &entry : (*frames)->entries()) {
    const auto *fde = llvm::dyn_cast<llvm::dwarf::FDE>(&entry);
    if (!fde)
      continue;
    const auto start = fde->getInitialLocation();
    if (start >= elf_end)
      available = std::min<std::uint64_t>(available, start - elf_end);
    else if (fde->getAddressRange() > elf_end - start)
      return 0;
    else if (fde->getAddressRange() == elf_end - start && start + *image->bias == function_start)
      function_end = true;
  }
  if (!function_end || available < minimum)
    return 0;
  const auto padding = native_entry::nop_prefix(end, image->code(end).substr(0, available), minimum);
  if (!padding)
    return 0;
  std::vector<std::uint64_t> addresses;
  for (auto pc = patch + 1; pc < end + padding; ++pc)
    addresses.push_back(pc);
  if (std::ranges::any_of(direct_redirects(addresses), [](const auto &entry) {
        return !entry.second.empty();
      }))
    return 0;
  return padding;
}
#endif

std::pair<std::uint64_t, std::size_t> Plan::containing_function(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  if (!image)
    return {};
  const auto address = pc - *image->bias;
  Range best{};
  for (const auto &[start, size] : image->functions) {
    if (start > address)
      break;
    if (address - start < size && (!best.second || size < best.second))
      best = {start, size};
  }
  return best.second ? std::pair{best.first + *image->bias, std::size_t(best.second)} :
                       std::pair<std::uint64_t, std::size_t>{};
}

const Plan::Image *Plan::image_for(std::uint64_t pc) const {
  if (!image_->code(pc).empty())
    return image_.get();
  return main_ && !main_->code(pc).empty() ? main_.get() : nullptr;
}

bool Plan::is_delete_target(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  return image && image->bias && image->is_delete_target(pc - *image->bias);
}

std::uint64_t Plan::constructor_tail_target(std::uint64_t pc) const {
  for (std::size_t i = 0; i <= delegating_constructor_tails.size(); ++i) {
    const auto next = delegating_constructor_tails.find(pc);
    if (next == delegating_constructor_tails.end())
      return pc;
    pc = next->second;
  }
  return 0;
}

std::vector<std::uint64_t> Plan::exception_roots(std::uint64_t start, std::size_t size) const {
  const auto *image = image_for(start);
  return image ? image->exception_roots(start, size) : std::vector<std::uint64_t>{};
}

std::vector<std::uint64_t> Plan::lifetime_return_callers(std::uint64_t pc, bool constructor) const {
  std::vector<std::uint64_t> entries{pc};
  if (constructor)
    for (const auto &[delegate, _] : delegating_constructor_tails)
      if (constructor_tail_target(delegate) == pc)
        entries.push_back(delegate);

  Redirects branches;
  for (const auto entry : entries)
    branches.try_emplace(entry);
  find_direct_redirects(*image_, branches, DirectEdges::branches);
  if (main_)
    find_direct_redirects(*main_, branches, DirectEdges::branches);
  for (const auto &[target, sources] : branches)
    for (const auto source : sources) {
      const auto [caller, size] = containing_function(source);
      const auto delegate = delegating_constructor_tails.find(caller);
      if (constructor && delegate != delegating_constructor_tails.end() &&
          delegate->second == target && constructor_tail_target(caller) == pc) {
        const auto body = native_entry::scan(caller, code(caller).substr(0, size));
        if (!body.decode_failed && body.diagnostic.empty() && !body.instructions.empty() &&
            body.instructions.back().address == source)
          continue;
      }
      throw std::runtime_error("native lifetime return continuation has an unverified tail caller: " +
                               this->source(source).function);
    }

  std::set<std::uint64_t> callers;
  for (const auto &[entry, calls] : direct_redirects(entries, true))
    callers.insert(calls.begin(), calls.end());
  return {callers.begin(), callers.end()};
}

const std::string &Plan::module_at(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  if (!image)
    throw std::runtime_error("PC is outside the verified ELF images");
  return image->path;
}

std::uint64_t Plan::elf_pc(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  if (!image)
    throw std::runtime_error("PC is outside the verified ELF images");
  return pc - *image->bias;
}

Source Plan::Image::source(std::uint64_t pc) const {
  const llvm::object::SectionedAddress address{pc - *bias,
                                              llvm::object::SectionedAddress::UndefSection};
  const llvm::DILineInfoSpecifier spec{
      llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
      llvm::DILineInfoSpecifier::FunctionNameKind::LinkageName};
  const auto function = [](const auto &line) {
    return line.FunctionName == llvm::DILineInfo::BadString ? "" : llvm::demangle(line.FunctionName);
  };
  const auto file = [](const auto &line) {
    return line.FileName == llvm::DILineInfo::BadString ? std::string{} : line.FileName;
  };
  Source result;
  if (const auto line = dwarf->getLineInfoForAddress(address, spec)) {
    result = {function(*line), file(*line), line->Line, line->Column, {}};
  }
  const auto inlined = dwarf->getInliningInfoForAddress(address, spec);
  if (inlined.getNumberOfFrames() > 1) {
    for (unsigned index = 0; index < inlined.getNumberOfFrames(); ++index) {
      const auto &line = inlined.getFrame(index);
      auto &frame = result.inline_chain.emplace_back();
      frame.set_function(function(line));
      frame.set_file(file(line));
      frame.set_line(line.Line);
      frame.set_column(line.Column);
    }
  }
  if (result.function.empty()) {
    std::set<std::string> candidates;
    for (const auto &symbol : symbols) {
      const auto [start, size] = symbol.range;
      if (symbol.kind == llvm::ELF::STT_FUNC && address.Address >= start &&
          address.Address - start < size)
        candidates.insert(symbol.demangled);
    }
    if (candidates.size() == 1)
      result.function = *candidates.begin();
  }
  return result;
}

Source Plan::source(std::uint64_t pc) const {
  const auto *image = image_for(pc);
  return image ? image->source(pc) : Source{};
}

void Plan::Image::describe(std::uint64_t pc, const Source &source,
                           clangtracer::v1::ObjectObservation &event) const {
  event.set_pc(pc);
  event.set_elf_pc(pc - *bias);
  event.set_module(path);
  event.set_module_build_id(build_id);
  event.set_module_sha256(sha256);
  event.set_file(source.file);
  event.set_function(source.function);
  event.set_line(source.line);
  event.set_column(source.column);
  event.set_source_unavailable(source.file.empty() || !source.line);
  for (const auto &frame : source.inline_chain)
    *event.add_inline_chain() = frame;
}

void Plan::describe_origin(std::uint64_t pc, const Source &source,
                           clangtracer::v1::ObjectObservation &event) const {
  if (const auto *image = image_for(pc))
    image->describe(pc, source, event);
  else
    event.set_source_unavailable(true);
}

void Plan::describe_runtime_origin(std::uint64_t pc, std::uint32_t generation,
    const object_cache::RuntimeModule &module, clangtracer::v1::ObjectObservation &event) {
  event.set_pc(pc);
  event.set_runtime_module_generation(generation);
  event.set_source_unavailable(true);
  if (module.generation.load(std::memory_order_acquire) != generation || pc < module.bias ||
      module.path.back() != '\0')
    return;
  const auto digest = llvm::toHex(module.sha256, true);
  const std::string path(module.path.data());
  const auto [known, inserted] = runtime_images_.try_emplace(generation);
  if (inserted) {
    try {
      auto image = std::make_unique<Image>(0, path, false, false);
      const auto bytes = image->executable.getBinary()->getMemoryBufferRef().getBuffer();
      if (llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
              reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size())) == module.sha256) {
        image->bias = module.bias;
        known->second = std::move(image);
      }
    } catch (const std::exception &) {
      // Retain the verified runtime identity when the original file is gone.
    }
  }
  event.set_module(path);
  event.set_elf_pc(pc - module.bias);
  if (const auto &image = known->second; image && !image->code(pc).empty())
    image->describe(pc, image->source(pc), event);
  event.set_module_sha256(digest);
}

bool Plan::describe_runtime_frame(std::uint64_t pc, std::uint32_t generation,
                                  clangtracer::v1::StackFrame &frame) const {
  const auto known = runtime_images_.find(generation);
  if (known == runtime_images_.end() || !known->second || known->second->code(pc).empty())
    return false;
  const auto &image = *known->second;
  const auto source = image.source(pc);
  frame.set_module(image.path);
  frame.set_module_offset(pc - *image.bias);
  frame.set_file(source.file);
  frame.set_line(source.line);
  frame.set_function(source.function);
  return true;
}

Plan::MethodBoundary Plan::method_boundary(const std::string &module,
    const clangtracer::v1::PreparedInjector &prepared, const clangtracer::v1::DebugInfo *live,
    bool allow_rebind, std::uint32_t pid) {
  using namespace clangtracer::v1;
  const bool primary = pid && std::filesystem::read_symlink(proc::directory(pid) / "exe") == module;
  Image image(0, module, primary, false);
  const auto &object = *image.executable.getBinary();
#if defined(__aarch64__)
  if (!prepared.target_triple().starts_with("aarch64-") ||
      prepared.target_triple().find("linux") == std::string::npos ||
      llvm::cast<ObjectFile>(object).getArch() != llvm::Triple::aarch64)
    throw std::runtime_error("snapshot method requires an AArch64 Linux ELF and receiver ABI");
#endif
  const auto bytes = object.getMemoryBufferRef().getBuffer();
  const auto digest = llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
      reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
  const std::string file_sha256(reinterpret_cast<const char *>(digest.data()), digest.size());
  if (!allow_rebind && file_sha256 != prepared.module_sha256())
    throw std::runtime_error("snapshot adapter requires preparation for this ELF image");
  const auto range = image.function(0, prepared.symbol());
  const auto location = image.dwarf->getDIEsForAddress(range.first);
  verify_receiver_storage(*image.dwarf, location.CompileUnit, prepared);
  if (allow_rebind && file_sha256 != prepared.module_sha256())
    client::verify_rebound_method_signature(location.FunctionDIE, prepared);
  if (live && (live->entry_virtual_address() != range.first || live->entry_size() != range.second ||
               live->runtime_entry() < range.first))
    throw std::runtime_error("prepared snapshot entry disagrees with mapped ELF");
  if (pid) {
    (void)image.bind(pid, range);
    DebugInfo code_identity;
    code_identity.set_module(module);
    code_identity.set_file_size(bytes.size());
    code_identity.set_entry_virtual_address(range.first);
    code_identity.set_entry_size(range.second);
    code_identity.set_runtime_entry(*image.bias + range.first);
    (void)client::verified_function_code(pid, code_identity, range.second, file_sha256);
  } else {
    image.bias = live ? live->runtime_entry() - range.first : 0;
  }
  const auto pc = *image.bias + range.first;
  const auto code = image.code(pc).substr(0, range.second);
#if defined(__aarch64__)
  const auto decoded = native_entry::scan(pc, code);
  if (decoded.decode_failed || !decoded.diagnostic.empty() || decoded.instructions.empty())
    throw std::runtime_error("snapshot method has no verified AArch64 instruction boundaries");
  const auto entry_index = decoded.instructions.front().text.starts_with("bti") ? 1U : 0U;
  if (entry_index >= decoded.instructions.size())
    throw std::runtime_error("snapshot method has no instruction after its AArch64 landing pad");
  const auto &entry = decoded.instructions[entry_index];
  const auto patch_size = entry.size;
  if (entry.address != pc + entry_index * sizeof(std::uint32_t) || patch_size != sizeof(std::uint32_t) ||
      entry.flow != native_entry::Flow::ordinary || !entry.falls_through ||
      !entry.cache_copyable)
    throw std::runtime_error("snapshot method has no relocatable AArch64 entry instruction");
  const auto patch_offset = entry.address - pc;
#else
  const std::span<const std::uint8_t> bytes_at_entry{
      reinterpret_cast<const std::uint8_t *>(code.data()), code.size()};
  native_detour::Patch patch;
  std::string error;
  if (!native_detour::decode(bytes_at_entry, pc, patch, error) &&
      !native_detour::decode(bytes_at_entry, pc, patch, error, native_detour::kRelativeJumpBytes))
    throw std::runtime_error(prepared.selector() + " entry: " + error);
  const auto patch_size = patch.size;
  constexpr unsigned patch_offset = 0;
#endif
  MethodBoundary method;
  method.file_sha256 = file_sha256;
  if (pid)
    method.body = code;
  method.binding.set_role("method");
  method.binding.set_module(module);
  method.binding.set_module_build_id(image.build_id);
  method.binding.set_elf_pc(range.first + patch_offset);
  method.binding.set_runtime_pc(live || pid ? pc + patch_offset : 0);
  method.binding.set_original(std::string(code.substr(patch_offset, patch_size)));
  method.binding.set_patch_size(patch_size);
  method.binding.set_function(prepared.selector());
  auto source = image.source(pc);
  if (source.function.empty())
    source.function = prepared.selector();
  image.describe(pc, source, method.origin);
  method.origin.set_module_sha256(llvm::toHex(digest, true));
  method.origin.set_location_kind(ObjectObservation::CALL_BOUNDARY);
  return method;
}

clangtracer::v1::ObjectWatchPlan Plan::describe(bool live) const {
  clangtracer::v1::ObjectWatchPlan plan;
  plan.set_executable_build_id((main_ ? *main_ : *image_).build_id);
  plan.set_writer_build_id(image_->build_id);
  plan.set_executable_sha256((main_ ? *main_ : *image_).sha256);
  plan.set_binary_sha256(image_->sha256);
  *plan.mutable_source_writers() = source_writers;
  plan.set_field_offset(layout.scalar);
  for (const auto offset : layout.pointer_offsets)
    plan.add_pointee_offsets(offset);
  plan.set_field_encoding(layout.union_tag_offset ? "tagged union" :
      layout.opaque_union ? "opaque union bytes" :
      layout.atomic ? "libstdc++ atomic int32" : "signed int32");
  const auto add = [&](std::uint64_t pc, std::string_view bytes, const char *role) {
    auto &binding = *plan.add_bindings();
    const auto *image = image_for(pc);
    binding.set_role(role);
    binding.set_module(image->path);
    binding.set_module_build_id(image->build_id);
    binding.set_elf_pc(elf_pc(pc));
    if (live)
      binding.set_runtime_pc(pc);
    binding.set_original(std::string(bytes));
    binding.set_function(source(pc).function);
  };
  for (const auto &[start, bytes] : writer_ranges)
    add(start, bytes, "writer");
  if (caller_start)
    add(caller_start, caller_bytes, "borrow caller");
  for (const auto &[pc, site] : sites) {
    if (site.store)
      add(pc, site.bytes, "store");
    if (site.inline_mutation)
      add(pc, site.bytes, pc == site.inline_mutation ? "inline mutation entry" : "inline mutation continuation");
    if (!site.boundary)
      continue;
    constexpr const char *roles[] = {"construct", "destroy", "owner add", "get", "remove", "snapshot",
                                    "writer scope", "container mutation"};
    add(pc, code(pc).substr(0, boundaries.at(pc)),
        site.uncertain_complete ? "construct (complete/base alias)" :
        site.deleting_destructor ? "destroy (deleting)" :
                                  roles[static_cast<unsigned>(*site.boundary)]);
  }
  return plan;
}

void Plan::verify_live(std::uint32_t pid) const {
  const auto check = [pid](std::uint64_t pc, std::string_view original) {
    std::array<char, 4096> current{};
    while (!original.empty()) {
      const auto size = std::min(current.size(), original.size());
      iovec local{current.data(), size};
      iovec remote{reinterpret_cast<void *>(pc), size};
      if (linux_api::process_vm_readv(pid, &local, 1, &remote, 1, 0) !=
              static_cast<ssize_t>(size) ||
          std::string_view(current.data(), size) != original.substr(0, size))
        throw std::runtime_error("live object-watch bytes disagree with the ELF binding");
      pc += size;
      original.remove_prefix(size);
    }
  };
  for (const auto &[start, bytes] : writer_ranges)
    check(start, bytes);
  if (caller_start)
    check(caller_start, caller_bytes);
  for (const auto &[pc, size] : boundaries)
    check(pc, code(pc).substr(0, size));
  for (const auto &[entry, return_pc] : complete_constructor_returns) {
    const auto call = return_pc - object_cache::direct_call_width;
    check(call, code(call).substr(0, object_cache::direct_call_width));
  }
}

std::string compare_prepared(const clangtracer::v1::ObjectWatchPlan &prepared,
                             const clangtracer::v1::ObjectWatchPlan &live, bool no_values) {
  if ((prepared.binary_sha256().empty() && prepared.writer_build_id().empty()) ||
      (prepared.executable_sha256().empty() && prepared.executable_build_id().empty() &&
       prepared.binary_sha256().empty()) ||
      prepared.bindings().empty() || prepared.field_encoding() != live.field_encoding())
    throw std::runtime_error("prepared watch field encoding or identity is incompatible");
  if (prepared.executable_build_id() != live.executable_build_id() ||
      prepared.writer_build_id() != live.writer_build_id() ||
      prepared.binary_sha256() != live.binary_sha256() ||
      prepared.executable_sha256() != live.executable_sha256())
    return "compatible-rebuild";
  if (prepared.pointee_offsets_size() != live.pointee_offsets_size() ||
      !std::equal(prepared.pointee_offsets().begin(), prepared.pointee_offsets().end(),
                  live.pointee_offsets().begin()))
    throw std::runtime_error("prepared pointee path disagrees with the verified live layout");
  const auto keys = [no_values](const auto &bindings, bool include_deleting) {
    std::multiset<std::tuple<std::string, std::string, std::uint64_t, std::string>> result;
    for (const auto &binding : bindings) {
      if ((no_values && binding.role() == "snapshot") ||
          (!include_deleting && binding.role() == "destroy (deleting)"))
        continue;
      auto role = binding.role();
      if (role == "destroy (deleting)")
        role = "destroy";
      result.emplace(std::move(role), binding.module_build_id(), binding.elf_pc(), binding.original());
    }
    return result;
  };
  const auto expected = keys(prepared.bindings(), true);
  const bool legacy = std::ranges::none_of(prepared.bindings(), [](const auto &binding) {
    return binding.role() == "destroy (deleting)";
  });
  // Older packages recorded D0 as a generic destructor or omitted it.
  // The current plan still verifies the live D0 before applying any patch.
  if (prepared.field_offset() != live.field_offset() ||
      (expected != keys(live.bindings(), true) &&
       !(legacy && expected == keys(live.bindings(), false))))
    throw std::runtime_error("same-build bindings disagree with the prepared package");
  return "same-build";
}

std::string field_path(const clangtracer::v1::ObjectWatchRequest &request, const Layout &layout) {
  const auto prefix = request.concrete_type() + "::";
  if (layout.logical_map) {
    std::ostringstream path;
    path << prefix << request.map_field() << '[' << std::quoted(request.map_key()) << "]."
         << request.field();
    return path.str();
  }
  if (layout.logical_vector)
    return prefix + request.vector_field() + "[" + request.element_key_field() + "=" +
           std::to_string(request.element_key()) + "]." + request.field();
  if (!layout.pointer_offsets.empty())
    return prefix + request.pointee_field() + "->" + request.field();
  if (layout.union_tag_offset)
    return prefix + request.field().substr(0, request.field().rfind('.'));
  return prefix + (layout.opaque_union ? "<storage>" : request.field());
}

void set_scalar_values(clangtracer::v1::ObjectObservation &event, const Layout &layout,
                       ScalarSample old, ScalarSample next) {
  if (!layout.opaque_union) {
    if (old.value)
      event.set_old_integer(*old.value);
    if (next.value)
      event.set_new_integer(*next.value);
    return;
  }
  if (!layout.union_tag_offset)
    event.set_diagnostic("untagged union storage; active member unavailable");
  const auto capture = [&](const ScalarSample &sample, bool before) {
    if (!sample.value)
      return;
    const auto alternative = sample.tag ? layout.union_cases.find(*sample.tag) : layout.union_cases.end();
    if (sample.tag) {
      if (before)
        event.set_old_union_tag(*sample.tag);
      else
        event.set_new_union_tag(*sample.tag);
    }
    if (alternative == layout.union_cases.end()) {
      if (layout.union_tag_offset)
        event.set_diagnostic("union discriminator is unreadable or unregistered");
      if (before)
        event.set_old_bytes(&*sample.value, sizeof(*sample.value));
      else
        event.set_new_bytes(&*sample.value, sizeof(*sample.value));
      return;
    }
    const auto &[member, type] = alternative->second;
    auto &value = before ? *event.mutable_snapshot_before() : *event.mutable_snapshot_after();
    if (before)
      event.set_old_union_member(member);
    else
      event.set_new_union_member(member);
    switch (type) {
    case UnionCase::Type::signed_integer:
      value.set_signed_value(*sample.value);
      break;
    case UnionCase::Type::unsigned_integer:
      value.set_unsigned_value(static_cast<std::uint32_t>(*sample.value));
      break;
    case UnionCase::Type::real:
      value.set_real_value(std::bit_cast<float>(*sample.value));
      break;
    }
  };
  capture(old, true);
  capture(next, false);
}
} // namespace tracer::object_watch

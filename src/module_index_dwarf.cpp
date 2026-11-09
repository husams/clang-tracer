#include "module_index_internal.hpp"
#include "module_debug_file.hpp"

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/SHA256.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace tracer::module_index {
namespace {
namespace dw = llvm::dwarf;
using llvm::DWARFDie;

std::optional<std::uint64_t> number(DWARFDie die, dw::Attribute attribute) {
  if (auto value = die.find(attribute)) return value->getAsUnsignedConstant();
  return {};
}

bool flag(DWARFDie die, dw::Attribute attribute) {
  return number(die, attribute).value_or(0) != 0;
}

std::string name(DWARFDie die) {
  if (const auto *text = die.getShortName()) return text;
  return {};
}

bool scope(dw::Tag tag) {
  return tag == dw::DW_TAG_namespace || tag == dw::DW_TAG_class_type ||
         tag == dw::DW_TAG_structure_type || tag == dw::DW_TAG_union_type ||
         tag == dw::DW_TAG_subprogram || tag == dw::DW_TAG_enumeration_type;
}

std::string qualified(DWARFDie die) {
  std::vector<std::string> parts{name(die)};
  for (auto parent = die.getParent(); parent; parent = parent.getParent()) {
    if (!scope(parent.getTag())) continue;
    auto text = name(parent);
    parts.push_back(text.empty() ? "(anonymous)" : std::move(text));
  }
  std::string result;
  if (parts.front().empty()) return result;
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    if (!result.empty()) result += "::";
    result += *it;
  }
  return result;
}

std::string type_id(DWARFDie die) {
  if (!die) return "void";
  // Offset spaces for .debug_info and DWARF 4 .debug_types are separate.
  return std::string(die.getDwarfUnit()->isTypeUnit() &&
                     die.getDwarfUnit()->getVersion() < 5 ? "u" : "t") +
         std::to_string(die.getOffset());
}

std::string referenced_type(DWARFDie die) {
  auto attribute = die.findRecursively({dw::DW_AT_type});
  if (!attribute) return "void";
  auto target = die.getAttributeValueAsReferencedDie(*attribute);
  return target ? type_id(target) : "unresolved";
}

const char *kind(dw::Tag tag) {
  switch (tag) {
  case dw::DW_TAG_base_type: return "scalar";
  case dw::DW_TAG_unspecified_type: return "unspecified";
  case dw::DW_TAG_structure_type: return "struct";
  case dw::DW_TAG_class_type: return "class";
  case dw::DW_TAG_union_type: return "union";
  case dw::DW_TAG_enumeration_type: return "enum";
  case dw::DW_TAG_typedef: return "alias";
  case dw::DW_TAG_array_type: return "array";
  case dw::DW_TAG_pointer_type: return "pointer";
  case dw::DW_TAG_reference_type: return "reference";
  case dw::DW_TAG_rvalue_reference_type: return "rvalue_reference";
  case dw::DW_TAG_const_type: return "const";
  case dw::DW_TAG_volatile_type: return "volatile";
  case dw::DW_TAG_restrict_type: return "restrict";
  case dw::DW_TAG_subroutine_type: return "function";
  case dw::DW_TAG_ptr_to_member_type: return "member_pointer";
  case dw::DW_TAG_atomic_type: return "atomic";
  default: return nullptr;
  }
}

ModuleType describe_type(DWARFDie die, const char *tag) {
  ModuleType result;
  result.id = type_id(die);
  result.name = qualified(die);
  result.kind = tag;
  result.target_id = referenced_type(die);
  result.size = number(die, dw::DW_AT_byte_size);
  result.alignment = number(die, dw::DW_AT_alignment);
  result.encoding = number(die, dw::DW_AT_encoding);
  result.declaration = flag(die, dw::DW_AT_declaration);
  result.namespace_scope = true;
  for (auto parent = die.getParent(); parent; parent = parent.getParent())
    if (parent.getTag() != dw::DW_TAG_namespace &&
        parent.getTag() != dw::DW_TAG_compile_unit &&
        parent.getTag() != dw::DW_TAG_type_unit &&
        parent.getTag() != dw::DW_TAG_partial_unit)
      result.namespace_scope = false;
  result.file = die.getDeclFile(llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath);
  result.line = die.getDeclLine();
  if (result.kind == "array") {
    const auto dimension = die.getFirstChild();
    const auto sibling = dimension ? dimension.getSibling() : DWARFDie{};
    if (dimension && dimension.getTag() == dw::DW_TAG_subrange_type &&
        (!sibling || sibling.getTag() == dw::DW_TAG_null)) {
      const auto lower = number(dimension, dw::DW_AT_lower_bound).value_or(0);
      if (lower == 0) {
        result.count = number(dimension, dw::DW_AT_count);
        if (!result.count) {
          const auto upper = number(dimension, dw::DW_AT_upper_bound);
          if (upper && *upper < UINT64_MAX) result.count = *upper + 1;
        }
      }
    }
  }
  for (auto field : die.children()) {
    const bool base = field.getTag() == dw::DW_TAG_inheritance;
    if (!base && field.getTag() != dw::DW_TAG_member) continue;
    result.fields.push_back({name(field), referenced_type(field),
        number(field, dw::DW_AT_data_member_location),
        number(field, dw::DW_AT_data_bit_offset),
        number(field, dw::DW_AT_bit_size), base});
  }
  return result;
}

std::pair<std::string, std::string> symbol_names(const std::string &linkage) {
  llvm::ItaniumPartialDemangler demangler;
  if (demangler.partialDemangle(linkage.c_str()) || !demangler.isFunction())
    return {llvm::demangle(linkage), llvm::demangle(linkage)};
  using Text = std::unique_ptr<char, decltype(&std::free)>;
  std::size_t size = 0;
  const Text name(demangler.getFunctionName(nullptr, &size), &std::free);
  const Text params(demangler.getFunctionParameters(nullptr, &size), &std::free);
  const Text full(demangler.finishDemangle(nullptr, &size), &std::free);
  if (!name || !params || !full) return {linkage, linkage};
  const auto base = std::string(name.get()) + params.get();
  const std::string_view text(full.get());
  const auto start = text.find(base);
  return {name.get(), start == text.npos ? base : std::string(text.substr(start))};
}

using Key = std::tuple<std::string, std::uint64_t, std::string, std::string>;
using Symbols = std::map<Key, ModuleSymbol>;

Symbols elf_symbols(const Elf &elf, ModuleIdentity &identity,
                    const std::function<void()> &budget) {
  Symbols result;
  auto versions = elf.readDynsymVersions();
  if (!versions) {
    identity.diagnostics.push_back("symbol_versions_unavailable: " +
                                   llvm::toString(versions.takeError()));
  }
  llvm::ArrayRef<llvm::object::ELF64LE::Versym> version_indices;
  auto sections = elf.getELFFile().sections();
  if (!sections) {
    llvm::consumeError(sections.takeError());
  } else {
    for (const auto &section : *sections) {
      if (section.sh_type != llvm::ELF::SHT_GNU_versym) continue;
      auto indices = elf.getELFFile().getSectionContentsAsArray<llvm::object::ELF64LE::Versym>(section);
      if (indices) version_indices = *indices;
      else llvm::consumeError(indices.takeError());
      break;
    }
  }
  const auto add = [&](const llvm::object::ELFSymbolRef &entry, bool dynamic,
                       std::string version, bool default_version) {
    budget();
    auto flags = entry.getFlags();
    auto raw = entry.getName();
    auto address = entry.getAddress();
    if (!flags || !raw || !address) {
      if (!flags) llvm::consumeError(flags.takeError());
      if (!raw) llvm::consumeError(raw.takeError());
      if (!address) llvm::consumeError(address.takeError());
      return;
    }
    if (raw->empty() || (*flags & llvm::object::SymbolRef::SF_Undefined)) return;
    const auto type = entry.getELFType();
    const auto binding = entry.getBinding();
    const auto visibility = entry.getOther() & 3;
    const bool exported = dynamic && binding != llvm::ELF::STB_LOCAL &&
        (visibility == llvm::ELF::STV_DEFAULT || visibility == llvm::ELF::STV_PROTECTED);
    const char *tag;
    switch (type) {
    case llvm::ELF::STT_FUNC: tag = "function"; break;
    case llvm::ELF::STT_OBJECT: tag = "object"; break;
    case llvm::ELF::STT_TLS: tag = "tls"; break;
    case llvm::ELF::STT_GNU_IFUNC: tag = "ifunc"; break;
    case llvm::ELF::STT_NOTYPE:
      if (!exported) return;
      tag = "untyped";
      break;
    default: return;
    }
    auto linkage = raw->str();
    if (const auto at = linkage.find('@'); at != linkage.npos) {
      default_version = linkage[at + 1] == '@';
      if (version.empty()) version = linkage.substr(at + 1 + default_version);
      linkage.resize(at);
    }
    auto &symbol = result[{linkage, *address, tag, version}];
    symbol.linkage = std::move(linkage);
    symbol.version = std::move(version);
    symbol.kind = tag;
    symbol.address = *address;
    symbol.size = std::max(symbol.size, entry.getSize());
    symbol.exported |= exported;
    symbol.default_version |= default_version;
  };
  for (const auto &entry : elf.symbols()) add(entry, false, {}, false);
  std::size_t ordinal = 0;
  for (const auto &entry : elf.getDynamicSymbolIterators()) {
    // readDynsymVersions omits the mandatory null entry, as does the iterator.
    if (versions && ordinal < versions->size()) {
      const auto &version = (*versions)[ordinal];
      const bool is_default = version.IsVerDef && ordinal + 1 < version_indices.size() &&
          !(version_indices[ordinal + 1].vs_index & llvm::ELF::VERSYM_HIDDEN);
      add(entry, true, version.Name, is_default);
    } else {
      add(entry, true, {}, false);
    }
    ++ordinal;
  }
  for (auto it = result.begin(); it != result.end();) {
    const auto &symbol = it->second;
    const auto next = std::next(it);
    if (symbol.version.empty() && !symbol.exported && next != result.end() &&
        next->second.exported && next->second.linkage == symbol.linkage &&
        next->second.address == symbol.address && next->second.kind == symbol.kind) {
      it = result.erase(it);
    } else {
      ++it;
    }
  }
  for (auto &[key, symbol] : result) {
    auto [name, selector] = symbol_names(symbol.linkage);
    symbol.name = std::move(name);
    symbol.selector = std::move(selector);
    symbol.diagnostic = "missing_debug_signature";
  }
  return result;
}

DWARFDie declaration(DWARFDie die) {
  if (auto decl = die.getAttributeValueAsReferencedDie(dw::DW_AT_specification)) return decl;
  if (auto origin = die.getAttributeValueAsReferencedDie(dw::DW_AT_abstract_origin)) return origin;
  return die;
}

void describe_symbol(DWARFDie die, ModuleSymbol &symbol) {
  auto decl = declaration(die);
  symbol.has_debug = true;
  symbol.type_id = referenced_type(die);
  symbol.file = decl.getDeclFile(llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath);
  symbol.line = decl.getDeclLine();
  symbol.diagnostic.clear();
  if (die.getTag() != dw::DW_TAG_subprogram) return;
  if (number(decl, dw::DW_AT_calling_convention).value_or(dw::DW_CC_normal) != dw::DW_CC_normal ||
      flag(decl, dw::DW_AT_virtuality))
    symbol.diagnostic = "unsupported_call_abi";
  auto parameters = die;
  const auto children = die.children();
  if (std::none_of(children.begin(), children.end(), [](DWARFDie child) {
        return child.getTag() == dw::DW_TAG_formal_parameter;
      }))
    parameters = decl;
  for (auto parameter : parameters.children()) {
    if (parameter.getTag() == dw::DW_TAG_unspecified_parameters)
      symbol.diagnostic = "unsupported_variadic_signature";
    if (parameter.getTag() != dw::DW_TAG_formal_parameter) continue;
    const auto description = declaration(parameter);
    if (flag(parameter, dw::DW_AT_artificial) || flag(description, dw::DW_AT_artificial)) {
      if (name(parameter) == "this" ||
          decl.getAttributeValueAsReferencedDie(dw::DW_AT_object_pointer) == description) {
        symbol.member = true;
      } else {
        symbol.diagnostic = "unsupported_artificial_parameter";
      }
      continue;
    }
    symbol.parameters.push_back({name(parameter), referenced_type(parameter)});
  }
  for (auto child : decl.children())
    if (child.getTag() == dw::DW_TAG_unspecified_parameters)
      symbol.diagnostic = "unsupported_variadic_signature";
  if (const auto qualified_name = qualified(decl); !qualified_name.empty() &&
      symbol.selector == symbol.linkage)
    symbol.name = qualified_name;
}

} // namespace

std::string digest(llvm::StringRef bytes) {
  return llvm::toHex(llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
      reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size())), true);
}

std::string inspect_elf(const fs::path &path, const std::function<void(const Elf &)> &inspect,
                        std::string_view expected_digest) {
  auto binary = llvm::object::createBinary(path.string());
  if (!binary) throw std::runtime_error(llvm::toString(binary.takeError()));
  const auto *elf = llvm::dyn_cast<Elf>(binary->getBinary());
  if (!elf || elf->getArch() != llvm::Triple::x86_64 ||
      (elf->getELFFile().getHeader().e_type != llvm::ELF::ET_EXEC &&
       elf->getELFFile().getHeader().e_type != llvm::ELF::ET_DYN))
    throw std::runtime_error("module must be a linked Linux x86_64 ELF image");
  const auto hash = digest(elf->getMemoryBufferRef().getBuffer());
  if (!expected_digest.empty() && hash != expected_digest)
    throw std::invalid_argument("module_identity_mismatch: ELF differs from the module index");
  if (inspect) inspect(*elf);
  return hash;
}

ModuleIdentity scan(const ModuleSource &source, const ModuleIndexOptions &options,
                    const std::function<void(const ModuleType &)> &type_sink,
                    const std::function<void(const ModuleSymbol &)> &symbol_sink) {
  ModuleIdentity identity;
  identity.image = fs::canonical(source.image).string();
  std::uint64_t entries = 0;
  const auto budget = [&] {
    if (options.cancelled && options.cancelled()) throw std::runtime_error("module index cancelled");
    if (options.max_entries && ++entries > options.max_entries)
      throw std::runtime_error("module index entry budget exceeded");
  };
  identity.sha256 = inspect_elf(identity.image, [&](const Elf &elf) {
    identity.build_id = llvm::toHex(llvm::object::getBuildID(&elf), true);
    if (!source.expected_build_id.empty() && source.expected_build_id != identity.build_id)
      throw std::invalid_argument("module_identity_mismatch: mapped ELF changed during indexing");
    auto symbols = elf_symbols(elf, identity, budget);
    const auto companion = debug_file::open(identity.image, elf, source.debug_file);
    if (!source.debug_file.empty() && !companion)
      throw std::runtime_error("debug_file_mismatch: debug file does not match the module");
    const auto &debug = companion ? *llvm::cast<Elf>(companion->binary.getBinary()) : elf;
    if (companion) {
      identity.debug_file = fs::absolute(companion->path).string();
      identity.debug_sha256 = digest(debug.getMemoryBufferRef().getBuffer());
    }
    auto dwarf = llvm::DWARFContext::create(debug, llvm::DWARFContext::ProcessDebugRelocations::Process,
        nullptr, "", [&](llvm::Error error) {
          identity.diagnostics.push_back("dwarf_error: " + llvm::toString(std::move(error)));
        }, [&](llvm::Error error) {
          identity.diagnostics.push_back("dwarf_warning: " + llvm::toString(std::move(error)));
        });
    std::unordered_multimap<std::string, ModuleSymbol *> by_linkage;
    for (auto &[key, symbol] : symbols) by_linkage.emplace(symbol.linkage, &symbol);
    bool has_units = false;
    for (auto &unit : dwarf->normal_units()) {
      has_units = true;
      if (unit->getDWOId())
        identity.diagnostics.push_back("split_dwarf_unresolved: unit " +
                                       std::to_string(unit->getOffset()));
      std::vector<DWARFDie> stack{unit->getUnitDIE(false)};
      while (!stack.empty()) {
        auto die = stack.back();
        if (!die) { stack.pop_back(); continue; }
        stack.back() = die.getSibling();
        budget();
        if (options.max_depth && stack.size() > options.max_depth)
          throw std::runtime_error("module index depth budget exceeded");
        if (const auto *tag = kind(die.getTag())) type_sink(describe_type(die, tag));
        if (die.getTag() == dw::DW_TAG_subprogram || die.getTag() == dw::DW_TAG_variable) {
          const auto *linkage = die.getLinkageName();
          if (linkage && !flag(die, dw::DW_AT_declaration)) {
            const auto range = by_linkage.equal_range(linkage);
            std::uint64_t address = 0, high = 0, section = 0;
            const bool has_address = die.getLowAndHighPC(address, high, section);
            for (auto it = range.first; it != range.second; ++it) {
              auto &symbol = *it->second;
              if (symbol.kind == "function" && has_address && address == symbol.address &&
                  die.getTag() == dw::DW_TAG_subprogram) {
                if (!symbol.has_debug) describe_symbol(die, symbol);
                else if (symbol.type_id != referenced_type(die))
                  symbol.diagnostic = "conflicting_debug_signature";
              } else if (!symbol.has_debug && symbol.kind == "object" &&
                         die.getTag() == dw::DW_TAG_variable &&
                         flag(die, dw::DW_AT_external)) {
                describe_symbol(die, symbol);
              }
            }
          }
        }
        if (auto child = die.getFirstChild()) stack.push_back(child);
      }
    }
    if (!has_units) identity.diagnostics.push_back("no_dwarf");
    std::size_t id = 0;
    for (auto &[key, symbol] : symbols) {
      symbol.id = "s" + std::to_string(++id);
      symbol_sink(symbol);
    }
  });
  if (identity.debug_file.empty()) identity.debug_sha256 = identity.sha256;
  return identity;
}

} // namespace tracer::module_index

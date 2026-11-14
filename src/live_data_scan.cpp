#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>

#include <llvm/ADT/StringExtras.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>

#include <dlfcn.h>
#include <link.h>

#include "debug_resolver.hpp"
#include "linux_syscalls.hpp"
#include "live_data_internal.hpp"
#include "proc_identity.hpp"

namespace tracer::live::data {
namespace {
namespace fs = std::filesystem;
namespace dw = llvm::dwarf;
using Elf = llvm::object::ELF64LEObjectFile;
using Binary = llvm::object::OwningBinary<llvm::object::Binary>;
using Address = std::uintptr_t;

struct Mapping {
  Address begin, end;
  std::string permissions, name;

  bool contains(Address address, std::size_t size) const {
    return address >= begin && address < end && size <= end - address;
  }
};

std::vector<Mapping> mappings() {
  std::ifstream input("/proc/self/maps");
  if (!input) {
    throw std::runtime_error("cannot read the target mappings");
  }
  std::vector<Mapping> result;
  for (std::string line; std::getline(input, line);) {
    Mapping map{};
    char perms[5]{};
    int name_offset = 0;
    if (std::sscanf(line.c_str(), "%lx-%lx %4s %*s %*s %*s %n", &map.begin, &map.end, perms,
                    &name_offset) != 3 ||
        !name_offset || map.end <= map.begin || std::strlen(perms) != 4) {
      throw std::runtime_error("invalid target mapping entry");
    }
    map.permissions = perms;
    map.name = line.substr(name_offset);
    result.push_back(std::move(map));
  }
  return result;
}

bool read(Address address, void* data, std::size_t size) {
  iovec local{data, size};
  iovec remote{reinterpret_cast<void*>(address), size};
  return linux_api::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0) ==
         static_cast<ssize_t>(size);
}

template <class T>
bool read(Address address, T& value) {
  return read(address, &value, sizeof(value));
}

Binary open(const std::string& path) {
  auto file = llvm::object::createBinary(path);
  if (!file) {
    throw std::runtime_error("cannot open " + path + ": " + llvm::toString(file.takeError()));
  }
  if (!llvm::isa<Elf>(file->getBinary())) {
    throw std::runtime_error("live data requires a Linux x86-64 ELF");
  }
  return std::move(*file);
}

Address checked_add(Address left, Address right) {
  if (right > UINTPTR_MAX - left) {
    throw std::runtime_error("invalid live ELF address");
  }
  return left + right;
}

std::optional<std::uint64_t> number(llvm::DWARFDie die, dw::Attribute attr) {
  if (const auto value = die.find(attr)) {
    return value->getAsUnsignedConstant();
  }
  return std::nullopt;
}

struct Layout {
  std::size_t size, alignment;
  bool operator==(const Layout&) const = default;
};

using Layouts = std::map<std::string, std::optional<Layout>, std::less<>>;

bool record(dw::Tag tag) {
  return tag == dw::DW_TAG_class_type || tag == dw::DW_TAG_structure_type;
}

void read_layouts(llvm::DWARFDie current, const std::string& scope, Layouts& layouts) {
  for (const auto child : current.children()) {
    const auto tag = child.getTag();
    const auto* name = child.getShortName();
    if (!name || (!record(tag) && tag != dw::DW_TAG_namespace)) {
      continue;
    }
    const auto qualified = scope + name;
    if (record(tag)) {
      const auto size = number(child, dw::DW_AT_byte_size);
      if (size && *size) {
        const Layout layout{*size, number(child, dw::DW_AT_alignment).value_or(0)};
        auto [entry, inserted] = layouts.try_emplace(qualified, layout);
        if (!inserted && entry->second != layout) {
          entry->second.reset();
        }
      }
    }
    if (qualified != "std" && qualified != "__gnu_cxx") {
      read_layouts(child, qualified + "::", layouts);
    }
  }
}

struct TypeEvidence {
  std::string name;
  Layout layout;
  Address vtable_begin, vtable_end, rtti;
};

using Symbols = std::map<std::string, std::optional<std::pair<Address, std::size_t>>>;

void collect_symbols(const Elf& elf, Symbols& wanted) {
  const auto collect = [&](auto symbols) {
    for (const auto symbol : symbols) {
      const llvm::object::ELFSymbolRef entry(symbol);
      if (entry.getELFType() != STT_OBJECT) {
        continue;
      }
      auto name = entry.getName();
      if (!name) {
        llvm::consumeError(name.takeError());
        continue;
      }
      if (!name->starts_with("_ZTV") && !name->starts_with("_ZTI")) {
        continue;
      }
      const auto found = wanted.find(llvm::demangle(*name));
      if (found == wanted.end() || !entry.getSize()) {
        continue;
      }
      auto value = entry.getValue();
      if (!value) {
        throw std::runtime_error(llvm::toString(value.takeError()));
      }
      if (!*value) {
        continue;
      }
      const auto definition = std::make_pair(*value, entry.getSize());
      if (found->second && *found->second != definition) {
        throw std::runtime_error("ambiguous " + found->first);
      }
      found->second = definition;
    }
  };
  collect(elf.symbols());
  collect(elf.getDynamicSymbolIterators());
}

bool readable(const std::vector<Mapping>& maps, Address address, std::size_t size) {
  return std::ranges::any_of(maps, [&](const auto& map) {
    return map.permissions[0] == 'r' && map.contains(address, size);
  });
}

std::map<Address, TypeEvidence> address_points(const std::vector<TypeEvidence>& types,
                                               const std::vector<Mapping>& maps) {
  std::map<Address, TypeEvidence> result;
  for (const auto& type : types) {
    if (!readable(maps, type.rtti, 2 * sizeof(Address)) ||
        !readable(maps, type.vtable_begin, type.vtable_end - type.vtable_begin)) {
      continue;
    }
    for (auto point = type.vtable_begin + 2 * sizeof(Address);
         point < type.vtable_end && type.vtable_end - point >= sizeof(Address);
         point += sizeof(Address)) {
      std::array<Address, 3> words{};
      if (!read(point - 2 * sizeof(Address), words) || words[0] || words[1] != type.rtti) {
        continue;
      }
      if (std::ranges::any_of(maps, [&](const auto& map) {
            return map.permissions[2] == 'x' && map.contains(words[2], 1);
          })) {
        result.emplace(point, type);
      }
    }
  }
  return result;
}
} // namespace

struct TargetImage::Image {
  LiveDataTarget target;
  Binary binary;
  const Elf& elf;
  std::unique_ptr<void, decltype(&::dlclose)> loader{nullptr, &::dlclose};
  Address bias = 0, text = 0;
  mutable std::optional<Layouts> layouts;

  explicit Image(const LiveDataTarget& selection)
      : target(selection)
      , binary(open(target.module))
      , elf(*llvm::cast<Elf>(binary.getBinary())) {
    if (!fs::path(target.module).is_absolute() || !target.process_start ||
        target.build_id.empty() ||
        llvm::toHex(llvm::object::getBuildID(&elf), true) != target.build_id) {
      throw std::runtime_error("live data requires the exact process and mapped ELF build ID");
    }
    auto headers = elf.getELFFile().program_headers();
    if (!headers) {
      throw std::runtime_error(llvm::toString(headers.takeError()));
    }
    for (const auto& segment : *headers) {
      if (segment.p_type == PT_LOAD && (segment.p_flags & PF_X) &&
          segment.p_filesz && !text) {
        text = segment.p_vaddr;
      }
    }
    if (!text) {
      throw std::runtime_error("live data ELF has no executable anchor");
    }
    bias = client::bind_live_executable(::getpid(), target.module, elf, text, 1);
    if (fs::equivalent(target.module, "/proc/self/exe")) {
      loader.reset(::dlopen(nullptr, RTLD_NOW));
    } else {
      loader.reset(::dlopen(target.module.c_str(), RTLD_NOW | RTLD_NOLOAD));
    }
    link_map* linkage = nullptr;
    if (!loader || ::dlinfo(loader.get(), RTLD_DI_LINKMAP, &linkage) != 0 || !linkage ||
        linkage->l_addr != bias) {
      throw std::runtime_error("cannot retain the verified live ELF mapping");
    }
    verify();
  }

  void verify() const {
    if (target.process_start != proc::start_time("/proc/self/stat") ||
        client::bind_live_executable(::getpid(), target.module, elf, text, 1) != bias) {
      throw std::runtime_error("live data target changed");
    }
  }

  const Layouts& records() const {
    if (!layouts) {
      std::optional<Binary> separate;
      const auto* debug = &elf;
      if (const auto path = client::find_matching_debug_file(target.module, elf)) {
        separate.emplace(open(*path));
        debug = llvm::cast<Elf>(separate->getBinary());
      }
      const auto dwarf = llvm::DWARFContext::create(*debug);
      layouts.emplace();
      for (const auto& unit : dwarf->compile_units()) {
        read_layouts(unit->getUnitDIE(false), "", *layouts);
      }
    }
    return *layouts;
  }
};

TargetImage::TargetImage(const LiveDataTarget& target)
    : image_(std::make_unique<Image>(target)) {}

TargetImage::~TargetImage() = default;

void TargetImage::verify() const {
  image_->verify();
}

bool TargetImage::module_matches(std::string_view selector) const {
  return selector.empty() || selector == image_->target.module ||
         selector == fs::path(image_->target.module).filename();
}

bool TargetImage::verified_layout(const ObjectView& object) const {
  const auto& records = image_->records();
  const auto record = records.find(object.type);
  return record != records.end() && record->second && record->second->size == object.size &&
         (!record->second->alignment || record->second->alignment == object.alignment) &&
         reinterpret_cast<Address>(object.object) % object.alignment == 0;
}

Scan TargetImage::scan(const std::vector<std::string>& types) const {
  const auto& records = image_->records();
  Scan report;
  report.limitations.push_back(
      "Scan covers readable [heap] ranges only. Arena, mmap and custom allocations may be absent.");
  report.limitations.push_back("Matching bytes can occur in freed, reused or fabricated storage. "
                               "Scan evidence alone is read-only.");
  Symbols symbols;
  const std::set<std::string> requested(types.begin(), types.end());
  for (const auto& type : requested) {
    symbols.try_emplace("vtable for " + type);
    symbols.try_emplace("typeinfo for " + type);
  }
  collect_symbols(image_->elf, symbols);
  std::vector<TypeEvidence> inputs;
  for (const auto& name : requested) {
    const auto record = records.find(name);
    if (record == records.end() || !record->second) {
      report.limitations.push_back(name + ": missing or ambiguous build-matched DWARF layout");
      continue;
    }
    const auto& table = symbols.at("vtable for " + name);
    const auto& rtti = symbols.at("typeinfo for " + name);
    if (!table || !rtti) {
      report.limitations.push_back(name + ": no exact ELF vtable and RTTI");
      continue;
    }
    auto layout = *record->second;
    if (!layout.alignment) {
      layout.alignment = alignof(Address);
    }
    inputs.push_back({name, layout, checked_add(image_->bias, table->first),
                      checked_add(checked_add(image_->bias, table->first), table->second),
                      checked_add(image_->bias, rtti->first)});
  }
  const auto maps = mappings();
  const auto points = address_points(inputs, maps);
  if (points.empty()) {
    return report;
  }

  std::array<Address, 4096> words;
  for (const auto& map : maps) {
    if (map.name != "[heap]" || map.permissions[0] != 'r') {
      continue;
    }
    for (auto base = map.begin; base < map.end;) {
      const auto size = std::min(sizeof(words), map.end - base);
      if (read(base, words.data(), size)) {
        for (std::size_t i = 0; i < size / sizeof(Address); ++i) {
          const auto match = points.find(words[i]);
          if (match == points.end()) {
            continue;
          }
          const auto address = base + i * sizeof(Address);
          const auto& type = match->second;
          if (address % type.layout.alignment || !map.contains(address, type.layout.size)) {
            continue;
          }
          report.candidates.push_back(
              {{"", type.name, image_->target.module, image_->target.build_id, "",
                "matching DWARF size, ELF primary vtable and RTTI; ownership unknown", address},
               type.layout.size,
               match->first});
        }
      } else {
        if (report.limitations.back() != "Some heap ranges became unreadable during the scan.") {
          report.limitations.push_back("Some heap ranges became unreadable during the scan.");
        }
      }
      base += size;
    }
  }
  verify();
  return report;
}

bool TargetImage::matches(const Candidate& candidate) const {
  const auto& record = candidate.report;
  if (record.build_id != image_->target.build_id || record.module != image_->target.module) {
    return false;
  }
  const auto maps = mappings();
  if (std::ranges::none_of(maps, [&](const auto& map) {
        return map.name == "[heap]" && map.permissions[0] == 'r' &&
               map.contains(record.address, candidate.size);
      })) {
    return false;
  }
  Address current = 0;
  return read(record.address, current) && current == candidate.vptr;
}
} // namespace tracer::live::data

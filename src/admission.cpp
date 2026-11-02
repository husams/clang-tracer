#include "admission.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <elf.h>
#include <fcntl.h>
#include <llvm/DebugInfo/DWARF/DWARFDataExtractor.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/DebugInfo/DWARF/LowLevel/DWARFUnwindTable.h>
#include <llvm/Support/Error.h>
#include <llvm/TargetParser/Triple.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "injector.hpp"
#include "mapped_path.hpp"

namespace tracer::injector {

namespace detail {
struct ApprovedModuleIdentity {
  std::string path;
  std::uint64_t load_bias = 0;
};
} // namespace detail

ApplicationModule::ApplicationModule(
    std::shared_ptr<const detail::ApprovedModuleIdentity> identity)
    : identity_(std::move(identity)) {}

const std::string &ApplicationModule::path() const noexcept {
  static const std::string empty;
  return identity_ ? identity_->path : empty;
}

namespace {

[[noreturn]] void unavailable(std::string_view why) {
  throw IdlePointUnavailable(std::string(why));
}

template <class T>
bool range_ok(std::size_t size, std::uint64_t offset, std::uint64_t count) {
  return offset <= size && count <= (size - offset) / sizeof(T);
}

struct Mapping {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint64_t offset = 0;
  bool readable = false;
  bool executable = false;
  std::string path;
  std::string canonical_path;
};

std::vector<Mapping> read_mappings(pid_t pid) {
  std::ifstream input("/proc/" + std::to_string(pid) + "/maps");
  if (!input) {
    unavailable("cannot read target process mappings");
  }
  std::vector<Mapping> out;
  std::string line;
  std::unordered_map<std::string, std::string> canonical_paths;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string addresses, permissions, device, inode;
    Mapping m;
    if (!(row >> addresses >> permissions >> std::hex >> m.offset >> device >>
          std::dec >> inode)) {
      continue;
    }
    m.readable = permissions.size() >= 1 && permissions[0] == 'r';
    m.executable = permissions.size() >= 3 && permissions[2] == 'x';
    const auto dash = addresses.find('-');
    if (dash == std::string::npos) {
      continue;
    }
    try {
      m.begin = std::stoull(addresses.substr(0, dash), nullptr, 16);
      m.end = std::stoull(addresses.substr(dash + 1), nullptr, 16);
    } catch (...) {
      continue;
    }
    std::getline(row, m.path);
    const auto first = m.path.find_first_not_of(' ');
    if (first != std::string::npos) {
      m.path.erase(0, first);
    }
    if (!m.path.empty() && m.path.front() == '/') {
      const auto found = canonical_paths.find(m.path);
      if (found != canonical_paths.end()) {
        m.canonical_path = found->second;
      } else {
        m.canonical_path = ::tracer::detail::canonical_mapped_path(m.path);
        canonical_paths.emplace(m.path, m.canonical_path);
      }
    }
    out.push_back(std::move(m));
  }
  return out;
}

struct FileImage {
  std::string path;
  std::uint16_t elf_type = 0;
  std::string soname;
  std::string interpreter;
  std::vector<std::uint8_t> bytes;
  std::uint64_t bias = 0;
  std::vector<Elf64_Phdr> loads;

  struct Function {
    std::string name;
    std::uint64_t start;
    std::uint64_t size;
  };

  std::vector<Function> functions;
  std::uint64_t eh_frame_addr = 0;
  std::string eh_frame;

  std::uint64_t runtime(const Function &f) const {
    if (f.start > std::numeric_limits<std::uint64_t>::max() - bias) {
      unavailable("ELF function address overflows the target address space");
    }
    return bias + f.start;
  }

  const Function *named(std::string_view name) const {
    const Function *found = nullptr;
    for (const auto &f : functions) {
      if (f.name != name || f.size == 0) {
        continue;
      }
      if (found && (found->start != f.start || found->size != f.size)) {
        unavailable("ELF has ambiguous function symbols");
      }
      found = &f;
    }
    return found;
  }

  bool executable_pc(std::uint64_t pc, const std::vector<Mapping> &maps) const {
    if (pc < bias) {
      return false;
    }
    const auto link_pc = pc - bias;
    for (const auto &p : loads) {
      if (p.p_type != PT_LOAD || (p.p_flags & PF_X) == 0 ||
          p.p_memsz > std::numeric_limits<std::uint64_t>::max() - p.p_vaddr ||
          p.p_filesz > std::numeric_limits<std::uint64_t>::max() - p.p_vaddr ||
          link_pc < p.p_vaddr || link_pc - p.p_vaddr >= p.p_filesz) {
        continue;
      }
      const auto file_offset = p.p_offset + (link_pc - p.p_vaddr);
      if (file_offset < p.p_offset) {
        continue;
      }
      const bool mapped =
          std::any_of(maps.begin(), maps.end(), [&](const Mapping &m) {
            if (!m.executable || m.canonical_path != path ||
                pc < m.begin || pc >= m.end) {
              return false;
            }
            const auto delta = pc - m.begin;
            return delta <=
                       std::numeric_limits<std::uint64_t>::max() - m.offset &&
                   m.offset + delta == file_offset;
          });
      if (mapped) {
        return true;
      }
    }
    return false;
  }
};

FileImage load_image_fd(const std::string &path, int fd,
                        const std::vector<Mapping> &maps) {
  FileImage image;
  image.path = path;
  struct stat st{};
  if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    unavailable("mapped ELF path is not a readable regular file");
  }
  if (st.st_size < static_cast<off_t>(sizeof(Elf64_Ehdr)) ||
      static_cast<std::uint64_t>(st.st_size) > 512ULL * 1024 * 1024) {
    unavailable("mapped ELF has an unsupported size");
  }
  image.bytes.resize(static_cast<std::size_t>(st.st_size));
  std::size_t got = 0;
  while (got < image.bytes.size()) {
    const auto n = ::pread(fd, image.bytes.data() + got,
                           image.bytes.size() - got, static_cast<off_t>(got));
    if (n <= 0) {
      unavailable("could not read complete mapped ELF image");
    }
    got += static_cast<std::size_t>(n);
  }
  const auto *eh = reinterpret_cast<const Elf64_Ehdr *>(image.bytes.data());
  if (std::memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 ||
      eh->e_ident[EI_CLASS] != ELFCLASS64 ||
      eh->e_ident[EI_DATA] != ELFDATA2LSB || eh->e_machine != EM_X86_64 ||
      eh->e_phentsize != sizeof(Elf64_Phdr) ||
      eh->e_shentsize != sizeof(Elf64_Shdr) ||
      (eh->e_type != ET_DYN && eh->e_type != ET_EXEC) ||
      !range_ok<Elf64_Phdr>(image.bytes.size(), eh->e_phoff, eh->e_phnum) ||
      !range_ok<Elf64_Shdr>(image.bytes.size(), eh->e_shoff, eh->e_shnum)) {
    unavailable("mapped image is not a supported x86_64 ELF");
  }
  image.elf_type = eh->e_type;
  const auto *ph =
      reinterpret_cast<const Elf64_Phdr *>(image.bytes.data() + eh->e_phoff);
  for (unsigned i = 0; i < eh->e_phnum; ++i) {
    if (ph[i].p_type == PT_INTERP) {
      if (ph[i].p_offset > image.bytes.size() || ph[i].p_filesz == 0 ||
          ph[i].p_filesz > 4096 ||
          ph[i].p_filesz > image.bytes.size() - ph[i].p_offset) {
        unavailable("ELF interpreter path is malformed");
      }
      const char *interp =
          reinterpret_cast<const char *>(image.bytes.data() + ph[i].p_offset);
      const auto *terminator =
          static_cast<const char *>(std::memchr(interp, 0, ph[i].p_filesz));
      if (!terminator || terminator == interp || interp[0] != '/') {
        unavailable("ELF interpreter path is malformed");
      }
      image.interpreter.assign(interp, terminator);
    }
    if (ph[i].p_type == PT_LOAD) {
      if (ph[i].p_filesz > ph[i].p_memsz ||
          ph[i].p_offset > image.bytes.size() ||
          ph[i].p_filesz > image.bytes.size() - ph[i].p_offset ||
          ph[i].p_memsz >
              std::numeric_limits<std::uint64_t>::max() - ph[i].p_vaddr) {
        unavailable("ELF load segment is malformed");
      }
      image.loads.push_back(ph[i]);
    }
  }
  if (image.loads.empty()) {
    unavailable("ELF has no loadable segments");
  }

  const auto page_size = ::sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    unavailable("cannot determine target page size");
  }
  const auto page = static_cast<std::uint64_t>(page_size);
  const auto same_file = [&](const Mapping &m) {
    return !m.canonical_path.empty() && m.canonical_path == path;
  };
  const auto file_page_range = [&](const Elf64_Phdr &segment,
                                   std::uint64_t &file_begin,
                                   std::uint64_t &virtual_begin,
                                   std::uint64_t &length) {
    if (segment.p_filesz == 0 || segment.p_offset % page !=
                                    segment.p_vaddr % page) {
      return false;
    }
    const auto file_end = segment.p_offset + segment.p_filesz;
    const auto virtual_end = segment.p_vaddr + segment.p_filesz;
    if (file_end > std::numeric_limits<std::uint64_t>::max() - (page - 1) ||
        virtual_end > std::numeric_limits<std::uint64_t>::max() - (page - 1)) {
      return false;
    }
    file_begin = (segment.p_offset / page) * page;
    virtual_begin = (segment.p_vaddr / page) * page;
    const auto file_page_end = ((file_end + page - 1) / page) * page;
    const auto virtual_page_end = ((virtual_end + page - 1) / page) * page;
    if (file_page_end < file_begin || virtual_page_end < virtual_begin ||
        file_page_end - file_begin != virtual_page_end - virtual_begin) {
      return false;
    }
    length = file_page_end - file_begin;
    return length != 0;
  };
  const auto mapping_covers_range = [&](std::uint64_t load_bias,
                                       const Elf64_Phdr &segment) {
    std::uint64_t file_begin = 0, virtual_begin = 0, length = 0;
    if (!file_page_range(segment, file_begin, virtual_begin, length) ||
        virtual_begin >
            std::numeric_limits<std::uint64_t>::max() - load_bias) {
      return false;
    }
    const auto address_begin = load_bias + virtual_begin;
    if (length > std::numeric_limits<std::uint64_t>::max() - address_begin) {
      return false;
    }
    const auto address_end = address_begin + length;
    auto cursor = address_begin;
    while (cursor < address_end) {
      auto covered_until = cursor;
      for (const auto &mapping : maps) {
        if (!same_file(mapping) ||
            ((segment.p_flags & PF_X) != 0 && !mapping.executable) ||
            cursor < mapping.begin || cursor >= mapping.end) {
          continue;
        }
        const auto address_delta = cursor - address_begin;
        const auto mapping_delta = cursor - mapping.begin;
        if (address_delta >
                std::numeric_limits<std::uint64_t>::max() - file_begin ||
            mapping_delta >
                std::numeric_limits<std::uint64_t>::max() - mapping.offset ||
            file_begin + address_delta != mapping.offset + mapping_delta) {
          continue;
        }
        covered_until = std::max(covered_until,
                                 std::min(address_end, mapping.end));
      }
      if (covered_until == cursor) {
        return false;
      }
      cursor = covered_until;
    }
    return true;
  };
  bool bias_found = false;
  bool header_identity_found = false;
  for (const auto &p : image.loads) {
    if (p.p_offset != 0 || p.p_filesz == 0) {
      continue;
    }
    std::uint64_t expected_offset = 0, expected_vaddr = 0, range_length = 0;
    if (!file_page_range(p, expected_offset, expected_vaddr, range_length)) {
      unavailable("ELF header load segment has invalid page alignment for " +
                  path);
    }
    for (const auto &m : maps) {
      if (!same_file(m) || m.offset > expected_offset) {
        continue;
      }
      const auto file_delta = expected_offset - m.offset;
      if (file_delta >= m.end - m.begin ||
          file_delta > std::numeric_limits<std::uint64_t>::max() - m.begin) {
        continue;
      }
      const auto header_address = m.begin + file_delta;
      if (header_address < expected_vaddr) {
        continue;
      }
      header_identity_found = true;
      const auto candidate = header_address - expected_vaddr;
      // A VMA may cover several adjacent PT_LOAD ranges after the kernel
      // coalesces mappings with matching protections and file offsets. Check
      // byte-page coverage and file-offset correspondence across the range.
      const bool complete =
          std::all_of(image.loads.begin(), image.loads.end(), [&](const auto &load) {
            return load.p_filesz == 0 || mapping_covers_range(candidate, load);
          });
      if (!complete) {
        continue;
      }
      if (bias_found && candidate != image.bias) {
        unavailable("ELF header mappings imply inconsistent load biases");
      }
      image.bias = candidate;
      bias_found = true;
    }
  }
  if (!bias_found) {
    if (!header_identity_found) {
      unavailable("no target ELF header mapping matches canonical path " +
                  path);
    }
    unavailable("target ELF header identity found but PT_LOAD file coverage "
                "is incomplete for " +
                path);
  }
  if (eh->e_type == ET_EXEC && image.bias != 0) {
    unavailable("unexpected load bias for fixed-address ELF");
  }

  const auto *sh =
      reinterpret_cast<const Elf64_Shdr *>(image.bytes.data() + eh->e_shoff);
  if (eh->e_shstrndx >= eh->e_shnum) {
    unavailable("ELF section names are unavailable");
  }
  const auto &names = sh[eh->e_shstrndx];
  if (names.sh_offset > image.bytes.size() ||
      names.sh_size > image.bytes.size() - names.sh_offset) {
    unavailable("ELF section-name table is malformed");
  }
  const char *strtab =
      reinterpret_cast<const char *>(image.bytes.data() + names.sh_offset);
  for (unsigned i = 0; i < eh->e_shnum; ++i) {
    if (sh[i].sh_name < names.sh_size) {
      const char *name = strtab + sh[i].sh_name;
      const auto rem = names.sh_size - sh[i].sh_name;
      if (std::memchr(name, 0, rem) && std::strcmp(name, ".eh_frame") == 0 &&
          sh[i].sh_type != SHT_NOBITS) {
        if (sh[i].sh_offset > image.bytes.size() ||
            sh[i].sh_size > image.bytes.size() - sh[i].sh_offset) {
          unavailable("ELF .eh_frame section is malformed");
        }
        image.eh_frame_addr = sh[i].sh_addr;
        image.eh_frame.assign(reinterpret_cast<const char *>(
                                  image.bytes.data() + sh[i].sh_offset),
                              sh[i].sh_size);
      }
    }
    if (sh[i].sh_type != SHT_DYNAMIC || sh[i].sh_entsize != sizeof(Elf64_Dyn) ||
        sh[i].sh_link >= eh->e_shnum || sh[i].sh_offset > image.bytes.size() ||
        sh[i].sh_size > image.bytes.size() - sh[i].sh_offset) {
      continue;
    }
    const auto &dynamic_strings = sh[sh[i].sh_link];
    if (dynamic_strings.sh_offset > image.bytes.size() ||
        dynamic_strings.sh_size >
            image.bytes.size() - dynamic_strings.sh_offset) {
      continue;
    }
    const char *dynstr = reinterpret_cast<const char *>(
        image.bytes.data() + dynamic_strings.sh_offset);
    const auto *entries = reinterpret_cast<const Elf64_Dyn *>(
        image.bytes.data() + sh[i].sh_offset);
    const auto count = sh[i].sh_size / sizeof(Elf64_Dyn);
    for (std::uint64_t j = 0; j < count; ++j) {
      if (entries[j].d_tag != DT_SONAME ||
          entries[j].d_un.d_val >= dynamic_strings.sh_size) {
        continue;
      }
      const char *value = dynstr + entries[j].d_un.d_val;
      const auto available = dynamic_strings.sh_size - entries[j].d_un.d_val;
      const auto *end =
          static_cast<const char *>(std::memchr(value, 0, available));
      if (end) {
        image.soname.assign(value, end);
      }
    }
  }
  for (unsigned i = 0; i < eh->e_shnum; ++i) {
    if (sh[i].sh_type != SHT_SYMTAB && sh[i].sh_type != SHT_DYNSYM) {
      continue;
    }
    if (sh[i].sh_link >= eh->e_shnum || sh[i].sh_entsize != sizeof(Elf64_Sym) ||
        sh[i].sh_offset > image.bytes.size() ||
        sh[i].sh_size > image.bytes.size() - sh[i].sh_offset) {
      continue;
    }
    const auto &strings = sh[sh[i].sh_link];
    if (strings.sh_offset > image.bytes.size() ||
        strings.sh_size > image.bytes.size() - strings.sh_offset) {
      continue;
    }
    const char *string_data =
        reinterpret_cast<const char *>(image.bytes.data() + strings.sh_offset);
    const auto *symbols = reinterpret_cast<const Elf64_Sym *>(
        image.bytes.data() + sh[i].sh_offset);
    const auto count = sh[i].sh_size / sizeof(Elf64_Sym);
    for (std::uint64_t j = 0; j < count; ++j) {
      const auto &sym = symbols[j];
      if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC ||
          sym.st_name >= strings.sh_size || !sym.st_size) {
        continue;
      }
      const char *name = string_data + sym.st_name;
      if (!std::memchr(name, 0, strings.sh_size - sym.st_name)) {
        continue;
      }
      image.functions.push_back({name, sym.st_value, sym.st_size});
    }
  }
  if (image.eh_frame.empty()) {
    unavailable("mapped ELF has no usable .eh_frame section");
  }
  image.bytes.clear();
  image.bytes.shrink_to_fit();
  return image;
}

FileImage load_image(const std::string &path, const std::vector<Mapping> &maps) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    unavailable("cannot open mapped ELF image");
  }
  FileImage image;
  try {
    image = load_image_fd(path, fd, maps);
  } catch (...) {
    ::close(fd);
    throw;
  }
  ::close(fd);
  return image;
}

FileImage load_executable(pid_t pid, const std::vector<Mapping> &maps) {
  const std::string proc_path = "/proc/" + std::to_string(pid) + "/exe";
  const auto path = ::tracer::detail::canonical_mapped_path(proc_path);
  if (path.empty()) {
    unavailable("cannot identify the target executable");
  }
  return load_image(path, maps);
}

FileImage load_libc(const std::vector<Mapping> &maps) {
  for (const auto &m : maps) {
    const auto basename = m.path.substr(m.path.find_last_of('/') + 1);
    const bool candidate =
        basename == "libc.so.6" || (basename.starts_with("libc-") &&
                                    basename.ends_with(".so"));
    if (m.canonical_path.empty() || !candidate) {
      continue;
    }
    const auto &path = m.canonical_path;
    auto image = load_image(path, maps);
    if (image.soname == "libc.so.6") {
      return image;
    }
  }
  unavailable("cannot identify the mapped libc ELF image");
}

using ExecutableMapRecord =
    std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::string>;

std::vector<ExecutableMapRecord>
executable_map_snapshot(const std::vector<Mapping> &maps) {
  std::vector<ExecutableMapRecord> result;
  for (const auto &m : maps) {
    if (m.executable && !m.canonical_path.empty()) {
      result.emplace_back(m.begin, m.end, m.offset, m.canonical_path);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

bool forbidden_soname(std::string soname);

std::vector<std::string>
protected_runtime_identities(const std::vector<Mapping> &maps,
                             const FileImage &libc, std::string loader) {
  std::vector<std::string> protected_files{libc.path, std::move(loader)};
  for (const auto &mapping : maps) {
    if (mapping.canonical_path.empty()) {
      continue;
    }
    const auto &path = mapping.canonical_path;
    const auto slash = path.find_last_of('/');
    const auto basename =
        path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (!forbidden_soname(basename)) {
      continue;
    }
    if (std::find(protected_files.begin(), protected_files.end(), path) ==
        protected_files.end()) {
      protected_files.push_back(path);
    }
  }
  return protected_files;
}

std::string loader_identity(const FileImage &executable,
                            const std::vector<Mapping> &maps) {
  if (executable.interpreter.empty()) {
    unavailable("executable does not declare a dynamic loader");
  }
  const auto path =
      ::tracer::detail::canonical_mapped_path(executable.interpreter);
  if (path.empty() ||
      std::none_of(maps.begin(), maps.end(), [&](const Mapping &mapping) {
        return mapping.canonical_path == path;
      })) {
    unavailable("ELF PT_INTERP identity is not present in target mappings");
  }
  return path;
}

bool forbidden_soname(std::string soname) {
  std::transform(
      soname.begin(), soname.end(), soname.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  constexpr std::array<std::string_view, 27> denied = {
      "libc.so",     "libpthread.so", "libdl.so",    "libstdc++.so",
      "libgcc_s.so", "libgcc.so",     "libc++.so",   "libc++abi.so",
      "libllvm",     "libclang",      "libunwind",   "libasan",
      "libtsan",     "libubsan",      "libomp",      "libgomp",
      "libatomic",   "libm.so",       "librt.so",    "libresolv.so",
      "libnss_",     "libutil.so",    "libcrypt.so", "libanl.so",
      "libgfortran", "clang-tracer",  "libtracer"};
  return std::any_of(
      denied.begin(), denied.end(),
      [&](std::string_view prefix) { return soname.starts_with(prefix); });
}

void require_usable_module(const FileImage &image) {
  if (image.elf_type != ET_DYN) {
    throw std::invalid_argument(
        "approved application modules must be ET_DYN ELF images");
  }
  auto filename = image.path.substr(image.path.find_last_of('/') + 1);
  if (forbidden_soname(image.soname) || forbidden_soname(filename)) {
    throw std::invalid_argument(
        "system runtime, compiler, and tracer modules cannot be approved");
  }
  bool has_executable_load = false;
  for (const auto &p : image.loads) {
    if ((p.p_flags & PF_X) != 0 && p.p_filesz != 0) {
      has_executable_load = true;
    }
  }
  if (!has_executable_load || image.eh_frame.empty()) {
    throw std::invalid_argument(
        "approved modules need executable pages and DWARF CFI");
  }

  llvm::DWARFDebugFrame frame(llvm::Triple::x86_64, true, image.eh_frame_addr);
  llvm::DWARFDataExtractor data(llvm::StringRef(image.eh_frame), true, 8);
  if (auto error = frame.parse(data)) {
    llvm::consumeError(std::move(error));
    throw std::invalid_argument("approved module has malformed DWARF CFI");
  }
  bool found_executable_fde = false;
  for (const auto &entry : frame.entries()) {
    const auto *fde = llvm::dyn_cast<llvm::dwarf::FDE>(&entry);
    if (!fde || fde->getAddressRange() == 0) {
      continue;
    }
    if (fde->getAddressRange() >
        std::numeric_limits<std::uint64_t>::max() - fde->getInitialLocation()) {
      throw std::invalid_argument(
          "approved module has overflowing DWARF FDE range");
    }
    for (const auto &p : image.loads) {
      if ((p.p_flags & PF_X) == 0 || p.p_filesz == 0 ||
          fde->getInitialLocation() >= p.p_vaddr + p.p_filesz ||
          p.p_vaddr >= fde->getInitialLocation() + fde->getAddressRange()) {
        continue;
      }
      auto table = llvm::dwarf::createUnwindTable(fde);
      if (!table) {
        llvm::consumeError(table.takeError());
        throw std::invalid_argument(
            "approved module has an invalid executable CFI frame");
      }
      found_executable_fde = true;
    }
  }
  if (!found_executable_fde) {
    throw std::invalid_argument(
        "approved module has no CFI covering executable code");
  }
}

std::vector<FileImage> revalidate_modules(
    const std::vector<std::shared_ptr<const detail::ApprovedModuleIdentity>>
        &modules,
    const std::vector<Mapping> &maps,
    const std::vector<std::string> &protected_files) {
  if (modules.size() > 256) {
    throw std::invalid_argument(
        "at most 256 discovered application modules are supported");
  }
  std::vector<FileImage> images;
  std::vector<std::string> seen;
  images.reserve(modules.size());
  seen.reserve(modules.size());
  for (const auto &module : modules) {
    if (!module) {
      throw std::invalid_argument("application module handle is empty");
    }
    const auto &identity = *module;
    const auto current_path =
        ::tracer::detail::canonical_mapped_path(identity.path);
    if (current_path.empty() || current_path != identity.path) {
      throw std::runtime_error(
          "approved application module path is no longer canonical: " +
          identity.path);
    }
    if (std::find(seen.begin(), seen.end(), identity.path) != seen.end()) {
      throw std::invalid_argument(
          "application module handles contain duplicate canonical paths");
    }
    seen.push_back(identity.path);
    if (std::find(protected_files.begin(), protected_files.end(), identity.path) !=
        protected_files.end()) {
      throw std::runtime_error(
          "approved application module path aliases a protected runtime image");
    }
    FileImage image;
    try {
      image = load_image(identity.path, maps);
      require_usable_module(image);
      if (image.bias != identity.load_bias) {
        throw std::runtime_error(
            "approved application module load bias changed");
      }
    } catch (const std::exception &error) {
      throw std::runtime_error(
          "approved application module is no longer valid: " + identity.path +
          ": " + error.what());
    }
    images.push_back(std::move(image));
  }
  return images;
}

const FileImage *application_image_for(std::uint64_t pc,
                                       const FileImage &executable,
                                       const std::vector<FileImage> &modules,
                                       const std::vector<Mapping> &maps) {
  if (executable.executable_pc(pc, maps)) {
    return &executable;
  }
  for (const auto &module : modules) {
    if (module.executable_pc(pc, maps)) {
      return &module;
    }
  }
  return nullptr;
}

std::vector<llvm::dwarf::UnwindRow> rows_for(const FileImage &image,
                                             std::uint64_t runtime_pc) {
  if (runtime_pc < image.bias) {
    unavailable("frame address precedes its ELF load bias");
  }
  const auto link_pc = runtime_pc - image.bias;
  llvm::DWARFDebugFrame frame(llvm::Triple::x86_64, true, image.eh_frame_addr);
  llvm::DWARFDataExtractor data(llvm::StringRef(image.eh_frame), true, 8);
  if (auto error = frame.parse(data)) {
    llvm::consumeError(std::move(error));
    unavailable("LLVM could not parse target .eh_frame data");
  }
  for (const auto &entry : frame.entries()) {
    const auto *fde = llvm::dyn_cast<llvm::dwarf::FDE>(&entry);
    if (!fde || link_pc < fde->getInitialLocation() ||
        link_pc - fde->getInitialLocation() >= fde->getAddressRange()) {
      continue;
    }
    const auto *cie = fde->getLinkedCIE();
    if (!cie || cie->getAugmentationString().contains('S')) {
      unavailable("signal-frame or incomplete DWARF CFI is not admissible");
    }
    auto table = llvm::dwarf::createUnwindTable(fde);
    if (!table) {
      llvm::consumeError(table.takeError());
      unavailable("LLVM rejected target DWARF call-frame rules");
    }
    return std::vector<llvm::dwarf::UnwindRow>(table->begin(), table->end());
  }
  unavailable("no DWARF CFI covers an application or startup frame");
}

std::uint64_t reg_value(const user_regs_struct &r, unsigned dwarf_reg) {
  switch (dwarf_reg) {
  case 0:
    return r.rax;
  case 1:
    return r.rdx;
  case 2:
    return r.rcx;
  case 3:
    return r.rbx;
  case 4:
    return r.rsi;
  case 5:
    return r.rdi;
  case 6:
    return r.rbp;
  case 7:
    return r.rsp;
  case 8:
    return r.r8;
  case 9:
    return r.r9;
  case 10:
    return r.r10;
  case 11:
    return r.r11;
  case 12:
    return r.r12;
  case 13:
    return r.r13;
  case 14:
    return r.r14;
  case 15:
    return r.r15;
  case 16:
    return r.rip;
  default:
    unavailable("DWARF CFI requires an unsupported x86_64 register");
  }
}

struct StackBounds {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
};

StackBounds original_stack(const std::vector<Mapping> &maps,
                           std::uint64_t rsp) {
  for (const auto &m : maps) {
    if (m.readable && m.begin <= rsp && rsp < m.end) {
      return {m.begin, m.end};
    }
  }
  unavailable("stopped stack pointer is outside a readable mapping");
}

std::uint64_t add_signed(std::uint64_t base, std::int32_t offset) {
  const auto delta = static_cast<std::int64_t>(offset);
  if (delta < 0) {
    const auto magnitude = static_cast<std::uint64_t>(-delta);
    if (base < magnitude) {
      unavailable("DWARF offset underflows the address space");
    }
    return base - magnitude;
  }
  const auto magnitude = static_cast<std::uint64_t>(delta);
  if (base > std::numeric_limits<std::uint64_t>::max() - magnitude) {
    unavailable("DWARF offset overflows the address space");
  }
  return base + magnitude;
}

void require_stack_range(const StackBounds &stack, std::uint64_t address,
                         std::size_t length) {
  if (address < stack.begin || address > stack.end ||
      length > stack.end - address) {
    unavailable("DWARF unwind read leaves the original readable stack mapping");
  }
}

void set_reg(user_regs_struct &r, unsigned dwarf_reg, std::uint64_t value) {
  switch (dwarf_reg) {
  case 0:
    r.rax = value;
    break;
  case 1:
    r.rdx = value;
    break;
  case 2:
    r.rcx = value;
    break;
  case 3:
    r.rbx = value;
    break;
  case 4:
    r.rsi = value;
    break;
  case 5:
    r.rdi = value;
    break;
  case 6:
    r.rbp = value;
    break;
  case 7:
    r.rsp = value;
    break;
  case 8:
    r.r8 = value;
    break;
  case 9:
    r.r9 = value;
    break;
  case 10:
    r.r10 = value;
    break;
  case 11:
    r.r11 = value;
    break;
  case 12:
    r.r12 = value;
    break;
  case 13:
    r.r13 = value;
    break;
  case 14:
    r.r14 = value;
    break;
  case 15:
    r.r15 = value;
    break;
  case 16:
    r.rip = value;
    break;
  default:
    unavailable("DWARF CFI requires an unsupported x86_64 register");
  }
}

std::uint64_t read_remote(pid_t pid, std::uint64_t address,
                          const StackBounds &stack) {
  require_stack_range(stack, address, sizeof(std::uint64_t));
  std::uint64_t value = 0;
  iovec local{&value, sizeof(value)};
  iovec remote{reinterpret_cast<void *>(address), sizeof(value)};
  if (::process_vm_readv(pid, &local, 1, &remote, 1, 0) != sizeof(value)) {
    unavailable("cannot read a stopped target stack frame");
  }
  return value;
}

user_regs_struct unwind_one(pid_t pid, const FileImage &image, std::uint64_t pc,
                            const user_regs_struct &regs,
                            std::array<bool, 17> &known,
                            const StackBounds &stack,
                            bool pc_is_return_address = false) {
  if (pc_is_return_address && pc == 0) {
    unavailable("invalid frame return address");
  }
  const auto lookup_pc = pc_is_return_address ? pc - 1 : pc;
  auto rows = rows_for(image, lookup_pc);
  const llvm::dwarf::UnwindRow *row = nullptr;
  const auto link_pc = lookup_pc - image.bias;
  for (const auto &candidate : rows) {
    if (!candidate.hasAddress() || candidate.getAddress() > link_pc) {
      continue;
    }
    if (!row || candidate.getAddress() > row->getAddress()) {
      row = &candidate;
    }
  }
  if (!row) {
    unavailable("no CFI row applies at the stopped instruction");
  }
  const auto &cfa_rule = row->getCFAValue();
  if (cfa_rule.getLocation() != llvm::dwarf::UnwindLocation::RegPlusOffset ||
      cfa_rule.hasAddressSpace() || cfa_rule.getRegister() >= known.size() ||
      !known[cfa_rule.getRegister()]) {
    unavailable("unsupported DWARF CFA rule");
  }
  const auto cfa_reg = cfa_rule.getRegister();
  const auto cfa = add_signed(reg_value(regs, cfa_reg), cfa_rule.getOffset());
  if (cfa <= regs.rsp || cfa < stack.begin || cfa > stack.end) {
    unavailable(
        "DWARF CFA is not a strictly increasing address on the original stack");
  }

  user_regs_struct caller = regs;
  auto caller_known = known;
  constexpr std::array<unsigned, 17> registers = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  for (const unsigned reg : registers) {
    const auto saved = row->getRegisterLocations().getRegisterLocation(reg);
    if (!saved ||
        saved->getLocation() == llvm::dwarf::UnwindLocation::Unspecified) {
      const bool callee_saved =
          reg == 3 || reg == 6 || (reg >= 12 && reg <= 15);
      caller_known[reg] = callee_saved && known[reg];
      continue;
    }
    if (saved->getLocation() == llvm::dwarf::UnwindLocation::Same) {
      caller_known[reg] = known[reg];
      continue;
    }
    if (saved->getLocation() == llvm::dwarf::UnwindLocation::Undefined) {
      caller_known[reg] = false;
      continue;
    }
    if (saved->getLocation() == llvm::dwarf::UnwindLocation::CFAPlusOffset) {
      const auto value = add_signed(cfa, saved->getOffset());
      set_reg(caller, reg,
              saved->getDereference() ? read_remote(pid, value, stack) : value);
      caller_known[reg] = true;
      continue;
    }
    if (saved->getLocation() == llvm::dwarf::UnwindLocation::RegPlusOffset &&
        !saved->hasAddressSpace() && !saved->getDereference()) {
      const auto source = saved->getRegister();
      if (source >= known.size() || !known[source]) {
        caller_known[reg] = false;
        continue;
      }
      set_reg(caller, reg,
              add_signed(reg_value(regs, source), saved->getOffset()));
      caller_known[reg] = true;
      continue;
    }
    unavailable("unsupported DWARF saved-register rule");
  }
  caller.rsp = cfa;
  caller_known[7] = true;
  if (!caller_known[16]) {
    unavailable("DWARF CFI cannot recover the caller instruction pointer");
  }
  known = caller_known;
  return caller;
}

} // namespace

std::vector<ApplicationModule> discover_application_modules(pid_t pid) {
  if (pid <= 0) {
    throw std::invalid_argument("application module process id is invalid");
  }
  const auto maps = read_mappings(pid);
  const auto executable = load_executable(pid, maps);
  const auto libc = load_libc(maps);
  const auto loader = loader_identity(executable, maps);
  const auto protected_files = protected_runtime_identities(maps, libc, loader);

  struct Candidate {
    std::string path;
  };

  std::vector<Candidate> candidates;
  for (const auto &mapping : maps) {
    if (!mapping.executable || mapping.canonical_path.empty()) {
      continue;
    }
    const auto &path = mapping.canonical_path;
    if (path == executable.path ||
        std::find(protected_files.begin(), protected_files.end(), path) !=
            protected_files.end()) {
      continue;
    }
    auto found = std::find_if(
        candidates.begin(), candidates.end(),
        [&](const Candidate &candidate) { return candidate.path == path; });
    if (found == candidates.end()) {
      candidates.push_back({path});
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
              return a.path < b.path;
            });
  if (candidates.size() > 256) {
    throw std::runtime_error(
        "mapped application module snapshot exceeds 256 identities");
  }

  const auto initial_maps = executable_map_snapshot(maps);
  std::vector<ApplicationModule> result;
  result.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    const int fd = ::open(candidate.path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
      continue;
    }

    FileImage image;
    try {
      image = load_image_fd(candidate.path, fd, maps);
      if (image.elf_type != ET_DYN ||
          std::find(protected_files.begin(), protected_files.end(),
                    candidate.path) != protected_files.end()) {
        ::close(fd);
        continue;
      }
      require_usable_module(image);
    } catch (const IdlePointUnavailable &) {
      ::close(fd);
      continue; // Non-ELF, unreadable metadata, or unsupported mapping.
    } catch (const std::invalid_argument &) {
      ::close(fd);
      continue; // Known runtime and modules without supported CFI are excluded.
    } catch (const std::exception &) {
      ::close(fd);
      throw;
    }

    auto identity = std::make_shared<detail::ApprovedModuleIdentity>();
    identity->path = candidate.path;
    identity->load_bias = image.bias;
    ::close(fd);
    result.emplace_back(ApplicationModule(std::move(identity)));
  }

  if (executable_map_snapshot(read_mappings(pid)) != initial_maps) {
    throw std::runtime_error(
        "executable mappings changed during module discovery");
  }
  return result;
}

void validate_application_modules(pid_t pid,
                                  std::span<const ApplicationModule> modules) {
  if (pid <= 0) {
    throw std::invalid_argument("application module process id is invalid");
  }
  if (modules.size() > 256) {
    throw std::invalid_argument(
        "at most 256 discovered application modules are supported");
  }
  if (modules.empty()) {
    return;
  }
  const auto maps = read_mappings(pid);
  const auto executable = load_executable(pid, maps);
  const auto libc = load_libc(maps);
  const auto loader = loader_identity(executable, maps);
  const auto protected_files = protected_runtime_identities(maps, libc, loader);
  std::vector<std::shared_ptr<const detail::ApprovedModuleIdentity>> identities;
  identities.reserve(modules.size());
  for (const auto &module : modules) {
    identities.push_back(module.identity_);
  }
  (void)revalidate_modules(identities, maps, protected_files);
}

std::size_t
require_application_rendezvous(pid_t pid, const user_regs_struct &regs,
                               std::span<const ApplicationModule> modules) {
  if (pid <= 0 || regs.rip == 0) {
    unavailable("invalid stopped-process state");
  }
  const auto maps = read_mappings(pid);
  auto executable = load_executable(pid, maps);
  auto libc = load_libc(maps);
  const auto loader = loader_identity(executable, maps);
  const auto protected_files = protected_runtime_identities(maps, libc, loader);
  std::vector<std::shared_ptr<const detail::ApprovedModuleIdentity>> identities;
  identities.reserve(modules.size());
  for (const auto &module : modules) {
    identities.push_back(module.identity_);
  }
  auto approved = revalidate_modules(identities, maps, protected_files);
  const auto *start_call_main = libc.named("__libc_start_call_main");
  const auto *start_main = libc.named("__libc_start_main");
  const auto *thread_start = libc.named("start_thread");
  const auto *clone = libc.named("__clone");
  const auto *clone3 = libc.named("__clone3");
  const FileImage *current_image =
      application_image_for(regs.rip, executable, approved, maps);
  if (!current_image) {
    unavailable("stopped instruction is outside a mapped executable segment");
  }

  const auto stack = original_stack(maps, regs.rsp);
  user_regs_struct current = regs;
  std::array<bool, 17> known;
  known.fill(true);
  std::uint64_t pc = regs.rip;
  bool pc_is_return_address = false;
  std::size_t application_frames = 1;
  std::size_t traversed_frames = 1;
  std::set<std::pair<std::uint64_t, std::uint64_t>> seen;
  seen.emplace(regs.rip, regs.rsp);

  while (traversed_frames < 128) {
    auto caller = unwind_one(pid, *current_image, pc, current, known, stack,
                             pc_is_return_address);
    const auto return_pc = caller.rip;
    if (return_pc == 0) {
      unavailable("DWARF CFI produced an invalid return address");
    }
    const auto caller_pc = return_pc - 1;
    const FileImage *caller_image =
        application_image_for(caller_pc, executable, approved, maps);
    if (caller_image) {
      if (traversed_frames + 1 + 2 > 128) {
        unavailable("application unwind exceeds the 128-frame admission bound");
      }
      if (!seen.emplace(caller_pc, caller.rsp).second) {
        unavailable("application unwind contains a repeated frame cycle");
      }
      ++application_frames;
      ++traversed_frames;
      current = caller;
      current_image = caller_image;
      pc = return_pc;
      pc_is_return_address = true;
      continue;
    }
    const auto within = [&](const FileImage::Function *function,
                            std::uint64_t address) {
      return function && address >= libc.runtime(*function) &&
             address - libc.runtime(*function) < function->size &&
             libc.executable_pc(address, maps);
    };
    if (within(start_call_main, caller_pc)) {
      if (!start_main || traversed_frames + 2 > 128) {
        unavailable("required exact main startup symbols are unavailable");
      }
      ++traversed_frames;
      auto startup_caller =
          unwind_one(pid, libc, return_pc, caller, known, stack, true);
      const auto startup_return_pc = startup_caller.rip;
      if (startup_return_pc == 0 ||
          !within(start_main, startup_return_pc - 1)) {
        unavailable(
            "libc startup caller chain does not reach __libc_start_main");
      }
      return application_frames;
    }
    if (!within(thread_start, caller_pc) || traversed_frames + 2 > 128) {
      unavailable(
          "application unwind leaves the executable outside a validated "
          "libc startup chain");
    }

    ++traversed_frames;
    auto thread_caller =
        unwind_one(pid, libc, return_pc, caller, known, stack, true);
    const auto thread_return_pc = thread_caller.rip;
    const auto clone_pc = thread_return_pc == 0 ? 0 : thread_return_pc - 1;
    if (thread_return_pc == 0 ||
        (!within(clone, clone_pc) && !within(clone3, clone_pc))) {
      unavailable(
          "pthread startup caller chain does not reach exact __clone or "
          "__clone3");
    }
    return application_frames;
  }
  unavailable("application unwind exceeds the 128-frame admission bound");
}

} // namespace tracer::injector

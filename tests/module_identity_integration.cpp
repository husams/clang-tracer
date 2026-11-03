#include "../src/admission.hpp"
#include "../src/mapped_path.hpp"

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using tracer::injector::ApplicationModule;
using tracer::injector::discover_application_modules;
using tracer::injector::validate_application_modules;

namespace {

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    constexpr std::string_view prefix = "/tmp/clang-tracer-modules-XXXXXX";
    std::array<char, prefix.size() + 1> pattern{};
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    pattern[prefix.size()] = '\0';
    const char *created = ::mkdtemp(pattern.data());
    if (created == nullptr) {
      throw std::runtime_error("mkdtemp failed: " +
                               std::string(std::strerror(errno)));
    }
    path_ = created;
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  [[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
  fs::path path_;
};

class SharedObject final {
public:
  explicit SharedObject(const fs::path &path) {
    handle_ = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr) {
      const char *error = ::dlerror();
      throw std::runtime_error("dlopen failed for " + path.string() + ": " +
                               (error == nullptr ? "unknown error" : error));
    }
  }

  ~SharedObject() {
    if (handle_ != nullptr) {
      ::dlclose(handle_);
    }
  }

  void close() {
    if (handle_ != nullptr) {
      if (::dlclose(handle_) != 0) {
        throw std::runtime_error("dlclose failed for test module");
      }
      handle_ = nullptr;
    }
  }

  SharedObject(const SharedObject &) = delete;
  SharedObject &operator=(const SharedObject &) = delete;

private:
  void *handle_ = nullptr;
};

template <typename Exception, typename Callable>
void expect_throws(Callable &&callable, std::string_view description) {
  try {
    std::forward<Callable>(callable)();
  } catch (const Exception &) {
    return;
  }
  throw std::runtime_error("expected exception for " +
                           std::string(description));
}

fs::path copy_module(const fs::path &fixture, const fs::path &directory,
                     std::string_view name) {
  const fs::path destination = directory / name;
  fs::copy_file(fixture, destination, fs::copy_options::overwrite_existing);
  return destination;
}

std::vector<ApplicationModule>::const_iterator
find_module(const std::vector<ApplicationModule> &modules,
            const fs::path &path);

void corrupt_eh_frame_length(const fs::path &path) {
  const int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    throw std::runtime_error("could not open module for CFI mutation: " +
                             std::string(std::strerror(errno)));
  }
  try {
    struct stat status{};
    Elf64_Ehdr header{};
    if (::fstat(fd, &status) != 0 || status.st_size < 0 ||
        ::pread(fd, &header, sizeof(header), 0) !=
            static_cast<ssize_t>(sizeof(header)) ||
        header.e_shentsize != sizeof(Elf64_Shdr) ||
        header.e_shstrndx >= header.e_shnum ||
        header.e_shoff > static_cast<std::uint64_t>(status.st_size) ||
        header.e_shnum >
            (static_cast<std::uint64_t>(status.st_size) - header.e_shoff) /
                sizeof(Elf64_Shdr)) {
      throw std::runtime_error("could not inspect CFI mutation fixture");
    }
    std::vector<Elf64_Shdr> sections(header.e_shnum);
    if (::pread(fd, sections.data(), sections.size() * sizeof(Elf64_Shdr),
                static_cast<off_t>(header.e_shoff)) !=
        static_cast<ssize_t>(sections.size() * sizeof(Elf64_Shdr))) {
      throw std::runtime_error("could not read CFI mutation sections");
    }
    const auto &names = sections[header.e_shstrndx];
    if (names.sh_offset > static_cast<std::uint64_t>(status.st_size) ||
        names.sh_size >
            static_cast<std::uint64_t>(status.st_size) - names.sh_offset) {
      throw std::runtime_error("CFI mutation section names are malformed");
    }
    std::vector<char> section_names(static_cast<std::size_t>(names.sh_size));
    if (::pread(fd, section_names.data(), section_names.size(),
                static_cast<off_t>(names.sh_offset)) !=
        static_cast<ssize_t>(section_names.size())) {
      throw std::runtime_error("could not read CFI mutation section names");
    }
    const auto eh_frame = std::find_if(
        sections.begin(), sections.end(), [&](const Elf64_Shdr &section) {
          if (section.sh_name >= names.sh_size) {
            return false;
          }
          const char *name = section_names.data() + section.sh_name;
          return std::memchr(name, 0, names.sh_size - section.sh_name) &&
                 std::strcmp(name, ".eh_frame") == 0;
        });
    if (eh_frame == sections.end() ||
        eh_frame->sh_size < sizeof(std::uint32_t) ||
        eh_frame->sh_offset > static_cast<std::uint64_t>(status.st_size) ||
        eh_frame->sh_size >
            static_cast<std::uint64_t>(status.st_size) - eh_frame->sh_offset) {
      throw std::runtime_error("CFI mutation fixture has no valid .eh_frame");
    }
    constexpr std::uint32_t malformed_length = 0x7fffffff;
    if (::pwrite(fd, &malformed_length, sizeof(malformed_length),
                 static_cast<off_t>(eh_frame->sh_offset)) !=
        static_cast<ssize_t>(sizeof(malformed_length))) {
      throw std::runtime_error("could not corrupt CFI mutation fixture");
    }
  } catch (...) {
    ::close(fd);
    throw;
  }
  ::close(fd);
}

void verify_cfi_snapshot_cache(pid_t pid, const fs::path &fixture,
                               const fs::path &directory) {
  const fs::path module_path =
      copy_module(fixture, directory, "changed-cfi.so");
  SharedObject handle(module_path);
  const auto discovered = discover_application_modules(pid);
  const auto module_it = find_module(discovered, module_path);
  if (module_it == discovered.end()) {
    throw std::runtime_error("loaded CFI mutation module was not discovered");
  }
  std::vector<ApplicationModule> module{*module_it};
  validate_application_modules(pid, module);

  struct stat before{};
  if (::stat(module_path.c_str(), &before) != 0) {
    throw std::runtime_error("stat before timestamp update failed: " +
                             std::string(std::strerror(errno)));
  }
  const timespec times[2] = {{0, UTIME_OMIT}, {before.st_mtime + 10, 0}};
  if (::utimensat(AT_FDCWD, module_path.c_str(), times, 0) != 0) {
    throw std::runtime_error("utimensat failed: " +
                             std::string(std::strerror(errno)));
  }
  validate_application_modules(pid, module);

  corrupt_eh_frame_length(module_path);
  expect_throws<std::runtime_error>(
      [&] { validate_application_modules(pid, module); },
      "changed malformed CFI after module discovery");
}

class CoalescedLoadMapping final {
public:
  CoalescedLoadMapping(const fs::path &fixture, const fs::path &directory,
                       std::string_view name, bool wrong_second_offset = false)
      : path_(copy_module(fixture, directory, name)),
        wrong_second_offset_(wrong_second_offset) {
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
      throw std::runtime_error("could not get page size for coalesced ELF fixture");
    }
    page_size_ = static_cast<std::size_t>(page_size);
    const int fd = ::open(path_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      throw std::runtime_error("could not open coalesced ELF fixture: " +
                               std::string(std::strerror(errno)));
    }
    try {
      struct stat status{};
      Elf64_Ehdr header{};
      if (::fstat(fd, &status) != 0 || status.st_size < 2 * page_size ||
          ::pread(fd, &header, sizeof(header), 0) != sizeof(header) ||
          std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
          header.e_phentsize != sizeof(Elf64_Phdr) ||
          header.e_phoff > static_cast<std::uint64_t>(status.st_size) ||
          header.e_phnum >
              (static_cast<std::uint64_t>(status.st_size) - header.e_phoff) /
                  sizeof(Elf64_Phdr)) {
        throw std::runtime_error("fixture is not a usable ELF shared object");
      }
      std::vector<Elf64_Phdr> headers(header.e_phnum);
      if (::pread(fd, headers.data(), headers.size() * sizeof(Elf64_Phdr),
                  static_cast<off_t>(header.e_phoff)) !=
          static_cast<ssize_t>(headers.size() * sizeof(Elf64_Phdr))) {
        throw std::runtime_error("could not read coalesced ELF program headers");
      }
      std::vector<std::size_t> load_indices;
      for (std::size_t index = 0; index < headers.size(); ++index) {
        if (headers[index].p_type == PT_LOAD) {
          load_indices.push_back(index);
        }
      }
      if (load_indices.size() < 2 || status.st_size < 2 * page_size) {
        throw std::runtime_error("fixture must have at least two load segments and two file pages");
      }
      for (std::size_t index = 0; index < 2; ++index) {
        auto &segment = headers[load_indices[index]];
        segment.p_offset = index * page_size_;
        segment.p_vaddr = index * page_size_;
        segment.p_paddr = segment.p_vaddr;
        segment.p_filesz = page_size_;
        segment.p_memsz = page_size_;
        segment.p_flags = PF_R | PF_X;
        segment.p_align = page_size_;
      }
      for (std::size_t index = 2; index < load_indices.size(); ++index) {
        headers[load_indices[index]].p_type = PT_NULL;
      }
      if (::pwrite(fd, headers.data(), headers.size() * sizeof(Elf64_Phdr),
                   static_cast<off_t>(header.e_phoff)) !=
          static_cast<ssize_t>(headers.size() * sizeof(Elf64_Phdr))) {
        throw std::runtime_error("could not write coalesced ELF program headers");
      }
      mapping_size_ = 2 * page_size_;
      mapping_ = ::mmap(nullptr, mapping_size_, PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        throw std::runtime_error("could not reserve coalesced ELF mapping range");
      }
      if (::mmap(mapping_, page_size_, PROT_READ | PROT_EXEC,
                 MAP_PRIVATE | MAP_FIXED, fd, 0) == MAP_FAILED ||
          ::mmap(static_cast<char *>(mapping_) + page_size_, page_size_,
                 PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, fd,
                 static_cast<off_t>(wrong_second_offset_ ? 0 : page_size_)) ==
              MAP_FAILED) {
        ::munmap(mapping_, mapping_size_);
        mapping_ = nullptr;
        throw std::runtime_error("could not map adjacent coalesced ELF segments");
      }
    } catch (...) {
      ::close(fd);
      throw;
    }
    ::close(fd);
    if (wrong_second_offset_) {
      verify_split_mapping();
    } else {
      verify_coalesced_mapping();
    }
  }

  ~CoalescedLoadMapping() {
    if (mapping_) {
      ::munmap(mapping_, mapping_size_);
    }
  }

  CoalescedLoadMapping(const CoalescedLoadMapping &) = delete;
  CoalescedLoadMapping &operator=(const CoalescedLoadMapping &) = delete;

  [[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
  void verify_coalesced_mapping() const {
    std::ostringstream range;
    range << std::hex << reinterpret_cast<std::uintptr_t>(mapping_) << '-'
          << reinterpret_cast<std::uintptr_t>(mapping_) + mapping_size_;
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
      if (line.rfind(range.str() + " r-xp ", 0) == 0 &&
          line.find(path_.string()) != std::string::npos) {
        return;
      }
    }
    throw std::runtime_error(
        "kernel did not coalesce adjacent same-permission ELF file mappings");
  }

  void verify_split_mapping() const {
    std::ostringstream first, second;
    first << std::hex << reinterpret_cast<std::uintptr_t>(mapping_) << '-'
          << reinterpret_cast<std::uintptr_t>(mapping_) + page_size_;
    second << std::hex
           << reinterpret_cast<std::uintptr_t>(mapping_) + page_size_ << '-'
           << reinterpret_cast<std::uintptr_t>(mapping_) + mapping_size_;
    bool found_first = false, found_second = false;
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
      found_first |= line.rfind(first.str() + " r-xp 00000000 ", 0) == 0 &&
                     line.find(path_.string()) != std::string::npos;
      found_second |= line.rfind(second.str() + " r-xp 00000000 ", 0) == 0 &&
                      line.find(path_.string()) != std::string::npos;
    }
    if (!found_first || !found_second) {
      throw std::runtime_error(
          "kernel did not retain the wrong-offset mappings as separate VMAs");
    }
  }

  fs::path path_;
  void *mapping_ = nullptr;
  std::size_t page_size_ = 0;
  std::size_t mapping_size_ = 0;
  bool wrong_second_offset_ = false;
};

bool same_mapped_path(const fs::path &left, const fs::path &right) {
  const auto canonical_left =
      tracer::detail::canonical_mapped_path(left.string());
  return !canonical_left.empty() &&
         canonical_left == tracer::detail::canonical_mapped_path(right.string());
}

std::vector<ApplicationModule>::const_iterator
find_module(const std::vector<ApplicationModule> &modules,
            const fs::path &path) {
  return std::find_if(modules.begin(), modules.end(),
                      [&](const ApplicationModule &module) {
                        return same_mapped_path(module.path(), path);
                      });
}

void require_discovered(const std::vector<ApplicationModule> &modules,
                        const fs::path &path, std::string_view description) {
  if (find_module(modules, path) == modules.end()) {
    std::ostringstream details;
    details << "automatic discovery omitted " << description << "; target map entries:";
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);) {
      if (line.find(path.string()) != std::string::npos) {
        details << "\n  " << line;
      }
    }
    details << "; discovered paths:";
    for (const auto &module : modules) {
      details << "\n  " << module.path();
    }
    throw std::runtime_error(details.str());
  }
}

void require_excluded(const std::vector<ApplicationModule> &modules,
                      const fs::path &path, std::string_view description) {
  if (find_module(modules, path) != modules.end()) {
    throw std::runtime_error("automatic discovery included " +
                             std::string(description));
  }
}

std::string loaded_path(const void *address, std::string_view description) {
  Dl_info info{};
  if (::dladdr(address, &info) == 0 || info.dli_fname == nullptr) {
    throw std::runtime_error("dladdr could not resolve " +
                             std::string(description));
  }
  return info.dli_fname;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const bool cfi_cache_only =
        argc == 4 && std::string_view(argv[3]) == "--cfi-cache-only";
    if ((argc != 3 && !cfi_cache_only) ||
        std::string_view(argv[1]) != "--module") {
      throw std::invalid_argument("usage: module_identity_integration --module "
                                  "PATH [--cfi-cache-only]");
    }

    const fs::path fixture = fs::canonical(argv[2]);
    TemporaryDirectory temporary;
    const pid_t pid = ::getpid();
    if (cfi_cache_only) {
      verify_cfi_snapshot_cache(pid, fixture, temporary.path());
      std::cout << "module CFI snapshot cache checks passed\n";
      return 0;
    }

    // The fixture copy must be discovered from the live mapping set and
    // represented by its canonical mapped path.
    const fs::path pinned_path =
        copy_module(fixture, temporary.path(), "pinned.so");
    SharedObject pinned_handle(pinned_path);
    std::vector<ApplicationModule> discovered =
        discover_application_modules(pid);
    require_discovered(discovered, pinned_path,
                       "the loaded private fixture copy");
    const auto pinned_it = find_module(discovered, pinned_path);
    std::vector<ApplicationModule> pinned{*pinned_it};
    validate_application_modules(pid, pinned);

    // A literal filename suffix is not procfs's unlinked-file marker when the
    // complete pathname still resolves to an existing mapped module.
    const auto literal_deleted =
        copy_module(fixture, temporary.path(), "literal (deleted)");
    SharedObject literal_deleted_handle(literal_deleted);
    discovered = discover_application_modules(pid);
    require_discovered(discovered, literal_deleted,
                       "a mapped file with a literal deleted-marker suffix");

    // Adjacent same-permission PT_LOADs can become one VMA. Discovery must
    // validate their covered offsets, not require every segment to begin a VMA.
    CoalescedLoadMapping coalesced(fixture, temporary.path(),
                                   "coalesced-loads.so");
    discovered = discover_application_modules(pid);
    require_discovered(discovered, coalesced.path(),
                       "a valid ELF with coalesced adjacent PT_LOAD mappings");
    CoalescedLoadMapping wrong_offset(fixture, temporary.path(),
                                      "wrong-offset-load.so", true);
    discovered = discover_application_modules(pid);
    require_excluded(discovered, wrong_offset.path(),
                     "same-file mappings with an incorrect PT_LOAD file offset");

    const fs::path alias_path = temporary.path() / "alias.so";
    fs::create_symlink(pinned_path, alias_path);
    require_discovered(discovered, alias_path,
                       "a mapped module through its symlink identity");

    // Metadata-only changes do not invalidate a session-fixed module path.
    const fs::path touched_path =
        copy_module(fixture, temporary.path(), "touched.so");
    SharedObject touched_handle(touched_path);
    discovered = discover_application_modules(pid);
    require_discovered(discovered, touched_path,
                       "the second loaded fixture copy");
    const auto touched_it = find_module(discovered, touched_path);
    std::vector<ApplicationModule> touched{*touched_it};
    struct stat before{};
    if (::stat(touched_path.c_str(), &before) != 0) {
      throw std::runtime_error("stat before timestamp update failed: " +
                               std::string(std::strerror(errno)));
    }
    const timespec times[2] = {{0, UTIME_OMIT}, {before.st_mtime + 10, 0}};
    if (::utimensat(AT_FDCWD, touched_path.c_str(), times, 0) != 0) {
      throw std::runtime_error("utimensat failed: " +
                               std::string(std::strerror(errno)));
    }
    validate_application_modules(pid, touched);

    verify_cfi_snapshot_cache(pid, fixture, temporary.path());

    const fs::path unmapped_after_snapshot_path =
        copy_module(fixture, temporary.path(), "unmapped-after-snapshot.so");
    SharedObject unmapped_after_snapshot_handle(unmapped_after_snapshot_path);
    discovered = discover_application_modules(pid);
    const auto unmapped_after_snapshot_it =
        find_module(discovered, unmapped_after_snapshot_path);
    if (unmapped_after_snapshot_it == discovered.end()) {
      throw std::runtime_error(
          "loaded module was missing before unmapping validation");
    }
    std::vector<ApplicationModule> unmapped_after_snapshot{
        *unmapped_after_snapshot_it};
    validate_application_modules(pid, unmapped_after_snapshot);
    unmapped_after_snapshot_handle.close();
    expect_throws<std::runtime_error>(
        [&] { validate_application_modules(pid, unmapped_after_snapshot); },
        "module removed from mappings after the session snapshot");

    // A present but unmapped file must not enter the automatic snapshot.
    const fs::path unmapped_path =
        copy_module(fixture, temporary.path(), "unmapped.so");
    discovered = discover_application_modules(pid);
    require_excluded(discovered, unmapped_path, "an unmapped private copy");
    const fs::path missing_path = temporary.path() / "missing.so";
    require_excluded(discovered, missing_path, "a missing private copy");

    // More than the former 16-entry allowlist limit is discovered when all
    // distinct copies are actually mapped.
    std::vector<fs::path> many_paths;
    std::vector<std::unique_ptr<SharedObject>> many_handles;
    many_paths.reserve(17);
    many_handles.reserve(17);
    for (int index = 0; index < 17; ++index) {
      const std::string name = "many-" + std::to_string(index) + ".so";
      many_paths.push_back(copy_module(fixture, temporary.path(), name));
      many_handles.push_back(std::make_unique<SharedObject>(many_paths.back()));
    }
    discovered = discover_application_modules(pid);
    for (const fs::path &path : many_paths) {
      require_discovered(discovered, path, "one of 17 loaded fixture copies");
    }

    // Resolve protected runtime identities from actual process addresses and
    // verify discovery excludes both the runtime objects and their symlink
    // aliases by underlying file identity.
    const fs::path libc_path = loaded_path(
        reinterpret_cast<const void *>(&::dl_iterate_phdr), "libc entry point");
    const unsigned long loader_base = ::getauxval(AT_BASE);
    if (loader_base == 0) {
      throw std::runtime_error("AT_BASE did not identify the runtime loader");
    }
    const fs::path loader_path = loaded_path(
        reinterpret_cast<const void *>(loader_base), "AT_BASE loader mapping");
    discovered = discover_application_modules(pid);
    require_excluded(discovered, libc_path, "libc");
    require_excluded(discovered, loader_path, "the runtime loader");

    const fs::path libc_alias = temporary.path() / "libc-alias.so";
    const fs::path loader_alias = temporary.path() / "loader-alias.so";
    fs::create_symlink(libc_path, libc_alias);
    fs::create_symlink(loader_path, loader_alias);
    require_excluded(discovered, libc_alias, "a libc symlink alias");
    require_excluded(discovered, loader_alias,
                     "a runtime loader symlink alias");

    std::cout << "module identity integration checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "module identity integration failed: " << error.what() << '\n';
    return 1;
  }
}

#include "../src/admission.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
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
    std::array<char, 32> pattern{};
    constexpr std::string_view prefix = "/tmp/clang-tracer-modules-XXXXXX";
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

struct FileIdentity {
  dev_t device;
  ino_t inode;
};

FileIdentity file_identity(const fs::path &path) {
  struct stat status{};
  if (::stat(path.c_str(), &status) != 0) {
    throw std::runtime_error("stat failed for " + path.string() + ": " +
                             std::strerror(errno));
  }
  return {status.st_dev, status.st_ino};
}

bool same_identity(const fs::path &left, const fs::path &right) {
  const FileIdentity a = file_identity(left);
  const FileIdentity b = file_identity(right);
  return a.device == b.device && a.inode == b.inode;
}

std::vector<ApplicationModule>::const_iterator
find_module(const std::vector<ApplicationModule> &modules,
            const fs::path &path) {
  return std::find_if(modules.begin(), modules.end(),
                      [&](const ApplicationModule &module) {
                        try {
                          return same_identity(module.path(), path);
                        } catch (const std::runtime_error &) {
                          // A prior discovery can retain a mapping whose
                          // pathname was later replaced or unlinked; it is not
                          // a match for a current filesystem path.
                          return false;
                        }
                      });
}

void require_discovered(const std::vector<ApplicationModule> &modules,
                        const fs::path &path, std::string_view description) {
  if (find_module(modules, path) == modules.end()) {
    throw std::runtime_error("automatic discovery omitted " +
                             std::string(description));
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
    if (argc != 3 || std::string_view(argv[1]) != "--module") {
      throw std::invalid_argument(
          "usage: module_identity_integration --module PATH");
    }

    const fs::path fixture = fs::canonical(argv[2]);
    TemporaryDirectory temporary;
    const pid_t pid = ::getpid();

    // The fixture copy must be discovered from the live mapping set. Select
    // its opaque handle by file identity only for the later mutation checks.
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

    const fs::path alias_path = temporary.path() / "alias.so";
    fs::create_symlink(pinned_path, alias_path);
    require_discovered(discovered, alias_path,
                       "a mapped module through its symlink identity");

    // Replacing the path leaves the original mapping pinned to the old inode.
    const fs::path replacement =
        copy_module(fixture, temporary.path(), "replacement.so");
    if (::rename(replacement.c_str(), pinned_path.c_str()) != 0) {
      throw std::runtime_error("rename replacement failed: " +
                               std::string(std::strerror(errno)));
    }
    expect_throws<std::runtime_error>(
        [&] { validate_application_modules(pid, pinned); },
        "replaced pinned module path");

    // Timestamp-only mutation invalidates an approval although mapped bytes
    // and the loader handle remain unchanged.
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
    expect_throws<std::runtime_error>(
        [&] { validate_application_modules(pid, touched); },
        "metadata-changed module");

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

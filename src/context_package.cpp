#include "context_package.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tracer/compat.hpp"

namespace tracer::context_package {
namespace {

constexpr std::string_view kManifestHeader = "clang-tracer-lineage-v1\n";
std::atomic<std::uint64_t> next_temp_file{1};

struct LocatedPackage {
  std::filesystem::path root;
  std::filesystem::path manifest;
};

bool path_exists(const std::filesystem::path &path) {
  std::error_code error;
  return std::filesystem::exists(path, error) && !error;
}

std::filesystem::path canonical_or_empty(const std::filesystem::path &path) {
  if (path.empty())
    return {};
  std::error_code error;
  auto result = std::filesystem::weakly_canonical(path, error);
  return error ? std::filesystem::path{} : std::move(result);
}

std::filesystem::path git_common_directory(
    const std::filesystem::path &package_root) {
  const auto marker = package_root / ".git";
  std::error_code error;
  if (std::filesystem::is_directory(marker, error) && !error)
    return canonical_or_empty(marker);
  if (error || !std::filesystem::is_regular_file(marker, error) || error)
    return {};

  std::ifstream git_file(marker);
  std::string line;
  if (!std::getline(git_file, line) || !::tracer::compat::starts_with(line, "gitdir: ")) {
    return {};
  }
  auto git_directory = std::filesystem::path(line.substr(8));
  if (git_directory.is_relative())
    git_directory = package_root / git_directory;
  git_directory = canonical_or_empty(git_directory);
  if (git_directory.empty())
    return {};

  const auto common_file = git_directory / "commondir";
  if (std::filesystem::is_regular_file(common_file, error) && !error) {
    std::ifstream common_input(common_file);
    std::string common;
    if (std::getline(common_input, common) && !common.empty()) {
      auto common_directory = std::filesystem::path(common);
      if (common_directory.is_relative())
        common_directory = git_directory / common_directory;
      auto resolved = canonical_or_empty(common_directory);
      if (!resolved.empty())
        return resolved;
    }
  }
  return git_directory;
}

std::filesystem::path manifest_for_root(
    const std::filesystem::path &root) {
  const auto git_directory = git_common_directory(root);
  if (!git_directory.empty())
    return git_directory / "clang-tracer" / "source-lineage";
  return root / ".clang-tracer" / "source-lineage";
}

std::filesystem::path source_directory(
    const std::filesystem::path &source_path) {
  auto source = canonical_or_empty(source_path);
  if (source.empty())
    return {};
  std::error_code error;
  if (std::filesystem::is_directory(source, error) && !error)
    return source;
  return source.parent_path();
}

LocatedPackage find_existing_manifest(
    const std::filesystem::path &source_path) {
  auto directory = source_directory(source_path);
  while (!directory.empty()) {
    const auto plain_manifest = directory / ".clang-tracer" / "source-lineage";
    if (path_exists(plain_manifest))
      return {directory, plain_manifest};
    std::error_code error;
    const bool is_package_boundary =
        std::filesystem::exists(directory / ".git", error) && !error;
    const auto git_directory = git_common_directory(directory);
    if (!git_directory.empty()) {
      const auto git_manifest = git_directory / "clang-tracer" / "source-lineage";
      if (path_exists(git_manifest))
        return {directory, git_manifest};
    }
    if (is_package_boundary ||
        std::filesystem::is_regular_file(directory / "compile_commands.json",
                                         error))
      return {};
    const auto parent = directory.parent_path();
    if (parent == directory)
      break;
    directory = parent;
  }
  return {};
}

std::filesystem::path discover_root(
    const std::filesystem::path &source_path,
    const std::filesystem::path &project_root) {
  auto root = canonical_or_empty(project_root);
  if (!root.empty()) {
    std::error_code error;
    if (std::filesystem::is_regular_file(root, error) && !error)
      root = root.parent_path();
    return root;
  }

  auto directory = source_directory(source_path);
  if (directory.empty())
    return {};
  auto fallback = directory;
  while (!directory.empty()) {
    std::error_code error;
    if (std::filesystem::exists(directory / ".git", error) && !error)
      return directory;
    if (std::filesystem::is_regular_file(directory / "compile_commands.json",
                                         error) && !error)
      return directory;
    const auto parent = directory.parent_path();
    if (parent == directory)
      break;
    directory = parent;
  }
  return fallback;
}

bool valid_uuid_text(std::string_view text,
                     std::array<std::uint8_t, 16> &id) noexcept {
  if (text.size() != 32)
    return false;
  const auto nibble = [](char value) -> int {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < id.size(); ++i) {
    const int high = nibble(text[i * 2]);
    const int low = nibble(text[i * 2 + 1]);
    if (high < 0 || low < 0)
      return false;
    id[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return std::any_of(id.begin(), id.end(), [](std::uint8_t value) {
           return value != 0;
         }) &&
         (id[6] & 0xf0) == 0x40 && (id[8] & 0xc0) == 0x80;
}

bool read_manifest(const std::filesystem::path &path,
                   std::array<std::uint8_t, 16> &id,
                   std::string &error) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) {
    error = "package lineage metadata cannot be read";
    return false;
  }
  struct stat info {};
  if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
      info.st_uid != ::geteuid() || (info.st_mode & 0077) != 0 ||
      info.st_size < 0 || info.st_size > 128) {
    ::close(fd);
    error = "package lineage metadata has unsafe ownership or permissions";
    return false;
  }
  std::string contents;
  contents.reserve(static_cast<std::size_t>(info.st_size));
  std::array<char, 129> buffer{};
  while (contents.size() < buffer.size()) {
    const auto count = ::read(fd, buffer.data(), buffer.size());
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0) {
      ::close(fd);
      error = "package lineage metadata cannot be read";
      return false;
    }
    if (count == 0)
      break;
    contents.append(buffer.data(), static_cast<std::size_t>(count));
  }
  ::close(fd);
  if (contents.size() > 128 || !::tracer::compat::starts_with(contents, kManifestHeader)) {
    error = "package lineage metadata is malformed";
    return false;
  }
  auto uuid = std::string_view(contents).substr(kManifestHeader.size());
  if (!uuid.empty() && uuid.back() == '\n')
    uuid.remove_suffix(1);
  if (!valid_uuid_text(uuid, id)) {
    error = "package lineage UUID is malformed";
    return false;
  }
  return true;
}

void random_uuid(std::array<std::uint8_t, 16> &id) {
  std::random_device random;
  for (auto &byte : id)
    byte = static_cast<std::uint8_t>(random());
  id[6] = static_cast<std::uint8_t>((id[6] & 0x0f) | 0x40);
  id[8] = static_cast<std::uint8_t>((id[8] & 0x3f) | 0x80);
}

std::string uuid_text(const std::array<std::uint8_t, 16> &id) {
  constexpr char digits[] = "0123456789abcdef";
  std::string text;
  text.reserve(32);
  for (const auto byte : id) {
    text.push_back(digits[byte >> 4]);
    text.push_back(digits[byte & 15]);
  }
  return text;
}

bool write_all(int fd, std::string_view bytes) noexcept {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto result = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (result < 0 && errno == EINTR)
      continue;
    if (result <= 0)
      return false;
    offset += static_cast<std::size_t>(result);
  }
  return true;
}

PackageLineage unverified(std::string_view reason,
                          std::string package_root = {}) {
  PackageLineage result;
  random_uuid(result.id);
  result.package_root = std::move(package_root);
  result.diagnostic.assign(reason.substr(0, 240));
  return result;
}

PackageLineage load_or_create(const std::filesystem::path &root,
                              const std::filesystem::path &manifest,
                              bool create) {
  std::error_code error;
  const auto metadata_directory = manifest.parent_path();
  if (create) {
    std::filesystem::create_directories(metadata_directory, error);
    if (error)
      return unverified("cannot create package lineage metadata", root.string());
    struct stat directory_info {};
    if (::lstat(metadata_directory.c_str(), &directory_info) != 0 ||
        !S_ISDIR(directory_info.st_mode) ||
        directory_info.st_uid != ::geteuid() ||
        ::chmod(metadata_directory.c_str(), S_IRWXU) != 0)
      return unverified("package lineage directory ownership is unsafe",
                         root.string());
  }

  const auto lock_path = metadata_directory / "source-lineage.lock";
  const int lock_fd = ::open(lock_path.c_str(),
                             (create ? O_RDWR | O_CREAT | O_CLOEXEC
                                     : O_RDONLY | O_CLOEXEC) |
                                 O_NOFOLLOW,
                             S_IRUSR | S_IWUSR);
  struct stat lock_info {};
  const bool lock_valid = lock_fd >= 0 && ::fstat(lock_fd, &lock_info) == 0 &&
                          S_ISREG(lock_info.st_mode) &&
                          lock_info.st_uid == ::geteuid() &&
                          (lock_info.st_mode & 0077) == 0;
  if (!lock_valid ||
      ::flock(lock_fd, create ? LOCK_EX : LOCK_SH) != 0) {
    if (lock_fd >= 0)
      ::close(lock_fd);
    return unverified("cannot lock package lineage metadata", root.string());
  }

  PackageLineage result;
  result.package_root = root.string();
  if (path_exists(manifest)) {
    result.retained = read_manifest(manifest, result.id, result.diagnostic);
    if (!result.retained) {
      result = unverified(result.diagnostic, root.string());
    }
  } else if (!create) {
    result.diagnostic = "package lineage metadata is not retained";
  } else {
    random_uuid(result.id);
    const auto temporary = std::filesystem::path(
        manifest.string() + ".tmp-" + std::to_string(::getpid()) + "-" +
        std::to_string(next_temp_file.fetch_add(1, std::memory_order_relaxed)));
    const int manifest_fd = ::open(temporary.c_str(),
                                   O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                                   S_IRUSR | S_IWUSR);
    const bool written = manifest_fd >= 0 &&
        write_all(manifest_fd, kManifestHeader) &&
        write_all(manifest_fd, uuid_text(result.id)) &&
        write_all(manifest_fd, "\n") && ::fsync(manifest_fd) == 0;
    if (manifest_fd >= 0)
      ::close(manifest_fd);
    if (!written || ::link(temporary.c_str(), manifest.c_str()) != 0) {
      (void)::unlink(temporary.c_str());
      result = unverified("cannot persist package lineage metadata",
                          root.string());
    } else {
      (void)::unlink(temporary.c_str());
      const int directory_fd = ::open(metadata_directory.c_str(),
                                       O_RDONLY | O_DIRECTORY | O_CLOEXEC);
      const bool directory_flushed =
          directory_fd >= 0 && ::fsync(directory_fd) == 0;
      if (directory_fd >= 0)
        ::close(directory_fd);
      if (!directory_flushed) {
        (void)::unlink(manifest.c_str());
        result = unverified("cannot durably flush package lineage metadata",
                            root.string());
      } else {
        result.retained = true;
      }
    }
  }
  (void)::flock(lock_fd, LOCK_UN);
  ::close(lock_fd);
  return result;
}

} // namespace

PackageLineage PackageCatalog::admit_source(
    const std::filesystem::path &source_path,
    const std::filesystem::path &project_root) {
  const auto source = canonical_or_empty(source_path);
  if (source.empty())
    return unverified("source package path is missing or unverifiable");

  auto existing = find_existing_manifest(source);
  if (!existing.manifest.empty())
    return load_or_create(existing.root, existing.manifest, true);

  auto root = discover_root(source, project_root);
  if (root.empty())
    return unverified("source package root is missing or unverifiable");
  return load_or_create(root, manifest_for_root(root), true);
}

PackageLineage PackageCatalog::resolve_source(
    const std::filesystem::path &source_path) {
  const auto source = canonical_or_empty(source_path);
  if (source.empty())
    return unverified("source package path is missing or unverifiable");
  const auto existing = find_existing_manifest(source);
  if (existing.manifest.empty())
    return unverified("source package lineage is not retained");
  return load_or_create(existing.root, existing.manifest, false);
}

PackageLineage PackageCatalog::distinct_unverified(std::string_view reason) {
  return unverified(reason);
}

} // namespace tracer::context_package

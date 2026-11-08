#include "mapping_cache.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace tracer_repl {
namespace {
const std::string prefix = "// tracer-repl-mapping-v1 ";
const std::string dependency = "// tracer-repl-dependency ";
std::string read_header(const std::filesystem::path &path) {
  if (std::filesystem::file_size(path) > 4 * 1024 * 1024)
    throw std::runtime_error("generated header exceeds 4 MiB");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read generated header: " + path.string());
  return {std::istreambuf_iterator<char>(input), {}};
}
std::string hex(const std::array<std::uint8_t, 32> &digest) {
  std::string result;
  for (auto byte : digest) {
    result += "0123456789abcdef"[byte >> 4];
    result += "0123456789abcdef"[byte & 15];
  }
  return result;
}
} // namespace
std::string mapping_digest(const std::string &text) {
  llvm::SHA256 hash;
  hash.update(text);
  return hex(hash.final());
}
std::string mapping_file_digest(const std::filesystem::path &path) {
  if (std::filesystem::file_size(path) > 128 * 1024 * 1024)
    throw std::runtime_error("mapping dependency exceeds 128 MiB: " +
                             path.string());
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("cannot read mapping dependency: " +
                             path.string());
  llvm::SHA256 hash;
  std::array<char, 8192> bytes;
  while (file) {
    file.read(bytes.data(), bytes.size());
    hash.update(
        llvm::StringRef(bytes.data(), static_cast<std::size_t>(file.gcount())));
  }
  if (!file.eof())
    throw std::runtime_error("cannot hash mapping dependency: " +
                             path.string());
  return hex(hash.final());
}
std::filesystem::path mapping_cache_directory() {
  if (const auto *cache = std::getenv("XDG_CACHE_HOME");
      cache && *cache && std::filesystem::path(cache).is_absolute())
    return std::filesystem::path(cache) / "clang-tracer/repl/mappings";
  if (const auto *home = std::getenv("HOME");
      home && *home && std::filesystem::path(home).is_absolute())
    return std::filesystem::path(home) / ".cache/clang-tracer/repl/mappings";
  return std::filesystem::temp_directory_path() /
         ("clang-tracer-repl-" + std::to_string(::getuid())) / "mappings";
}
std::string mapping_search_root(const std::filesystem::path &path) {
  if (std::filesystem::is_directory(path))
    return "directory:" + std::filesystem::weakly_canonical(path).string();
  if (std::filesystem::is_regular_file(path))
    return "file:" + std::filesystem::weakly_canonical(path).string() + ":" +
           mapping_file_digest(path);
  return std::filesystem::exists(path) ? "other" : "missing";
}
bool valid_mapping_header(const std::filesystem::path &path,
                          const std::string &key) {
  try {
    const auto text = read_header(path);
    const auto line = text.find('\n');
    if (line == std::string::npos || text.substr(0, prefix.size()) != prefix)
      return false;
    std::istringstream metadata(
        text.substr(prefix.size(), line - prefix.size()));
    std::string actual_key, checksum;
    if (!(metadata >> actual_key >> checksum) || actual_key != key ||
        mapping_digest(text.substr(line + 1)) != checksum)
      return false;
    std::istringstream lines(text.substr(line + 1));
    bool found = false;
    for (std::string entry; std::getline(lines, entry);) {
      if (entry == "// tracer-repl-no-reuse")
        return false;
      const std::string root = "// tracer-repl-search-root ";
      if (entry.rfind(root, 0) == 0) {
        std::istringstream record(entry.substr(root.size()));
        std::string path, expected;
        if (!(record >> std::quoted(path) >> std::quoted(expected)) ||
            mapping_search_root(path) != expected)
          return false;
        continue;
      }
      const std::string lookup = "// tracer-repl-lookup ";
      if (entry.rfind(lookup, 0) == 0) {
        std::istringstream record(entry.substr(lookup.size()));
        std::string filename, expected;
        if (!(record >> std::quoted(filename) >> std::quoted(expected)))
          return false;
        const auto actual =
            std::filesystem::is_regular_file(filename)
                ? std::filesystem::weakly_canonical(filename).string()
                : "";
        if (actual != expected)
          return false;
        continue;
      }
      if (entry.rfind(dependency, 0))
        continue;
      std::istringstream record(entry.substr(dependency.size()));
      std::string expected, filename;
      if (!(record >> expected >> std::quoted(filename)) ||
          mapping_file_digest(filename) != expected)
        return false;
      found = true;
    }
    return found;
  } catch (const std::exception &) {
    return false;
  }
}
void save_mapping_header(const std::filesystem::path &requested,
                         const std::string &key, const std::string &body) {
  const auto path = std::filesystem::absolute(requested).lexically_normal();
  std::istringstream records(body);
  for (std::string line; std::getline(records, line);) {
    if (line.rfind(dependency, 0))
      continue;
    std::istringstream record(line.substr(dependency.size()));
    std::string digest, file;
    if (!(record >> digest >> std::quoted(file)))
      throw std::runtime_error("invalid dependency manifest");
    if (std::filesystem::weakly_canonical(file) ==
        std::filesystem::weakly_canonical(path))
      throw std::runtime_error(
          "generated output would overwrite an application dependency");
  }
  const auto content = prefix + key + " " + mapping_digest(body) + "\n" + body;
  if (content.size() > 4 * 1024 * 1024)
    throw std::runtime_error("generated header exceeds 4 MiB");
  if (std::filesystem::exists(path)) {
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path)))
      throw std::runtime_error("generated output must not be a symlink");
    const auto previous = read_header(path);
    if (previous == content)
      return;
    if (previous.rfind(prefix, 0))
      throw std::runtime_error(
          "output exists and is not a generated mapping header: " +
          path.string());
  }
  std::filesystem::create_directories(path.parent_path());
  auto temporary = (path.parent_path() / ".tracer-mapping.XXXXXX").string();
  const int fd = ::mkstemp(temporary.data());
  if (fd < 0)
    throw std::runtime_error("cannot create generated header");
  struct Pending {
    int fd;
    std::string path;
    ~Pending() {
      if (fd >= 0)
        ::close(fd);
      ::unlink(path.c_str());
    }
  } pending{fd, temporary};
  for (std::size_t offset = 0; offset < content.size();) {
    const auto count =
        ::write(fd, content.data() + offset, content.size() - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      throw std::runtime_error("cannot write generated header");
    offset += static_cast<std::size_t>(count);
  }
  if (::fsync(fd))
    throw std::runtime_error("cannot flush generated header");
  if (::close(fd)) {
    pending.fd = -1;
    throw std::runtime_error("cannot close generated header");
  }
  pending.fd = -1;
  std::filesystem::rename(temporary, path);
}
std::string mapping_header_summary(const std::filesystem::path &path) {
  std::istringstream lines(read_header(path));
  for (std::string line; std::getline(lines, line);)
    if (line.rfind("// Generated EventRecord mappings:", 0) == 0)
      return line.substr(3);
  throw std::runtime_error("generated header has no mapping summary");
}
} // namespace tracer_repl

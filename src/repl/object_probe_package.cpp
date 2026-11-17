#include "tracer/compat.hpp"
#include "object_probe_package.hpp"
#include "tracer/protocol.hpp"

#include <cerrno>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

namespace tracer_repl {
namespace {
constexpr std::string_view kMagic = "CTKOPKG2";
constexpr std::size_t kDigestSize = 32;
constexpr std::size_t kMaximumPackageSize = 64 * 1024 * 1024;
constexpr std::size_t kMaximumFields = 256;
constexpr std::size_t kMaximumWriters = 256;
constexpr std::size_t kMaximumArguments = 256;

std::string digest(std::string_view bytes) {
  llvm::SHA256 hash;
  hash.update(llvm::StringRef(bytes.data(), bytes.size()));
  const auto value = hash.final();
  return {reinterpret_cast<const char *>(value.data()), value.size()};
}

std::string package_bytes(const ObjectProbePackageInput &input,
                          const std::vector<ObjectProbePackage::Artifact> &artifacts) {
  clangtracer::v1::ObjectProbePackageData data;
  data.set_version(input.version);
  data.set_qualified_type(input.qualified_type);
  data.mutable_methods()->Assign(input.methods.begin(), input.methods.end());
  data.mutable_fields()->Assign(input.fields.begin(), input.fields.end());
  data.mutable_writer_selectors()->Assign(input.writer_selectors.begin(), input.writer_selectors.end());
  data.set_callback_source(input.callback_source);
  data.set_allow_dwarf_writer_fallback(input.allow_dwarf_writer_fallback);
  data.mutable_includes()->Assign(input.compile.includes.begin(), input.compile.includes.end());
  data.set_compilation_database(input.compile.compilation_database);
  data.set_translation_unit(input.compile.translation_unit);
  data.mutable_extra_arguments()->Assign(input.compile.extra_arguments.begin(), input.compile.extra_arguments.end());
  data.mutable_prepared_methods()->Assign(artifacts.begin(), artifacts.end());
  if (kMagic.size() + data.ByteSizeLong() + kDigestSize > kMaximumPackageSize)
    throw std::invalid_argument("object-probe package exceeds the total size limit");
  std::string bytes(kMagic);
  if (!data.AppendToString(&bytes))
    throw std::runtime_error("cannot serialize object-probe package");
  bytes += digest(bytes);
  return bytes;
}
} // namespace

void ObjectProbePackage::validate(const ObjectProbePackageInput &input,
                                  const std::vector<Artifact> *artifacts) {
  if (input.version != current_version)
    throw std::invalid_argument("unsupported object-probe package version");
  if (input.qualified_type.empty())
    throw std::invalid_argument("object-probe package requires a qualified type");
  if (input.fields.size() > kMaximumFields || input.writer_selectors.size() > kMaximumWriters)
    throw std::invalid_argument("object-probe package has too many field or writer selectors");
  if (input.fields.empty() != input.writer_selectors.empty() || input.fields.size() > 1)
    throw std::invalid_argument("select one field together with its writer scopes");
  if (input.methods.empty()) {
    if (!input.callback_source.empty())
      throw std::invalid_argument(
          "callback source without method selectors is unsupported until change callbacks are available");
    if (input.fields.empty() || input.writer_selectors.empty())
      throw std::invalid_argument("watch-only object-probe package requires a field and a writer selector");
  } else {
    if (input.callback_source.empty())
      throw std::invalid_argument("method object-probe package requires callback source");
    if (input.compile.includes.empty() || input.compile.compilation_database.empty() ||
        input.compile.translation_unit.empty())
      throw std::invalid_argument("method object-probe package requires include, compdb, and translation unit");
  }
  if (input.compile.includes.size() > kMaximumArguments)
    throw std::invalid_argument("object-probe package has too many includes");
  if (input.compile.extra_arguments.size() > kMaximumArguments)
    throw std::invalid_argument("object-probe package has too many compile arguments");

  const auto unique_nonempty = [](const std::vector<std::string> &values, const char *what) {
    std::set<std::string> seen;
    for (const auto &value : values)
      if (value.empty() || !seen.insert(value).second)
        throw std::invalid_argument(std::string("empty or duplicate ") + what + " selector");
  };
  unique_nonempty(input.methods, "method");
  unique_nonempty(input.fields, "field");
  unique_nonempty(input.writer_selectors, "writer");
  if (input.allow_dwarf_writer_fallback && !input.methods.empty())
    throw std::invalid_argument("DWARF writer fallback is only available for watch-only packages");
  if (artifacts && artifacts->size() != input.methods.size())
    throw std::invalid_argument("prepared method artifact count does not match method selectors");
}

ObjectProbePackage ObjectProbePackage::prepare(const ObjectProbePackageInput &input,
                                               const PrepareMethod &prepare_method) {
  validate(input);
  if (!prepare_method)
    throw std::invalid_argument("object-probe package requires a method preparer");
  ObjectProbePackage package;
  package.input_ = input;
  package.prepared_methods_.reserve(input.methods.size());
  for (const auto &method : input.methods)
    package.prepared_methods_.push_back(prepare_method(method, input.compile));
  validate(package.input_, &package.prepared_methods_);
  return package;
}

void ObjectProbePackage::save(const std::filesystem::path &path) const {
  validate(input_, &prepared_methods_);
  const auto bytes = package_bytes(input_, prepared_methods_);
  if (path.empty())
    throw std::invalid_argument("object-probe package output path is empty");
  auto temporary = path.string() + ".XXXXXX";
  const int fd = ::mkstemp(temporary.data());
  if (fd < 0)
    throw std::system_error(errno, std::generic_category(), "cannot create object-probe package");
  int failure = 0;
  std::string_view remaining(bytes);
  while (!remaining.empty()) {
    const auto count = ::write(fd, remaining.data(), remaining.size());
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0) {
      failure = count < 0 ? errno : EIO;
      break;
    }
    remaining.remove_prefix(static_cast<std::size_t>(count));
  }
  if (::close(fd) != 0 && !failure)
    failure = errno;
  if (!failure && ::link(temporary.c_str(), path.c_str()) != 0)
    failure = errno;
  ::unlink(temporary.c_str());
  if (failure == EEXIST)
    throw std::runtime_error("object-probe package already exists; select a new --output file");
  if (failure)
    throw std::system_error(failure, std::generic_category(), "cannot publish object-probe package");
}

ObjectProbePackage ObjectProbePackage::load(const std::filesystem::path &path) {
  const auto size = std::filesystem::file_size(path);
  if (size > kMaximumPackageSize || size <= kMagic.size() + kDigestSize)
    throw std::runtime_error("object-probe package has invalid size");
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("cannot open object-probe package: " + path.string());
  std::string bytes(static_cast<std::size_t>(size), '\0');
  file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!file || static_cast<std::size_t>(file.gcount()) != bytes.size())
    throw std::runtime_error("cannot read complete object-probe package: " + path.string());
  const std::string_view all(bytes);
  const auto payload = all.substr(0, all.size() - kDigestSize);
  const auto expected_digest = all.substr(all.size() - kDigestSize);
  if (digest(payload) != expected_digest)
    throw std::runtime_error("object-probe package integrity check failed");

  if (!::tracer::compat::starts_with(payload, kMagic))
    throw std::runtime_error("unsupported object-probe package format; prepare a new package");

  ObjectProbePackage package;
  const auto body = payload.substr(kMagic.size());
  clangtracer::v1::ObjectProbePackageData data;
  if (!data.ParseFromArray(body.data(), static_cast<int>(body.size())))
    throw std::runtime_error("invalid object-probe package payload");
  auto &input = package.input_;
  input.version = data.version();
  input.qualified_type = data.qualified_type();
  input.methods.assign(data.methods().begin(), data.methods().end());
  input.fields.assign(data.fields().begin(), data.fields().end());
  input.writer_selectors.assign(data.writer_selectors().begin(), data.writer_selectors().end());
  input.callback_source = data.callback_source();
  input.allow_dwarf_writer_fallback = data.allow_dwarf_writer_fallback();
  input.compile.includes.assign(data.includes().begin(), data.includes().end());
  input.compile.compilation_database = data.compilation_database();
  input.compile.translation_unit = data.translation_unit();
  input.compile.extra_arguments.assign(data.extra_arguments().begin(), data.extra_arguments().end());
  package.prepared_methods_.assign(data.prepared_methods().begin(), data.prepared_methods().end());
  validate(input, &package.prepared_methods_);
  return package;
}

} // namespace tracer_repl

#include "object_probe_package.hpp"
#include "tracer/protocol.hpp"

#include <array>
#include <cerrno>
#include <fstream>
#include <limits>
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
constexpr std::string_view kMagic = "CTKOPKG1";
constexpr std::size_t kDigestSize = 32;
constexpr std::size_t kMaximumPackageSize = 64 * 1024 * 1024;
constexpr std::size_t kMaximumStringSize = 8 * 1024 * 1024;
constexpr std::size_t kMaximumMethods = tracer::kMaxProbes;
constexpr std::size_t kMaximumFields = 256;
constexpr std::size_t kMaximumWriters = 256;
constexpr std::size_t kMaximumArguments = 256;

class Encoder {
public:
  void u32(std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
      bytes_.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void string(std::string_view value) {
    if (value.size() > kMaximumStringSize ||
        value.size() > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument("object-probe package field exceeds the size limit");
    u32(static_cast<std::uint32_t>(value.size()));
    if (!value.empty())
      bytes_.append(value.data(), value.size());
  }

  void strings(const std::vector<std::string> &values) {
    if (values.size() > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument("too many object-probe package entries");
    u32(static_cast<std::uint32_t>(values.size()));
    for (const auto &value : values)
      string(value);
  }

  void raw(std::string_view value) {
    if (!value.empty())
      bytes_.append(value.data(), value.size());
  }
  std::string take() && { return std::move(bytes_); }

private:
  std::string bytes_;
};

class Decoder {
public:
  explicit Decoder(std::string_view bytes) : bytes_(bytes) {}

  std::uint32_t u32() {
    require(4);
    std::uint32_t value = 0;
    for (unsigned i = 0; i != 4; ++i)
      value = (value << 8) | static_cast<unsigned char>(bytes_[offset_++]);
    return value;
  }

  std::string string() {
    const auto size = u32();
    if (size > kMaximumStringSize)
      throw std::runtime_error("object-probe package field exceeds the size limit");
    require(size);
    std::string value(bytes_.substr(offset_, size));
    offset_ += size;
    return value;
  }

  std::vector<std::string> strings(std::size_t maximum) {
    const auto count = u32();
    if (count > maximum)
      throw std::runtime_error("object-probe package contains too many selectors");
    std::vector<std::string> values;
    values.reserve(count);
    for (std::uint32_t i = 0; i != count; ++i)
      values.push_back(string());
    return values;
  }

  bool done() const noexcept { return offset_ == bytes_.size(); }

private:
  void require(std::size_t size) const {
    if (size > bytes_.size() - offset_)
      throw std::runtime_error("truncated object-probe package");
  }

  std::string_view bytes_;
  std::size_t offset_ = 0;
};

std::string digest(std::string_view bytes) {
  llvm::SHA256 hash;
  hash.update(llvm::StringRef(bytes.data(), bytes.size()));
  const auto value = hash.final();
  return {reinterpret_cast<const char *>(value.data()), value.size()};
}

void encode_compile(Encoder &encoder, const ObjectProbeCompileInputs &compile) {
  encoder.strings(compile.includes);
  encoder.string(compile.compilation_database);
  encoder.string(compile.translation_unit);
  encoder.strings(compile.extra_arguments);
}

ObjectProbeCompileInputs decode_compile(Decoder &decoder) {
  ObjectProbeCompileInputs compile;
  compile.includes = decoder.strings(kMaximumArguments);
  compile.compilation_database = decoder.string();
  compile.translation_unit = decoder.string();
  compile.extra_arguments = decoder.strings(kMaximumArguments);
  return compile;
}

std::string encode_input(const ObjectProbePackageInput &input) {
  Encoder encoder;
  encoder.u32(input.version);
  encoder.string(input.qualified_type);
  encoder.strings(input.methods);
  encoder.strings(input.fields);
  encoder.strings(input.writer_selectors);
  encoder.string(input.callback_source);
  encoder.u32(input.allow_dwarf_writer_fallback ? 1 : 0);
  encode_compile(encoder, input.compile);
  return std::move(encoder).take();
}

ObjectProbePackageInput decode_input(Decoder &decoder) {
  ObjectProbePackageInput input;
  input.version = decoder.u32();
  input.qualified_type = decoder.string();
  input.methods = decoder.strings(kMaximumMethods);
  input.fields = decoder.strings(kMaximumFields);
  input.writer_selectors = decoder.strings(kMaximumWriters);
  input.callback_source = decoder.string();
  const auto dwarf_fallback = decoder.u32();
  if (dwarf_fallback > 1)
    throw std::runtime_error("invalid DWARF writer fallback flag");
  input.allow_dwarf_writer_fallback = dwarf_fallback != 0;
  input.compile = decode_compile(decoder);
  return input;
}

void encode_artifact(Encoder &encoder, const ObjectProbePackage::Artifact &artifact) {
  std::string bytes;
  if (!artifact.SerializeToString(&bytes))
    throw std::runtime_error("cannot serialize prepared method artifact");
  encoder.string(bytes);
}

ObjectProbePackage::Artifact decode_artifact(Decoder &decoder) {
  const auto bytes = decoder.string();
  ObjectProbePackage::Artifact artifact;
  if (!artifact.ParseFromString(bytes))
    throw std::runtime_error("invalid prepared method artifact in object-probe package");
  return artifact;
}

std::string package_bytes(const ObjectProbePackageInput &input,
                          const std::vector<ObjectProbePackage::Artifact> &artifacts) {
  Encoder encoder;
  encoder.raw(kMagic);
  encoder.u32(ObjectProbePackage::current_version);
  const auto encoded_input = encode_input(input);
  encoder.string(encoded_input);
  if (artifacts.size() > kMaximumMethods)
    throw std::invalid_argument("object-probe package contains too many prepared methods");
  encoder.u32(static_cast<std::uint32_t>(artifacts.size()));
  for (const auto &artifact : artifacts)
    encode_artifact(encoder, artifact);
  auto bytes = std::move(encoder).take();
  if (bytes.size() + kDigestSize > kMaximumPackageSize)
    throw std::invalid_argument("object-probe package exceeds the total size limit");
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
  if (input.methods.size() > kMaximumMethods)
    throw std::invalid_argument("object-probe package exceeds the native method probe limit");
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
  if (!failure && ::rename(temporary.c_str(), path.c_str()) != 0)
    failure = errno;
  if (failure) {
    ::unlink(temporary.c_str());
    throw std::system_error(failure, std::generic_category(), "cannot publish object-probe package");
  }
}

ObjectProbePackage ObjectProbePackage::load(const std::filesystem::path &path) {
  const auto size = std::filesystem::file_size(path);
  if (size > kMaximumPackageSize || size < kMagic.size() + 4 + 4 + kDigestSize)
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

  if (payload.substr(0, kMagic.size()) != kMagic)
    throw std::runtime_error("invalid object-probe package magic");
  Decoder body(payload.substr(kMagic.size()));
  const auto format_version = body.u32();
  if (format_version != current_version)
    throw std::runtime_error("unsupported object-probe package format version");
  const auto input_bytes = body.string();
  Decoder input_decoder(input_bytes);
  ObjectProbePackage package;
  package.input_ = decode_input(input_decoder);
  if (!input_decoder.done())
    throw std::runtime_error("object-probe package input has trailing data");
  const auto count = body.u32();
  if (count > kMaximumMethods)
    throw std::runtime_error("object-probe package contains too many prepared methods");
  package.prepared_methods_.reserve(count);
  for (std::uint32_t i = 0; i != count; ++i)
    package.prepared_methods_.push_back(decode_artifact(body));
  if (!body.done())
    throw std::runtime_error("object-probe package has trailing data");
  validate(package.input_, &package.prepared_methods_);
  return package;
}

} // namespace tracer_repl

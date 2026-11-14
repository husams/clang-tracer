#include "server.hpp"
#include "prepared_contract.hpp"
#include "probe_extra_arguments.hpp"
#include "proc_identity.hpp"
#include "process_memory.hpp"
#include "object_snapshot.hpp"
#include "bounded_proto.hpp"
#include "value_snapshot_storage.hpp"
#include "object_watch_plan.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <grpcpp/grpcpp.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "context_collector.hpp"
#include "context_journal.hpp"
#include "context_package.hpp"
#include "debug_resolver.hpp"
#include "event_transport.hpp"
#include "header_context.hpp"
#include "injector.hpp"
#include "session.hpp"
#include "scalar_overrides.hpp"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"

namespace tracer {
namespace {
namespace api = clangtracer::v1;

constexpr std::size_t kSessionLimit = 64;
constexpr std::size_t kMaxCallbackSource = 64 * 1024;

void set_probe_extra_compile_arguments(
    api::ProbeSpec &spec, const google::protobuf::RepeatedPtrField<std::string> &options) {
  if (options.empty()) return;
  auto extra = normalize_extra_compile_arguments(
      {options.begin(), options.end()}, primary_probe_compile_arguments(spec),
      primary_probe_compile_directory(spec));
  for (auto &argument : extra)
    spec.add_extra_compile_arguments(std::move(argument));
  spec.set_extra_compile_arguments_checked(true);
}

template <class Headers, class Options>
grpc::Status validate_resolution_inputs(std::string_view selector,
                                        std::string_view module,
                                        std::string_view compdb,
                                        std::string_view translation_unit,
                                        bool typed_prepared,
                                        const Headers &headers,
                                        const Options &options) {
  const auto valid_text = [](std::string_view value, std::size_t limit,
                             bool allow_empty = false) {
    return (allow_empty || !value.empty()) && value.size() <= limit &&
           value.find('\0') == std::string_view::npos;
  };
  if (!valid_text(selector, 4096) || !valid_text(module, 4095, true) ||
      !valid_text(compdb, 4095, true) || !valid_text(translation_unit, 4095, true)) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "invalid selector, module, compilation database path, or translation unit"};
  }
  if (!translation_unit.empty() && compdb.empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "--translation-unit requires --compdb"};
  }
  if (!typed_prepared && !compdb.empty() && headers.empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "--compdb requires --include HEADER"};
  }
  if (typed_prepared && compdb.empty() && !headers.empty()) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "prepared injection requires --compdb to use --include"};
  }
  if (headers.size() > 16) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "at most 16 include headers are allowed"};
  }
  for (const auto &header : headers) {
    if (!valid_text(header, 4095) || header.front() != '/' ||
        header.find_first_of("\"\r\n") != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid optional include"};
    }
  }
  if (options.size() > 128) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "at most 128 compiler options are allowed"};
  }
  std::size_t option_bytes = 0;
  for (const auto &option : options) {
    option_bytes += option.size() + 1;
    if (!valid_text(option, 4096) || option_bytes > 65536) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "compiler options must be nonempty and total at most 64 KiB"};
    }
  }
  if (!safe_preflight_compiler_arguments(options.begin(), options.end())) {
    return {grpc::StatusCode::INVALID_ARGUMENT,
            "compiler options contain an unsupported preflight option"};
  }
  return grpc::Status::OK;
}

context_package::PackageLineage admit_probe_package(
    const clangtracer::v1::ProbeSpec &spec) {
  const std::filesystem::path source(spec.callback_package_path());
  if (source.empty() || spec.callback_package_path().front() != '/') {
    return context_package::PackageCatalog::distinct_unverified(
        "callback package source path is missing or not absolute");
  }

  std::ifstream input(source, std::ios::binary);
  if (!input) {
    return context_package::PackageCatalog::distinct_unverified(
        "callback package source cannot be verified");
  }
  const std::string contents{std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>()};
  if (input.bad() || contents != spec.callback_source()) {
    return context_package::PackageCatalog::distinct_unverified(
        "callback source differs from its package file; lineage isolated");
  }
  return context_package::PackageCatalog::admit_source(source);
}

volatile std::sig_atomic_t interrupted = 0;

void signal_handler(int) { interrupted = 1; }

class LiveContextCapture final {
public:
  bool accept(const clangtracer::context::v1::ExportOpen &,
              const clangtracer::context::v1::ContextEventEnvelope &envelope,
              std::shared_ptr<const context_engine::DecodedEvent> decoded) {
    if (!decoded)
      return false;
    const auto &event = envelope.event();
    std::cerr << "shared-context event " << event.name() << " states="
              << decoded->states.size() << '\n';
    for (const auto &state : decoded->states) {
      if (const auto *message = state.message())
        std::cerr << message->DebugString();
    }
    return true;
  }
};

class DurableStoreCatalog final {
public:
  DurableStoreCatalog(std::filesystem::path root, context_delivery::JournalLimits limits)
      : root_(std::move(root)), limits_(std::move(limits)) {}

  std::shared_ptr<context_delivery::ContextCollectorStore>
  acquire(std::string_view recording_id, std::string_view sink_id) {
    if (recording_id.empty() || sink_id.empty() || recording_id.size() > 1024 ||
        sink_id.size() > 1024)
      return {};
    std::string key;
    key.reserve(recording_id.size() + sink_id.size() + 24);
    key += std::to_string(recording_id.size());
    key.push_back(':');
    key.append(recording_id);
    key += std::to_string(sink_id.size());
    key.push_back(':');
    key.append(sink_id);

    std::lock_guard lock(mutex_);
    for (auto current = stores_.begin(); current != stores_.end();) {
      if (current->second.expired())
        current = stores_.erase(current);
      else
        ++current;
    }
    if (const auto found = stores_.find(key); found != stores_.end()) {
      if (auto existing = found->second.lock())
        return existing;
      stores_.erase(found);
    }
    if (stores_.size() >= kStoreLimit)
      return {};

    const auto directory = root_ / digest_directory_name(key);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
      return {};
    auto store = std::make_shared<context_delivery::ContextCollectorStore>(
        directory, limits_);
    stores_.emplace(std::move(key), store);
    return store;
  }

private:
  static constexpr std::size_t kStoreLimit = 64;

  static std::string digest_directory_name(std::string_view key) {
    llvm::SHA256 hash;
    hash.update(llvm::StringRef(key.data(), key.size()));
    const auto digest = hash.final();
    constexpr char hex[] = "0123456789abcdef";
    std::string name;
    name.reserve(digest.size() * 2);
    for (const auto byte : digest) {
      name.push_back(hex[byte >> 4]);
      name.push_back(hex[byte & 0x0f]);
    }
    return name;
  }

  const std::filesystem::path root_;
  const context_delivery::JournalLimits limits_;
  std::mutex mutex_;
  std::unordered_map<
      std::string,
      std::weak_ptr<context_delivery::ContextCollectorStore>> stores_;
};

std::uint64_t random_probe_seed() {
  std::uint64_t value = 0;
  const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    throw std::runtime_error("cannot create probe identifier sequence");
  const auto count = ::read(fd, &value, sizeof(value));
  ::close(fd);
  if (count != sizeof(value))
    throw std::runtime_error("cannot create probe identifier sequence");
  value &= 0x7fffffffffffffffULL;
  return value == 0 ? 1 : value;
}

std::string make_id() {
  std::array<unsigned char, 16> bytes{};
  const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    throw std::runtime_error("cannot create session identifier");
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0) {
      ::close(fd);
      throw std::runtime_error("cannot create session identifier");
    }
    offset += static_cast<std::size_t>(count);
  }
  ::close(fd);
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      result.push_back('-');
    result.push_back(hex[bytes[i] >> 4]);
    result.push_back(hex[bytes[i] & 15]);
  }
  return result;
}

class PrivateDirectory final {
public:
  PrivateDirectory() {
    std::array<char, 64> pattern{};
    std::strcpy(pattern.data(), "/tmp/clang-tracer-XXXXXX");
    char *created = ::mkdtemp(pattern.data());
    if (!created)
      throw std::runtime_error("cannot create private server directory");
    path_ = created;
    if (::chmod(path_.c_str(), 0700) != 0) {
      const auto reason = std::strerror(errno);
      ::rmdir(path_.c_str());
      throw std::runtime_error(
          std::string("cannot secure private server directory: ") + reason);
    }
  }
  ~PrivateDirectory() {
    if (path_.empty())
      return;
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::string &path() const { return path_; }

private:
  std::string path_;
};

class EndpointLock final {
public:
  explicit EndpointLock(const std::string &path) : socket_path_(path) {
    struct stat info{};
    if (::lstat(socket_path_.c_str(), &info) == 0) {
      throw std::runtime_error(
          "listen socket path already exists; refusing to replace it");
    }
    if (errno != ENOENT)
      throw std::runtime_error("cannot inspect listen socket path");
    lock_path_ = socket_path_ + ".lock";
    fd_ = ::open(lock_path_.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW,
                 0600);
    if (fd_ < 0) {
      const int error = errno;
      throw std::runtime_error("cannot open endpoint lock '" + lock_path_ +
                               "' (errno " + std::to_string(error) + "): " +
                               std::strerror(error));
    }
    if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
      const int error = errno;
      ::close(fd_);
      fd_ = -1;
      if (error == EWOULDBLOCK || error == EAGAIN) {
        throw std::runtime_error(
            "another clang-tracer server owns this endpoint");
      }
      throw std::runtime_error("cannot acquire endpoint lock '" + lock_path_ +
                               "' (errno " + std::to_string(error) + "): " +
                               std::strerror(error));
    }
    if (::lstat(socket_path_.c_str(), &info) == 0 || errno != ENOENT) {
      ::flock(fd_, LOCK_UN);
      ::close(fd_);
      fd_ = -1;
      throw std::runtime_error(
          "listen socket path appeared while acquiring endpoint lock");
    }
  }
  ~EndpointLock() {
    if (fd_ >= 0) {
      ::flock(fd_, LOCK_UN);
      ::close(fd_);
      ::unlink(lock_path_.c_str());
    }
    struct stat info{};
    if (::lstat(socket_path_.c_str(), &info) == 0 && S_ISSOCK(info.st_mode)) {
      ::unlink(socket_path_.c_str());
    }
  }

private:
  std::string socket_path_;
  std::string lock_path_;
  int fd_ = -1;
};

std::string unix_path_from_uri(const std::string &uri) {
  constexpr char prefix[] = "unix:///";
  if (uri.rfind(prefix, 0) != 0)
    return {};
  const std::string path = "/" + uri.substr(sizeof(prefix) - 1);
  if (path.size() >= sizeof(((sockaddr_un *)nullptr)->sun_path) ||
      path.find('\0') != std::string::npos) {
    throw std::invalid_argument("Unix socket path is too long");
  }
  return path;
}

bool valid_loopback_uri(const std::string &uri) {
  std::string port;
  if (uri.rfind("127.0.0.1:", 0) == 0)
    port = uri.substr(10);
  else if (uri.rfind("[::1]:", 0) == 0)
    port = uri.substr(6);
  else
    return false;
  if (port.empty())
    return false;
  try {
    std::size_t used = 0;
    const auto value = std::stoul(port, &used);
    return used == port.size() && value > 0 && value <= 65535;
  } catch (...) {
    return false;
  }
}

int hex_value(char value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}

std::string encode_symbol_page_token(
    const clangtracer::v1::ListSymbolsPageToken &token) {
  std::string serialized;
  if (!token.SerializeToString(&serialized))
    throw std::runtime_error("cannot encode symbol page token");
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(serialized.size() * 2);
  for (const unsigned char byte : serialized) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 0x0f]);
  }
  return result;
}

bool decode_symbol_page_token(
    const std::string &encoded,
    clangtracer::v1::ListSymbolsPageToken *token) {
  if (encoded.empty() || encoded.size() % 2 != 0)
    return false;
  std::string serialized;
  serialized.reserve(encoded.size() / 2);
  for (std::size_t i = 0; i < encoded.size(); i += 2) {
    const int high = hex_value(encoded[i]);
    const int low = hex_value(encoded[i + 1]);
    if (high < 0 || low < 0)
      return false;
    serialized.push_back(static_cast<char>((high << 4) | low));
  }
  return token->ParseFromString(serialized);
}

class EventHub final {
public:
  struct SnapshotWatch {
    struct Receiver {
      std::uint32_t active_thread = 0;
      std::size_t pending = 0;
      bool violated = false;
    };
    std::uint64_t id = 0;
    std::string type;
    std::vector<std::string> fields;
    std::vector<std::string> keys;
    std::uint64_t observations = 0;
    std::uint64_t gaps = 0;
    std::uint64_t dropped = 0;
    api::ObjectWatchRequest::Synchronization synchronization =
        api::ObjectWatchRequest::SYNCHRONIZATION_UNSPECIFIED;
    std::size_t max_instances = 0;
    std::uint64_t max_events = 0;
    std::chrono::seconds timeout{};
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
    std::map<std::uint64_t, Receiver> receivers;
    bool armed = false;
  };

  struct SnapshotBinding {
    std::shared_ptr<SnapshotWatch> watch;
    api::ObjectObservation origin;
    std::map<std::pair<std::uint32_t, std::uint64_t>, api::TraceEvent> pending;
    // The receiver owns this sample until it reacquires mutex_ after comparison.
    const api::TraceEvent *comparing = nullptr;
  };

private:
  struct ProbeIdentity {
    std::uint32_t pid = 0;
    std::string function_name;
    std::shared_ptr<const clangtracer::v1::PreparedInjector> replay_types;
  };

public:
  explicit EventHub(std::string path, EventHistoryLimits limits)
      : path_(std::move(path)), limits_(limits), instance_id_(make_id()) {
    if (path_.size() >= sizeof(((sockaddr_un *)nullptr)->sun_path)) {
      throw std::runtime_error("event socket path is too long");
    }
    fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd_ < 0)
      throw std::runtime_error("cannot create local event socket");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path_.c_str(), path_.size() + 1);
    if (::bind(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) !=
        0) {
      ::close(fd_);
      fd_ = -1;
      throw std::runtime_error(std::string("cannot bind local event socket: ") +
                               std::strerror(errno));
    }
    receiver_ = std::thread([this] { receive_loop(); });
  }
  ~EventHub() { stop(); }
  EventHub(const EventHub &) = delete;
  EventHub &operator=(const EventHub &) = delete;

  const std::string &path() const { return path_; }

  void set_probe(const std::string &session_id, std::uint64_t probe_id,
                 std::uint32_t pid, std::string function_name, bool active,
                 const clangtracer::v1::PreparedInjector *prepared = nullptr,
                 SnapshotBinding snapshot = {{}, {}, {}, nullptr}) {
    std::shared_ptr<const clangtracer::v1::PreparedInjector> replay_types;
    if (prepared && tracer::prepared::is_typed_version(prepared->format_version()) &&
        !prepared->adapter_declarations().empty() &&
        prepared->ByteSizeLong() <= tracer::prepared::max_replay_type_bytes) {
      auto types = std::make_shared<clangtracer::v1::PreparedInjector>();
      types->set_format_version(prepared->format_version());
      types->set_adapter_declarations(prepared->adapter_declarations());
      *types->mutable_adapter_standard_headers() = prepared->adapter_standard_headers();
      replay_types = std::move(types);
    }
    std::lock_guard lock(mutex_);
    const auto key = std::make_pair(session_id, probe_id);
    if (active) {
      if (snapshot.watch)
        snapshot_probes_.emplace(key, std::move(snapshot));
      const auto existing = active_probes_.find(key);
      if (existing != active_probes_.end()) {
        function_name = existing->second.function_name;
        replay_types = existing->second.replay_types;
      } else if (const auto known = known_probes_.find(key);
                 known != known_probes_.end()) {
        function_name = known->second.function_name;
        replay_types = known->second.replay_types;
      }
      ProbeIdentity identity{pid, std::move(function_name), std::move(replay_types)};
      active_probes_[key] = identity;
      constexpr std::size_t kKnownProbeLimit = 4096;
      bool retain_probe_identity = true;
      if (!::tracer::compat::contains(known_probes_, key) &&
          known_probes_.size() >= kKnownProbeLimit) {
        const auto stale =
            std::find_if(known_probes_.begin(), known_probes_.end(), [&](const auto& entry) {
              return !::tracer::compat::contains(active_probes_, entry.first);
            });
        if (stale != known_probes_.end())
          known_probes_.erase(stale);
        else
          retain_probe_identity = false;
      }
      if (retain_probe_identity)
        known_probes_[key] = std::move(identity);
      received_.try_emplace(key, 0);
    } else {
      flush_snapshot_probe_locked(key);
      active_probes_.erase(key);
      const auto found = received_.find(key);
      if (found != received_.end() && found->second == 0)
        received_.erase(found);
    }
  }

  void snapshot_status(const SnapshotWatch &watch, api::ObjectWatchStatus &status) {
    std::lock_guard lock(mutex_);
    status.set_observations(watch.observations);
    status.set_dropped(watch.dropped);
    status.set_coverage_gaps(watch.gaps);
    status.set_admitted_receivers(watch.receivers.size());
    status.set_capture_limit(snapshot_limit(watch));
  }

  void activate_snapshot_watch(SnapshotWatch &watch) {
    std::lock_guard lock(mutex_);
    watch.armed = true;
    if (watch.timeout.count())
      watch.deadline = std::chrono::steady_clock::now() + watch.timeout;
  }

  void publish_object_observation(clangtracer::v1::TraceEvent event) {
    std::lock_guard lock(mutex_);
    append_native_locked(std::move(event));
  }

  bool publish_context(
      const clangtracer::context::v1::ExportOpen &open,
      const clangtracer::context::v1::ContextEventEnvelope &envelope) {
    const auto drop = [this] {
      std::lock_guard lock(mutex_);
      ++dropped_;
      return true;
    };
    const auto &source = envelope.event();
    const auto parse_decimal = [](const std::string &text) -> std::uint64_t {
      if (text.empty() || text.size() > 20)
        return 0;
      std::uint64_t value = 0;
      for (const unsigned char c : text) {
        if (c < '0' || c > '9' || value > (UINT64_MAX - (c - '0')) / 10)
          return 0;
        value = value * 10 + (c - '0');
      }
      return value;
    };
    const auto parse_probe_identity = [&](const std::string &text) {
      if (text.size() == 16) {
        const bool decimal = std::all_of(text.begin(), text.end(),
            [](unsigned char c) { return c >= '0' && c <= '9'; });
        if (!decimal && std::all_of(text.begin() + 8, text.end(),
                [](unsigned char c) { return c == 0; })) {
          std::uint64_t value = 0;
          for (unsigned i = 0; i < 8; ++i)
            value |= static_cast<std::uint64_t>(
                static_cast<unsigned char>(text[i])) << (8 * i);
          return value;
        }
      }
      return parse_decimal(text);
    };
    const auto copy_value = [](const clangtracer::context::v1::TypedTraceValue &in,
                               clangtracer::v1::Value *out) {
      out->set_type(static_cast<clangtracer::v1::ValueType>(in.value_type()));
      if (in.has_snapshot()) *out->mutable_snapshot() = in.snapshot();
      switch (in.value_case()) {
      case clangtracer::context::v1::TypedTraceValue::kSignedValue:
        if (in.value_type() != clangtracer::v1::INT32 &&
            in.value_type() != clangtracer::v1::BOOL &&
            in.value_type() != clangtracer::v1::INT64)
          return false;
        out->set_signed_value(in.signed_value());
        return true;
      case clangtracer::context::v1::TypedTraceValue::kUnsignedValue:
        if (in.value_type() != clangtracer::v1::UINT64 &&
            in.value_type() != clangtracer::v1::POINTER &&
            in.value_type() != clangtracer::v1::OBJECT)
          return false;
        out->set_unsigned_value(in.unsigned_value());
        return true;
      case clangtracer::context::v1::TypedTraceValue::kRealValue:
        if (in.value_type() != clangtracer::v1::FLOAT32 &&
            in.value_type() != clangtracer::v1::FLOAT64)
          return false;
        out->set_real_value(in.real_value());
        return true;
      case clangtracer::context::v1::TypedTraceValue::VALUE_NOT_SET:
        return in.value_type() == clangtracer::v1::VOID ||
               in.value_type() == clangtracer::v1::VALUE_TYPE_UNSPECIFIED;
      }
      return false;
    };
    const auto copy_stack = [](const clangtracer::context::v1::ContextStackTrace &in,
                               clangtracer::v1::StackTrace *out) {
      if (!in.requested_depth() ||
          static_cast<unsigned>(in.frames_size()) > in.requested_depth() ||
          in.captured_depth() > in.requested_depth() ||
          in.hidden_frames() > in.captured_depth() ||
          (in.captured_depth() == 0
               ? in.application_only() || in.hidden_frames() != 0
               : static_cast<std::uint64_t>(in.frames_size()) + in.hidden_frames() !=
                     in.captured_depth()) ||
          (!in.application_only() && in.hidden_frames() != 0) ||
          in.detail().size() > 159 ||
          (in.status() != clangtracer::context::v1::ContextStackTrace::COMPLETE &&
           in.status() != clangtracer::context::v1::ContextStackTrace::TRUNCATED &&
           in.status() != clangtracer::context::v1::ContextStackTrace::PARTIAL))
        return false;
      const auto safe_text = [](const std::string &text) {
        return text.size() <= 256 &&
            std::none_of(text.begin(), text.end(), [](unsigned char c) {
              return c < 32 || c == 127;
            });
      };
      for (int i = 0; i < in.frames_size(); ++i) {
        const auto &frame = in.frames(i);
        const bool valid_kind = in.application_only()
            ? frame.application_frame() &&
                  (frame.kind() == clangtracer::context::v1::ContextStackFrame::TARGET_ENTRY ||
                   frame.kind() == clangtracer::context::v1::ContextStackFrame::NATIVE)
            : i == 0
                  ? frame.kind() == clangtracer::context::v1::ContextStackFrame::TARGET_ENTRY
                  : (frame.kind() == clangtracer::context::v1::ContextStackFrame::NATIVE ||
                     frame.kind() == clangtracer::context::v1::ContextStackFrame::JIT_BRIDGE);
        if (frame.pc() == 0 || !valid_kind || !safe_text(frame.function()) ||
            !safe_text(frame.module()) || !safe_text(frame.file()))
          return false;
      }
      out->set_status(static_cast<clangtracer::v1::StackTrace::Status>(in.status()));
      out->set_requested_depth(in.requested_depth());
      out->set_detail(in.detail());
      out->set_application_only(in.application_only());
      out->set_hidden_frames(in.hidden_frames());
      out->set_captured_depth(in.captured_depth());
      for (const auto &frame : in.frames()) {
        auto *copy = out->add_frames();
        copy->set_pc(frame.pc());
        copy->set_stack_pointer(frame.stack_pointer());
        copy->set_kind(static_cast<clangtracer::v1::StackFrame::Kind>(frame.kind()));
        copy->set_function(frame.function());
        copy->set_module(frame.module());
        copy->set_file(frame.file());
        copy->set_line(frame.line());
        copy->set_module_offset(frame.module_offset());
        copy->set_application_frame(frame.application_frame());
      }
      return true;
    };
    const auto session_id = open.producer_session_id();
    const auto probe_id = parse_probe_identity(source.probe_id());
    if (session_id.empty() || probe_id == 0 || !envelope.has_event() ||
        envelope.producer_session_id() != session_id ||
        envelope.epoch_id() != open.epoch_id())
      return drop();
    std::uint32_t verified_native_pid = 0;
    if (open.native_pid() != 0 || open.process_start_time() != 0) {
      if (open.native_pid() == 0 || open.process_start_time() == 0)
        return drop();
      try {
        if (Session::ProcessStartTime(open.native_pid()) !=
            open.process_start_time())
          return drop();
        verified_native_pid = open.native_pid();
      } catch (...) {
        return drop();
      }
    }

    clangtracer::v1::TraceEvent event;
    event.set_protocol_version(kProtocolVersion);
    event.set_session_id(session_id);
    event.set_probe_id(probe_id);
    event.set_probe_sequence(source.source_sequence());
    event.set_monotonic_time_ns(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
    if (source.invocation_id().size() == 16) {
      std::uint64_t call_id = 0;
      for (unsigned i = 0; i < 8; ++i)
        call_id |= static_cast<std::uint64_t>(
            static_cast<unsigned char>(source.invocation_id()[i])) << (8 * i);
      event.set_call_id(call_id);
    }
    event.set_phase(source.phase() == 1 ? clangtracer::v1::PRE
                    : source.phase() == 2 ? clangtracer::v1::POST
                                          : clangtracer::v1::TRACE_PHASE_UNSPECIFIED);
    for (const auto &argument : source.arguments()) {
      auto *value = event.add_arguments();
      if (!copy_value(argument.value(), value))
        return drop();
    }
    for (const auto &named : source.named_values()) {
      clangtracer::v1::NamedValue *value = named.kind() == 1
          ? event.add_named_arguments() : named.kind() == 2
          ? event.add_object_fields() : nullptr;
      if (!value)
        return drop();
      value->set_name(named.name());
      value->set_argument_index(named.argument_index());
      if (!copy_value(named.value(), value->mutable_value()))
        return drop();
    }
    if (source.has_result()) {
      if (!copy_value(source.result(), event.mutable_result()))
        return drop();
    }
    if (source.has_entry_stack() &&
        !copy_stack(source.entry_stack(), event.mutable_stack()))
      return drop();
    event.mutable_context()->CopyFrom(envelope);

    std::lock_guard lock(mutex_);
    const auto key = std::make_pair(session_id, probe_id);
    auto known = known_probes_.find(key);
    if (known == known_probes_.end()) {
      const auto active = active_probes_.find(key);
      if (active != active_probes_.end()) {
        event.set_pid(active->second.pid);
        event.set_function_name(active->second.function_name);
      } else if (verified_native_pid != 0) {
        // The collector may have restarted and lost its in-memory mapping.
        // The engine-supplied PID is accepted only while its process start
        // time still matches, so PID reuse cannot retarget the event.
        event.set_pid(verified_native_pid);
      } else {
        // An evicted, inactive producer identity cannot be associated safely.
        return true;
      }
    } else {
      event.set_pid(known->second.pid);
      event.set_function_name(known->second.function_name);
    }
    event.set_server_instance_id(instance_id_);
    const std::size_t charge = event.ByteSizeLong();
    if (charge + 64 > limits_.context_bytes) {
      ++dropped_;
      return true;
    }
    while (context_event_count_ >= limits_.context_events ||
           context_bytes_ + charge + 64 > limits_.context_bytes) {
      const auto oldest = std::find_if(history_.begin(), history_.end(),
          [](const auto &item) { return item.has_context(); });
      if (oldest == history_.end())
        break;
      context_bytes_ -= oldest->ByteSizeLong();
      --context_event_count_;
      evicted_through_ = std::max(evicted_through_, oldest->sequence());
      history_.erase(oldest);
      ++dropped_;
    }
    if (context_event_count_ >= limits_.context_events ||
        context_bytes_ + charge + 64 > limits_.context_bytes) {
      ++dropped_;
      return true;
    }
    event.set_sequence(next_sequence_++);
    if (history_.size() == limits_.events) {
      if (history_.front().has_context()) {
        context_bytes_ -= history_.front().ByteSizeLong();
        --context_event_count_;
      }
      evicted_through_ =
          std::max(evicted_through_, history_.front().sequence());
      history_.pop_front();
      ++dropped_;
    }
    event.set_server_dropped_events(dropped_);
    context_bytes_ += event.ByteSizeLong();
    ++context_event_count_;
    history_.push_back(std::move(event));
    changed_.notify_all();
    return true;
  }

  bool retire_probe(const std::string &session_id, std::uint64_t probe_id,
                    std::uint64_t expected, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    const auto key = std::make_pair(session_id, probe_id);
    // A successful send is not yet publication. Keep accepting this probe's
    // datagrams until the receiver has validated all the agent's final sends.
    if (!changed_.wait_for(lock, timeout, [&] {
          const auto found = received_.find(key);
          return (found == received_.end() ? 0 : found->second) >= expected;
        }))
      return false;
    flush_snapshot_probe_locked(key);
    active_probes_.erase(key);
    // Retain the count like Session's inactive ProbeRecord: detach and retries
    // may retire the same probe again after an earlier successful remove.
    return true;
  }

  bool stream(grpc::ServerContext *context,
              const clangtracer::v1::WatchEventsRequest *request,
              grpc::ServerWriter<clangtracer::v1::TraceEvent> *writer) {
    std::set<std::string> filter(request->session_ids().begin(),
                                 request->session_ids().end());
    std::set<std::string> name_filter(request->names().begin(),
                                      request->names().end());
    std::uint64_t cursor = request->after_sequence();
    for (;;) {
      std::vector<clangtracer::v1::TraceEvent> batch;
      std::size_t batch_bytes = 0;
      std::set<std::uint64_t> included_types;
      {
        std::unique_lock lock(mutex_);
        if (context->IsCancelled())
          return true;
        if (cursor != 0 && cursor < evicted_through_) {
          return false;
        }
        bool found = false;
        for (const auto &event : history_) {
          if (event.sequence() <= cursor)
            continue;
          const bool session_matches =
              filter.empty() || filter.count(event.session_id()) != 0;
          const bool pid_matches =
              request->pid() == 0 || event.pid() == request->pid();
          const bool name_matches = name_filter.empty() ||
              std::any_of(name_filter.begin(), name_filter.end(), [&](const auto &name) {
                return event.function_name() == name ||
                       (event.has_context() && event.context().has_event() &&
                        event.context().event().name() == name);
              });
          if (session_matches && pid_matches && name_matches) {
            const auto *types = request->include_replay_types()
                                    ? recording_types(event, included_types) : nullptr;
            const auto bytes = event.ByteSizeLong() +
                               (types ? types->ByteSizeLong() + 8 : 0);
            if (!batch.empty() && batch_bytes + bytes > 32 * 1024 * 1024)
              break;
            batch.push_back(event);
            if (types)
              *batch.back().mutable_replay_types() = *types;
            batch_bytes += bytes;
          }
          cursor = event.sequence();
          found = true;
          if (batch.size() >= 64)
            break;
        }
        if (!found || batch.empty()) {
          changed_.wait_for(lock, std::chrono::milliseconds(100));
          if (context->IsCancelled())
            return true;
          continue;
        }
      }
      for (const auto &event : batch) {
        if (context->IsCancelled())
          return true;
        if (!writer->Write(event))
          return true;
      }
    }
  }

  grpc::Status read(const clangtracer::v1::ReadEventsRequest &request,
                    clangtracer::v1::ReadEventsResponse &response) {
    std::lock_guard lock(mutex_);
    if ((!request.server_instance_id().empty() &&
         request.server_instance_id() != instance_id_) ||
        (request.through_sequence() != 0 &&
         request.server_instance_id().empty())) {
      return {grpc::StatusCode::FAILED_PRECONDITION,
              "snapshot server instance changed or missing"};
    }
    if (request.through_sequence() >= next_sequence_) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "snapshot watermark exceeds the current server sequence"};
    }
    const auto through = request.through_sequence() == 0
                             ? next_sequence_ - 1
                             : request.through_sequence();
    response.set_server_instance_id(instance_id_);
    response.set_through_sequence(through);
    response.set_server_dropped_events(dropped_);
    const auto &filter = request.filter();
    const auto limit = request.limit() == 0 ? 256 : request.limit();
    std::size_t bytes = 0;
    std::set<std::uint64_t> included_types;
    for (const auto &event : history_) {
      if (event.sequence() > through) {
        break;
      }
      if (event.sequence() <= filter.after_sequence() ||
          (filter.pid() != 0 && event.pid() != filter.pid()) ||
          (!filter.session_ids().empty() &&
           std::find(filter.session_ids().begin(), filter.session_ids().end(),
                     event.session_id()) == filter.session_ids().end()) ||
          (!filter.names().empty() &&
           std::none_of(filter.names().begin(), filter.names().end(),
                        [&](const auto &name) {
                          return event.function_name() == name ||
                                 (event.has_context() &&
                                  event.context().has_event() &&
                                  event.context().event().name() == name);
                        }))) {
        continue;
      }
      const auto *types = filter.include_replay_types()
                              ? recording_types(event, included_types) : nullptr;
      const auto charge = event.ByteSizeLong() + 16 +
                          (types ? types->ByteSizeLong() + 8 : 0);
      if (charge > 31 * 1024 * 1024) {
        return {grpc::StatusCode::RESOURCE_EXHAUSTED,
                "event exceeds drain response budget"};
      }
      if (bytes + charge > 31 * 1024 * 1024) {
        break;
      }
      auto *copy = response.add_events();
      copy->CopyFrom(event);
      if (types)
        *copy->mutable_replay_types() = *types;
      bytes += charge;
      if (response.events_size() >= static_cast<int>(limit)) {
        break;
      }
    }
    return grpc::Status::OK;
  }

  grpc::Status
  acknowledge(const clangtracer::v1::AcknowledgeEventsRequest &request,
              clangtracer::v1::AcknowledgeEventsResponse &response) {
    std::lock_guard lock(mutex_);
    if (request.server_instance_id() != instance_id_) {
      return {grpc::StatusCode::FAILED_PRECONDITION,
              "acknowledgement belongs to another server instance"};
    }
    const std::set<std::uint64_t> sequences(request.sequences().begin(),
                                            request.sequences().end());
    for (auto it = history_.begin(); it != history_.end();) {
      if (sequences.count(it->sequence()) == 0) {
        ++it;
        continue;
      }
      if (it->has_context()) {
        context_bytes_ -= it->ByteSizeLong();
        --context_event_count_;
      }
      evicted_through_ = std::max(evicted_through_, it->sequence());
      it = history_.erase(it);
      response.set_removed(response.removed() + 1);
    }
    changed_.notify_all();
    return grpc::Status::OK;
  }

  void stop() noexcept {
    stopping_.store(true);
    changed_.notify_all();
    if (receiver_.joinable())
      receiver_.join();
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
    if (!path_.empty())
      ::unlink(path_.c_str());
  }

private:
  std::string path_;
  EventHistoryLimits limits_;
  std::string instance_id_;
  std::uint64_t evicted_through_ = 0;
  int fd_ = -1;
  std::thread receiver_;
  std::atomic<bool> stopping_{false};
  std::mutex mutex_;
  std::condition_variable changed_;
  std::map<std::pair<std::string, std::uint64_t>, ProbeIdentity> active_probes_;
  std::map<std::pair<std::string, std::uint64_t>, ProbeIdentity> known_probes_;
  std::map<std::pair<std::string, std::uint64_t>, std::uint64_t> received_;
  std::map<std::pair<std::string, std::uint64_t>, SnapshotBinding> snapshot_probes_;
  std::size_t pending_snapshots_ = 0;
  std::size_t pending_snapshot_bytes_ = 0;
  std::deque<clangtracer::v1::TraceEvent> history_;
  std::size_t context_bytes_ = 0;
  std::size_t context_event_count_ = 0;
  std::uint64_t next_sequence_ = 1;
  std::uint64_t dropped_ = 0;

  static const char *snapshot_limit(const SnapshotWatch &watch) {
    if (watch.max_events && watch.observations >= watch.max_events)
      return "max-events";
    if (std::chrono::steady_clock::now() >= watch.deadline)
      return "timeout";
    return "";
  }

  void publish_snapshot_locked(const SnapshotBinding &binding, const api::TraceEvent &anchor,
                               const tracer_repl::SnapshotChange &change) {
    if (*snapshot_limit(*binding.watch))
      return;
    api::TraceEvent event;
    event.set_protocol_version(anchor.protocol_version());
    event.set_session_id(anchor.session_id());
    event.set_probe_id(binding.watch->id);
    event.set_pid(anchor.pid());
    event.set_tid(anchor.tid());
    event.set_call_id(anchor.call_id());
    event.set_object_address(anchor.object_address());
    event.set_monotonic_time_ns(anchor.monotonic_time_ns());
    event.set_agent_dropped_events(anchor.agent_dropped_events());
    event.set_function_name("object.trace");
    if (anchor.has_stack())
      *event.mutable_stack() = anchor.stack();
    auto &observation = *event.mutable_object_observation();
    observation = binding.origin;
    observation.set_boundary_probe_id(anchor.probe_id());
    observation.set_kind(change.gap ? api::ObjectObservation::COVERAGE_GAP
                                    : api::ObjectObservation::SNAPSHOT_DELTA);
    observation.set_concrete_type(binding.watch->type);
    observation.set_field(change.path);
    observation.set_diagnostic(change.detail);
    observation.set_synchronization(binding.watch->synchronization);
    if (change.before)
      tracer_repl::encode_observation_value(*change.before, *observation.mutable_snapshot_before());
    if (change.after)
      tracer_repl::encode_observation_value(*change.after, *observation.mutable_snapshot_after());
    ++binding.watch->observations;
    binding.watch->gaps += change.gap;
    append_native_locked(std::move(event));
  }

  void forget_snapshot_locked(SnapshotWatch &watch, const api::TraceEvent &event) {
    --pending_snapshots_;
    pending_snapshot_bytes_ -= event.ByteSizeLong();
    const auto receiver = watch.receivers.find(event.object_address());
    if (!--receiver->second.pending)
      watch.receivers.erase(receiver);
  }

  void flush_snapshot_probe_locked(const std::pair<std::string, std::uint64_t> &key) {
    const auto found = snapshot_probes_.find(key);
    if (found == snapshot_probes_.end())
      return;
    for (const auto &entry : found->second.pending) {
      const auto &event = entry.second;
      forget_snapshot_locked(*found->second.watch, event);
      publish_snapshot_locked(found->second, event, {"receiver", "method return unavailable at retirement", true});
    }
    if (found->second.comparing)
      publish_snapshot_locked(found->second, *found->second.comparing,
          {"receiver", "probe retired before snapshot comparison completed", true});
    snapshot_probes_.erase(found);
  }

  void sample_snapshot(std::unique_lock<std::mutex> &lock, SnapshotBinding &binding, api::TraceEvent event) {
    auto &watch = *binding.watch;
    if (!watch.armed)
      return;
    const auto key = std::make_pair(event.tid(), event.call_id());
    const auto existing = binding.pending.find(key);
    if (event.phase() == api::PRE) {
      if (*snapshot_limit(watch))
        return;
      if (existing != binding.pending.end()) {
        publish_snapshot_locked(binding, event, {"receiver", "duplicate method entry", true});
        forget_snapshot_locked(watch, existing->second);
        binding.pending.erase(existing);
        return;
      }
      const auto bytes = event.ByteSizeLong();
      if (pending_snapshots_ >= limits_.events ||
          bytes > limits_.context_bytes - pending_snapshot_bytes_) {
        ++watch.dropped;
        publish_snapshot_locked(binding, event, {"receiver", "method entry sample buffer exhausted", true});
        return;
      }
      if (!event.object_address()) {
        publish_snapshot_locked(binding, event, {"receiver", "method entry receiver unavailable", true});
        return;
      }
      auto found = watch.receivers.find(event.object_address());
      if (found == watch.receivers.end()) {
        if (watch.receivers.size() >= watch.max_instances) {
          publish_snapshot_locked(binding, event, {"receiver", "receiver admission limit reached", true});
          return;
        }
        found = watch.receivers.emplace(event.object_address(), SnapshotWatch::Receiver{event.tid()}).first;
      }
      auto &receiver = found->second;
      if (receiver.active_thread && receiver.active_thread != event.tid())
        receiver.violated = true;
      if (receiver.violated) {
        publish_snapshot_locked(binding, event, {"receiver", "declared snapshot synchronization was violated", true});
        return;
      }
      receiver.active_thread = event.tid();
      ++receiver.pending;
      pending_snapshot_bytes_ += bytes;
      ++pending_snapshots_;
      binding.pending.emplace(key, std::move(event));
      return;
    }
    if (existing == binding.pending.end()) {
      publish_snapshot_locked(binding, event, {"receiver", "method entry sample unavailable", true});
      return;
    }
    auto before = std::move(existing->second);
    binding.pending.erase(existing);
    const bool overlap = watch.receivers.at(before.object_address()).violated;
    forget_snapshot_locked(watch, before);
    if (overlap) {
      publish_snapshot_locked(binding, before, {"receiver", "overlapping observation has no stable receiver", true});
    } else if (!event.receiver_sampled_at_return() || !event.has_receiver_snapshot() ||
        !before.has_receiver_snapshot() || !event.object_address() ||
        before.object_address() != event.object_address()) {
      publish_snapshot_locked(binding, before, {"receiver", "matching receiver samples unavailable", true});
    } else {
      SnapshotBinding publication{binding.watch, binding.origin, {}, nullptr};
      binding.comparing = &before;
      lock.unlock();
      std::vector<tracer_repl::SnapshotChange> changes;
      bool failed = false;
      try {
        changes = tracer_repl::compare_snapshots(before.receiver_snapshot(),
            event.receiver_snapshot(), publication.watch->keys, publication.watch->fields);
      } catch (...) {
        failed = true;
      }
      lock.lock();
      const auto registered = snapshot_probes_.find({before.session_id(), before.probe_id()});
      if (registered == snapshot_probes_.end())
        return;
      registered->second.comparing = nullptr;
      if (failed) {
        publish_snapshot_locked(publication, before, {"receiver", "snapshot comparison failed", true});
        return;
      }
      for (const auto &change : changes)
        publish_snapshot_locked(publication, before, change);
    }
  }

  void append_native_locked(clangtracer::v1::TraceEvent event) {
    event.set_sequence(next_sequence_++);
    event.set_server_instance_id(instance_id_);
    if (history_.size() == limits_.events) {
      if (history_.front().has_context()) {
        context_bytes_ -= history_.front().ByteSizeLong();
        --context_event_count_;
      }
      evicted_through_ = std::max(evicted_through_, history_.front().sequence());
      history_.pop_front();
      ++dropped_;
    }
    event.set_server_dropped_events(dropped_);
    history_.push_back(std::move(event));
    changed_.notify_all();
  }

  const clangtracer::v1::PreparedInjector *
  recording_types(const clangtracer::v1::TraceEvent &event,
                  std::set<std::uint64_t> &included) const {
    if (event.context().event().states().empty() || !included.insert(event.probe_id()).second)
      return nullptr;
    const auto key = std::make_pair(event.session_id(), event.probe_id());
    const auto active = active_probes_.find(key);
    if (active != active_probes_.end())
      return active->second.replay_types.get();
    const auto known = known_probes_.find(key);
    return known == known_probes_.end() ? nullptr : known->second.replay_types.get();
  }

  void receive_loop() {
    std::array<char, EventTransport::kInlineEventBytes> bytes{};
    while (!stopping_.load()) {
      iovec data{bytes.data(), bytes.size()};
      alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
      msghdr message{};
      message.msg_iov = &data;
      message.msg_iovlen = 1;
      message.msg_control = control;
      message.msg_controllen = sizeof(control);
      const auto count = ::recvmsg(fd_, &message, MSG_CMSG_CLOEXEC);
      if (count < 0) {
        if (errno == EINTR)
          continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Wake as soon as a datagram arrives; a fixed 2 ms idle sleep lets
          // a hot producer fill its nonblocking socket before we resume.
          pollfd ready{fd_, POLLIN, 0};
          (void)::poll(&ready, 1, 10);
          continue;
        }
        if (stopping_.load())
          return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
      struct File {
        int fd = -1;
        ~File() { if (fd >= 0) ::close(fd); }
      } file;
      bool extra_descriptor = false;
      for (auto *header = CMSG_FIRSTHDR(&message); header;
           header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS)
          continue;
        const auto descriptors = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (std::size_t i = 0; i < descriptors; ++i) {
          int fd;
          std::memcpy(&fd, CMSG_DATA(header) + i * sizeof(fd), sizeof(fd));
          if (file.fd < 0) {
            file.fd = fd;
          } else {
            ::close(fd);
            extra_descriptor = true;
          }
        }
      }
      if (extra_descriptor || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)))
        continue;
      clangtracer::v1::TraceEvent event;
      bool parsed = false;
      if (file.fd >= 0) {
        struct stat info{};
        const int seals = ::fcntl(file.fd, EventTransport::kGetSeals);
        if (count != 1 || bytes[0] != EventTransport::kExternalEventMarker ||
            seals < 0 || (seals & EventTransport::kReadOnlySeals) != EventTransport::kReadOnlySeals ||
            ::fstat(file.fd, &info) != 0 || !S_ISREG(info.st_mode) ||
            info.st_size <= static_cast<off_t>(bytes.size()) ||
            info.st_size > std::numeric_limits<int>::max())
          continue;
        const auto size = static_cast<std::size_t>(info.st_size);
        auto *mapped = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, file.fd, 0);
        if (mapped == MAP_FAILED)
          continue;
        struct Mapping {
          void *address;
          std::size_t size;
          ~Mapping() { ::munmap(address, size); }
        } payload{mapped, size};
        try {
          parsed = event.ParseFromArray(payload.address, static_cast<int>(size));
        } catch (const std::bad_alloc &) {
          continue;
        }
      } else {
        parsed = event.ParseFromArray(bytes.data(), static_cast<int>(count));
      }
      if (!parsed ||
          event.protocol_version() != kProtocolVersion ||
          event.session_id().empty() || event.probe_id() == 0 ||
          event.pid() == 0 || event.tid() == 0 ||
          event.arguments_size() > static_cast<int>(kMaxArguments)) {
        continue;
      }
      bool valid_arguments = true;
      for (const auto &argument : event.arguments()) {
        const auto type = argument.type();
        const auto data = argument.data_case();
        const bool valid = ((type == clangtracer::v1::INT32 ||
                             type == clangtracer::v1::BOOL ||
                             type == clangtracer::v1::INT64) &&
                            data == clangtracer::v1::Value::kSignedValue) ||
                           ((type == clangtracer::v1::UINT64 ||
                             type == clangtracer::v1::POINTER ||
                             type == clangtracer::v1::OBJECT) &&
                            data == clangtracer::v1::Value::kUnsignedValue) ||
                           ((type == clangtracer::v1::FLOAT32 ||
                             type == clangtracer::v1::FLOAT64) &&
                            data == clangtracer::v1::Value::kRealValue);
        if (!valid) {
          valid_arguments = false;
          break;
        }
      }
      if (!valid_arguments)
        continue;
      const auto valid_result = [](const clangtracer::v1::Value &value) {
        const auto type = value.type();
        const auto data = value.data_case();
        if (type == clangtracer::v1::VOID)
          return data == clangtracer::v1::Value::DATA_NOT_SET;
        return ((type == clangtracer::v1::INT32 ||
                 type == clangtracer::v1::BOOL ||
                 type == clangtracer::v1::INT64) &&
                data == clangtracer::v1::Value::kSignedValue) ||
               ((type == clangtracer::v1::UINT64 ||
                 type == clangtracer::v1::POINTER ||
                 type == clangtracer::v1::OBJECT) &&
                data == clangtracer::v1::Value::kUnsignedValue) ||
               ((type == clangtracer::v1::FLOAT32 ||
                 type == clangtracer::v1::FLOAT64) &&
                data == clangtracer::v1::Value::kRealValue);
      };
      // Bound named snapshots and reject ambiguous or unsafe terminal names.
      bool valid_named =
          event.named_arguments_size() + event.object_fields_size() <= 16;
      std::set<std::string> argument_names, field_names;
      std::set<std::uint32_t> indices;
      const auto check_named = [&](const clangtracer::v1::NamedValue &item,
                                   std::set<std::string> &names) {
        const auto &name = item.name();
        if (name.empty() || name.size() > 63 || !names.insert(name).second ||
            !item.has_value() || item.value().type() == clangtracer::v1::VOID ||
            !valid_result(item.value()))
          return false;
        for (std::size_t i = 0; i < name.size(); ++i) {
          const char c = name[i];
          if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
                (i && name[i - 1] != '.' && c >= '0' && c <= '9') ||
                (c == '.' && i && name[i - 1] != '.' && i + 1 < name.size())))
            return false;
        }
        return true;
      };
      for (const auto &item : event.named_arguments()) {
        const auto index = item.argument_index();
        if (!check_named(item, argument_names) ||
            index >= static_cast<unsigned>(event.arguments_size()) ||
            !indices.insert(index).second ||
            item.value().SerializeAsString() !=
                event.arguments(index).SerializeAsString()) {
          valid_named = false;
          break;
        }
      }
      for (const auto &item : event.object_fields()) {
        if (!check_named(item, field_names) || item.argument_index() != 0) {
          valid_named = false;
          break;
        }
      }
      if (!valid_named)
        continue;
      if (event.has_stack()) {
        const auto &stack = event.stack();
        bool valid_stack = stack.requested_depth() >= 1 &&
            (stack.application_only() || stack.frames_size() >= 1) &&
            static_cast<unsigned>(stack.frames_size()) <= stack.requested_depth() &&
            stack.captured_depth() <= stack.requested_depth() &&
            stack.hidden_frames() <= stack.captured_depth() &&
            (stack.captured_depth() == 0 ? !stack.application_only() :
             static_cast<std::uint64_t>(stack.frames_size()) + stack.hidden_frames() == stack.captured_depth()) &&
            (stack.application_only() || stack.hidden_frames() == 0) &&
            stack.detail().size() <= 159 &&
            (stack.status() == clangtracer::v1::StackTrace::COMPLETE ||
             stack.status() == clangtracer::v1::StackTrace::TRUNCATED ||
             stack.status() == clangtracer::v1::StackTrace::PARTIAL);
        for (int i = 0; i < stack.frames_size(); ++i) {
          const auto &frame = stack.frames(i);
          const auto safe_text = [](const std::string &text) {
            return text.size() <= 256 &&
                std::none_of(text.begin(), text.end(), [](unsigned char c) {
                  return c < 32 || c == 127;
                });
          };
          valid_stack &= frame.pc() != 0 &&
              (stack.application_only() ? frame.application_frame() &&
                  (frame.kind() == clangtracer::v1::StackFrame::TARGET_ENTRY ||
                   frame.kind() == clangtracer::v1::StackFrame::NATIVE)
                : i == 0 ? frame.kind() == clangtracer::v1::StackFrame::TARGET_ENTRY
                      : (frame.kind() == clangtracer::v1::StackFrame::NATIVE ||
                         frame.kind() == clangtracer::v1::StackFrame::JIT_BRIDGE)) &&
              safe_text(frame.module()) && safe_text(frame.function()) && safe_text(frame.file());
        }
        if (!valid_stack) continue;
      }
      bool valid_phase = false;
      switch (event.phase()) {
      case clangtracer::v1::TRACE_PHASE_UNSPECIFIED:
        // Older version-2 agents published entry-only events without call IDs.
        valid_phase = event.call_id() == 0 && !event.has_result();
        break;
      case clangtracer::v1::EXCEPTION:
      case clangtracer::v1::PRE:
        valid_phase = event.call_id() != 0 && !event.has_result();
        break;
      case clangtracer::v1::POST:
        valid_phase = event.call_id() != 0 && event.has_result() &&
                      valid_result(event.result());
        break;
      default:
        break;
      }
      if (!valid_phase)
        continue;
      // Function names are assigned by the server from the registered ProbeSpec.
      if (!event.function_name().empty())
        continue;
      std::unique_lock lock(mutex_);
      const auto key = std::make_pair(event.session_id(), event.probe_id());
      const auto active = active_probes_.find(key);
      if (active == active_probes_.end() || active->second.pid != event.pid())
        continue;
      // Function identity comes from the registered probe, not the agent packet.
      event.clear_function_name();
      event.clear_replay_types();
      event.clear_object_observation();
      event.set_function_name(active->second.function_name);
      const auto snapshot = snapshot_probes_.find(key);
      if (snapshot == snapshot_probes_.end()) {
        ++received_[key];
        append_native_locked(std::move(event));
      } else {
        sample_snapshot(lock, snapshot->second, std::move(event));
        ++received_[key];
        changed_.notify_all();
      }
    }
  }
};

class TracerService final : public clangtracer::v1::Tracer::Service {
public:
  TracerService(std::string agent_path, std::string event_path,
                std::string source_directory, std::chrono::seconds timeout,
                std::string context_endpoint,
                std::string context_durable_directory,
                EventHistoryLimits limits, ContextTransportBudget context_budget,
                bool local_control)
      : agent_path_(std::move(agent_path)), event_path_(std::move(event_path)),
        source_directory_(std::move(source_directory)), timeout_(timeout),
        context_endpoint_(std::move(context_endpoint)),
        context_durable_directory_(std::move(context_durable_directory)),
        context_budget_(context_budget), local_control_(local_control),
        events_(event_path_, limits), next_probe_id_(random_probe_seed()) {}

  void set_context_endpoint(std::string endpoint) {
    std::lock_guard lock(mutex_);
    context_endpoint_ = std::move(endpoint);
    context_endpoint_ready_ = true;
    context_endpoint_cv_.notify_all();
  }

  bool publish_context(
      const clangtracer::context::v1::ExportOpen &open,
      const clangtracer::context::v1::ContextEventEnvelope &envelope) {
    return events_.publish_context(open, envelope);
  }

  grpc::Status Attach(grpc::ServerContext *,
                      const clangtracer::v1::AttachRequest *request,
                      clangtracer::v1::Session *response) override {
    {
      std::unique_lock lock(mutex_);
      context_endpoint_cv_.wait(lock, [&] { return context_endpoint_ready_; });
    }
    const auto pid = request->pid();
    if (pid == 0 || pid == static_cast<std::uint32_t>(::getpid())) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "pid must identify another process"};
    }
    std::uint64_t process_start = 0;
    try {
      process_start = Session::ProcessStartTime(pid);
    } catch (const std::exception &error) {
      return {grpc::StatusCode::NOT_FOUND, error.what()};
    }
    if (request->expected_executable().size() > 4096 ||
        request->expected_executable().find('\0') != std::string::npos)
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid expected executable path"};
    if (request->expected_process_start_time() != 0 &&
        request->expected_process_start_time() != process_start)
      return {grpc::StatusCode::FAILED_PRECONDITION,
              "target process start time changed; refresh process selection"};
    const auto id = make_id();
    std::shared_ptr<Session> prior_session;
    {
      std::lock_guard lock(mutex_);
      const auto active = active_by_pid_.find(pid);
      if (active != active_by_pid_.end()) {
        const auto found = sessions_.find(active->second);
        if (found != sessions_.end())
          prior_session = found->second;
      }
    }
    if (prior_session) {
      prior_session->refresh_process_state();
      const auto prior_state = prior_session->snapshot().state();
      if (prior_state != clangtracer::v1::Session::DETACHED &&
          prior_state != clangtracer::v1::Session::EXITED) {
        return {grpc::StatusCode::ALREADY_EXISTS,
                "an existing attached or failed session still owns this PID"};
      }
    }
    {
      std::lock_guard lock(mutex_);
      const auto active = active_by_pid_.find(pid);
      if (active != active_by_pid_.end() && prior_session &&
          active->second == prior_session->id()) {
        active_by_pid_.erase(active);
      } else if (active != active_by_pid_.end()) {
        return {grpc::StatusCode::ALREADY_EXISTS,
                "a session already owns this PID"};
      }
      if (sessions_.size() + pending_pids_.size() >= kSessionLimit) {
        return {grpc::StatusCode::RESOURCE_EXHAUSTED,
                "session history limit reached"};
      }
      if (!pending_pids_.insert(pid).second) {
        return {grpc::StatusCode::ALREADY_EXISTS,
                "an attach for this PID is in progress"};
      }
    }
    std::string context_endpoint;
    std::string producer_journal_directory;
    {
      std::lock_guard lock(mutex_);
      context_endpoint = context_endpoint_;
      if (!context_durable_directory_.empty())
        producer_journal_directory = context_durable_directory_ +
                                     "/producer-pid-" + std::to_string(pid) +
                                     "-" + std::to_string(process_start);
    }
    auto session = std::make_shared<Session>(
        id, pid, process_start, agent_path_, event_path_,
        std::move(context_endpoint), std::move(producer_journal_directory),
        timeout_, context_budget_);
    try {
      session->attach(request->expected_executable());
      std::lock_guard lock(mutex_);
      pending_pids_.erase(pid);
      sessions_.emplace(id, session);
      active_by_pid_[pid] = id;
    } catch (const injector::InitialAdmissionUnavailable &error) {
      // Admission is read-only and Process::attach has confirmed that all
      // seized tasks were released. Do not retain a failed session that owns
      // no target resources.
      std::lock_guard lock(mutex_);
      pending_pids_.erase(pid);
      return status_for(error);
    } catch (const AgentRuntimeIncompatible &error) {
      std::lock_guard lock(mutex_);
      pending_pids_.erase(pid);
      return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
    } catch (const std::exception &error) {
      session->record_failure(error.what());
      std::lock_guard lock(mutex_);
      pending_pids_.erase(pid);
      sessions_.emplace(id, session);
      return status_for(error);
    }
    response->CopyFrom(session->snapshot());
    return grpc::Status::OK;
  }

  grpc::Status ResolveProbe(grpc::ServerContext *context,
                            const clangtracer::v1::ResolveProbeRequest *request,
                            clangtracer::v1::ProbeSpec *response) override {
    const auto request_valid = validate_resolution_inputs(
        request->selector(), request->module(), request->compilation_database(),
        request->translation_unit(), tracer::prepared::is_typed_version(request->prepared().format_version()),
        request->include_headers(), request->compile_options());
    if (!request_valid.ok())
      return request_valid;
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    try {
      client::DebugLookupOptions lookup;
      lookup.module = request->module();
      lookup.max_entries = request->debug_max_entries();
      lookup.max_depth = request->debug_max_depth();
      lookup.is_cancelled = [&] { return context->IsCancelled(); };
      if (request->has_prepared() && request->prepared().selector() != request->selector())
        return {grpc::StatusCode::INVALID_ARGUMENT, "prepared selector mismatch"};
      auto resolved = request->has_prepared()
          ? client::bind_prepared_callable(session->pid(), request->prepared(), lookup)
          : client::resolve_debug_callable(session->pid(), request->selector(), lookup);
      for (const auto &header : request->include_headers())
        resolved.spec.add_include_headers(header);
      for (const auto &option : request->compile_options())
        resolved.spec.add_compile_options(option);
      if (!resolved.spec.has_header_context())
        client::resolve_header_context(resolved.spec, request->compilation_database(),
                                       request->translation_unit());
      set_probe_extra_compile_arguments(resolved.spec, request->extra_compile_arguments());
      response->Swap(&resolved.spec);
      return grpc::Status::OK;
    } catch (const client::DebugLookupLimit &error) {
      return {grpc::StatusCode::RESOURCE_EXHAUSTED, error.what()};
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::INVALID_ARGUMENT, error.what()};
    } catch (const std::exception &error) {
      if (context->IsCancelled())
        return {grpc::StatusCode::CANCELLED, "debug lookup cancelled"};
      if (request->has_prepared())
        return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
      return status_for(error);
    }
  }

  grpc::Status ValidateProbe(grpc::ServerContext *context,
                             const clangtracer::v1::ValidateProbeRequest *request,
                             clangtracer::v1::ValidationReport *response) override {
    if (request->selector().empty() ||
        request->selector().find('\0') != std::string::npos ||
        request->callback_source().empty() ||
        request->callback_source().size() > kMaxCallbackSource ||
        request->callback_source().find('\0') != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "selector and callback source must be nonempty and contain no NUL"};
    }
    if (request->level() != clangtracer::v1::ValidateProbeRequest::COMPILE &&
        request->level() != clangtracer::v1::ValidateProbeRequest::SYNTAX) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "unknown validation level"};
    }
    const auto request_valid = validate_resolution_inputs(
        request->selector(), request->module(), request->compilation_database(),
        request->translation_unit(), tracer::prepared::is_typed_version(request->prepared().format_version()),
        request->include_headers(), request->compile_options());
    if (!request_valid.ok())
      return request_valid;
    auto session = find_session(request->session_id());
    if (!session) {
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    }
    std::lock_guard operation_lock(session->operation_mutex());
    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED) {
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    }
    try {
      client::DebugLookupOptions lookup;
      lookup.module = request->module();
      lookup.max_entries = request->debug_max_entries();
      lookup.max_depth = request->debug_max_depth();
      lookup.is_cancelled = [&] { return context->IsCancelled(); };
      if (request->has_prepared() && request->prepared().selector() != request->selector())
        return {grpc::StatusCode::INVALID_ARGUMENT, "prepared selector mismatch"};
      auto resolved = request->has_prepared()
          ? client::bind_prepared_callable(session->pid(), request->prepared(), lookup)
          : client::resolve_debug_callable(session->pid(), request->selector(), lookup);
      for (const auto &header : request->include_headers())
        resolved.spec.add_include_headers(header);
      for (const auto &option : request->compile_options())
        resolved.spec.add_compile_options(option);
      if (!resolved.spec.has_header_context())
        client::resolve_header_context(resolved.spec, request->compilation_database(),
                                       request->translation_unit());
      set_probe_extra_compile_arguments(resolved.spec, request->extra_compile_arguments());
      resolved.spec.set_callback_source(request->callback_source());
      resolved.spec.set_allow_argument_overrides(request->allow_argument_overrides());
      resolved.spec.set_allow_result_override(request->allow_result_override());
      resolved.spec.set_allow_replacement(request->allow_replacement());
      resolved.spec.set_allow_call_original(request->allow_call_original());
      resolved.spec.set_trace_exceptions(request->trace_exceptions());
      resolved.spec.set_reclaimable(request->reclaimable());
      const auto input_valid = validate_probe(resolved.spec);
      if (!input_valid.ok())
        return input_valid;
      *response = session->validate_probe(resolved.spec, source_directory_,
                                          request->level());
      response->set_prepared_used(request->has_prepared());
      if (response->effective_options().empty())
        for (const auto &option : request->compile_options())
          response->add_effective_options(option);
      return grpc::Status::OK;
    } catch (const client::DebugLookupLimit &error) {
      return {grpc::StatusCode::RESOURCE_EXHAUSTED, error.what()};
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::INVALID_ARGUMENT, error.what()};
    } catch (const std::exception &error) {
      if (context->IsCancelled())
        return {grpc::StatusCode::CANCELLED, "probe validation cancelled"};
      if (request->has_prepared())
        return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
      return status_for(error);
    }
  }

  grpc::Status AddProbe(grpc::ServerContext *,
                        const clangtracer::v1::AddProbeRequest *request,
                        clangtracer::v1::Probe *response) override {
    const auto validation = validate_probe(request->spec());
    if (!validation.ok())
      return validation;
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    std::uint64_t probe_id = 0;
    return install_probe(session, request->spec(), {}, probe_id, response);
  }

  grpc::Status ReloadProbe(grpc::ServerContext *context,
                            const api::ReloadProbeRequest *request,
                            api::ReloadProbeResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      auto spec = session->reload_spec(request->probe_id(), request->callback_source());
      const auto validation = validate_probe(spec);
      if (!validation.ok())
        return validation;
      const auto retained = session->agent_retained_count();
      const auto snapshot = session->snapshot();
      const auto session_retained = std::count_if(snapshot.probes().begin(), snapshot.probes().end(),
          [](const auto &probe) { return !probe.reclaimed(); });
      if (!spec.reclaimable() && (retained == kMaxProbes || session_retained >= kMaxProbes))
        return {grpc::StatusCode::RESOURCE_EXHAUSTED, "reload reached the 32 retained-probe limit"};
      std::uint64_t pending_id = 0;
      const auto status = install_probe(session, spec, {}, pending_id, response->mutable_probe(),
                                        request->probe_id(), [&] {
        return context->IsCancelled() || std::chrono::system_clock::now() >= context->deadline();
      });
      if (!status.ok())
        return status;
      response->set_retained_limit(kMaxProbes);
      if (const auto used = session->agent_retained_count())
        response->set_agent_retained(*used);
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::NOT_FOUND, error.what()};
    } catch (const std::exception &error) {
      return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
    }
  }

  grpc::Status RemoveProbe(grpc::ServerContext *,
                           const clangtracer::v1::RemoveProbeRequest *request,
                           clangtracer::v1::Probe *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    return remove_probe(session, request->probe_id(), response);
  }

  grpc::Status Detach(grpc::ServerContext *,
                      const clangtracer::v1::DetachRequest *request,
                      clangtracer::v1::Session *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      session->refresh_process_state();
      session->detach();
      const auto snapshot = session->snapshot();
      for (const auto &probe : snapshot.probes()) {
        if (!events_.retire_probe(
                probe.session_id(), probe.id(),
                session->events_sent(probe.id()),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    timeout_)))
          throw std::runtime_error(
              "timed out waiting for emitted probe events");
      }
      erase_active_pid_if_inactive(session);
      response->CopyFrom(snapshot);
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      sync_event_probes(session);
      return status_for(error);
    }
  }

  grpc::Status
  ListSessions(grpc::ServerContext *,
               const clangtracer::v1::ListSessionsRequest *,
               clangtracer::v1::ListSessionsResponse *response) override {
    std::vector<std::shared_ptr<Session>> copy;
    {
      std::lock_guard lock(mutex_);
      for (const auto &[id, session] : sessions_) {
        (void)id;
        copy.push_back(session);
      }
    }
    for (const auto &session : copy) {
      std::lock_guard operation_lock(session->operation_mutex());
      session->refresh_process_state();
      const auto snapshot = session->snapshot();
      if (snapshot.state() == clangtracer::v1::Session::EXITED ||
          snapshot.state() == clangtracer::v1::Session::DETACHED) {
        sync_event_probes(session);
        erase_active_pid_if_inactive(session);
      }
      response->add_sessions()->CopyFrom(snapshot);
    }
    return grpc::Status::OK;
  }

  grpc::Status ListModules(
      grpc::ServerContext *, const clangtracer::v1::ListModulesRequest *request,
      clangtracer::v1::ListModulesResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    try {
      if (request->include_mappings() || !request->elf_path().empty()) {
        *response = client::list_debug_module_mappings(session->pid(), request->elf_path());
        response->set_mapping_token(session->id() + ":" + response->mapping_token());
      } else {
        for (const auto &path : client::list_debug_modules(session->pid()))
          response->add_modules(path);
      }
      session->refresh_process_state();
      if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "target process exited during module discovery"};
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::ABORTED, error.what()};
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ListMemoryMappings(
      grpc::ServerContext *context, const api::ListMemoryMappingsRequest *request,
      api::ListMemoryMappingsResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::list_mappings(*context, *session, timeout_, *response);
  }

  grpc::Status ReadMemory(grpc::ServerContext *context, const api::ReadMemoryRequest *request,
                          api::ReadMemoryResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::read(*context, *session, timeout_, *request, *response);
  }

  grpc::Status AllocateMemory(grpc::ServerContext *, const api::AllocateMemoryRequest *request,
                           api::MemoryAllocation *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "operation requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      *response = session->allocate_memory(request->size());
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status FreeMemory(grpc::ServerContext *, const api::FreeMemoryRequest *request,
                           api::MemoryAllocation *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "operation requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      *response = session->free_memory(request->allocation_id());
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status LoadTargetLibrary(grpc::ServerContext *, const api::LoadTargetLibraryRequest *request,
                           api::TargetLibrary *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "operation requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      *response = session->load_target_library(request->path());
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ApplyMemoryPatch(grpc::ServerContext *context, const api::ApplyMemoryPatchRequest *request,
                                api::MemoryPatch *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "memory patching requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::apply_patch(*context, *session, timeout_, *request, *response);
  }

  grpc::Status RestoreMemoryPatch(grpc::ServerContext *, const api::RestoreMemoryPatchRequest *request,
                                  api::MemoryPatch *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "memory patching requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      *response = session->restore_memory_patch(request->patch_id());
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status StartPageMonitor(grpc::ServerContext *context, const api::StartPageMonitorRequest *request,
                                api::PageMonitorResult *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "page monitoring requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::start_page_monitor(*context, *session, timeout_, *request, *response);
  }

  grpc::Status StopPageMonitor(grpc::ServerContext *, const api::StopPageMonitorRequest *request,
                               api::PageMonitorResult *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "page monitoring requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      *response = session->stop_page_monitor();
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ScanMemory(grpc::ServerContext *context, const api::ScanMemoryRequest *request,
                          api::ScanMemoryResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::scan(*context, *session, timeout_, *request, *response);
  }

  grpc::Status StartInstructionTrace(grpc::ServerContext *context,
      const api::StartInstructionTraceRequest *request, api::InstructionTrace *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "instruction tracing requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::start_instruction_trace(*context, *session, timeout_, *request, *response);
  }

  grpc::Status StopInstructionTrace(grpc::ServerContext *,
      const api::StopInstructionTraceRequest *request, api::InstructionTrace *response) override {
    if (!local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "instruction tracing requires local control"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      *response = session->stop_instruction_trace();
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ObjectWatch(grpc::ServerContext *, const api::ObjectWatchRequest *request,
                           api::ObjectWatchStatus *response) override {
    const bool prepare = request->action() == api::ObjectWatchRequest::PREPARE;
    auto session = find_session(request->session_id());
    if (!prepare && !session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    try {
      if (prepare && request->mode() == api::ObjectWatchRequest::SNAPSHOTS) {
        (void)prepare_snapshot_watch(*request, nullptr, *response);
        return grpc::Status::OK;
      }
      std::unique_lock<std::mutex> operation_lock;
      if (!prepare) {
        operation_lock = std::unique_lock<std::mutex>(session->operation_mutex());
        const auto group = snapshot_group(session->id());
        const bool existing = request->action() == api::ObjectWatchRequest::STATUS ||
                              request->action() == api::ObjectWatchRequest::STOP;
        if (existing && group &&
            (!request->watch_id() || request->watch_id() == group->events->id))
          return snapshot_watch_status(session, *group, request->action(), *response);
        if (request->mode() == api::ObjectWatchRequest::SNAPSHOTS && !existing)
          return start_snapshot_watch(session, *request, *response);
        if (request->action() == api::ObjectWatchRequest::START && group &&
            snapshot_group_active(*group, session->snapshot()))
          return {grpc::StatusCode::FAILED_PRECONDITION, "stop the active snapshot watch first"};
      }
      if (request->mode() != api::ObjectWatchRequest::WRITES)
        return {grpc::StatusCode::INVALID_ARGUMENT, "invalid object watch mode"};
      if (!request->application_rendezvous().empty())
        return {grpc::StatusCode::INVALID_ARGUMENT, "--rendezvous requires the AArch64 object server"};
      std::uint64_t id = 0;
      if (prepare || request->action() == api::ObjectWatchRequest::START ||
          request->action() == api::ObjectWatchRequest::PREVIEW) {
        if (request->writer_tid() && (prepare || !session ||
            !std::filesystem::exists(proc::directory(session->pid()) / "task" /
                                    std::to_string(request->writer_tid()))))
          throw std::invalid_argument("selected writer thread is not live in the target");
        for (const auto *value : {&request->concrete_type(), &request->base_type(),
                                 &request->owner_type(), &request->field(),
                                 &request->collection(), &request->snapshot_field(),
                                 &request->code_symbol(), &request->code_module(),
                                 &request->borrow_caller(), &request->vector_field(),
                                 &request->element_key_field(), &request->mutation_symbol(),
                                 &request->map_field(), &request->map_key(), &request->pointee_field(),
                                 &request->union_tag()}) {
          if (value->size() > 512 || value->find('\0') != std::string::npos)
            throw std::invalid_argument("object watch requires valid type, field and code selectors");
        }
        if (request->concrete_type().empty() || request->field().empty() ||
            request->code_symbol().empty())
          throw std::invalid_argument("object watch requires --type, --field and --code");
        const bool ownership = !request->owner_type().empty();
        const bool cache = request->backend() == api::ObjectWatchRequest::CODE_CACHE;
        if (!request->async_signals().empty() && !cache)
          throw std::invalid_argument("--async-signal requires native code-cache capture");
        std::set<unsigned> signal_numbers;
        for (const auto sig : request->async_signals())
          if (!sig || sig > 64 || sig == SIGKILL || sig == SIGSTOP || sig == SIGSEGV ||
              sig == SIGBUS || !signal_numbers.insert(sig).second)
            throw std::invalid_argument("invalid or duplicate asynchronous signal number");
        if (request->event_buffer() &&
            (!cache || request->event_buffer() < 2 || request->event_buffer() > 65536))
          throw std::invalid_argument("--event-buffer requires code-cache and 2 to 65536 slots");
        if (request->overflow_policy() != api::ObjectWatchRequest::DROP_NEW &&
            (!cache || request->overflow_policy() != api::ObjectWatchRequest::STOP_CAPTURE))
          throw std::invalid_argument("invalid code-cache overflow policy");
        if (request->cfi_stack() && (!cache || !request->stack_depth()))
          throw std::invalid_argument("native CFI requires code-cache and a positive stack depth");
        const bool vector = !request->vector_field().empty();
        const bool map = !request->map_field().empty();
        const bool pointee = !request->pointee_field().empty();
        const bool logical_range = vector || map || pointee;
        const bool tagged_union = !request->union_tag().empty();
        if (tagged_union != !request->union_cases().empty() ||
            request->union_cases_size() > 128 || (tagged_union && (ownership || logical_range)))
          throw std::invalid_argument("a standalone tagged union requires --union-tag and --union-case");
        if (tagged_union &&
            request->synchronization() != api::ObjectWatchRequest::THREAD_CONFINED &&
            request->synchronization() != api::ObjectWatchRequest::CALLER_LOCK)
          throw std::invalid_argument("tagged union capture requires a declared synchronization policy");
        if (request->writer_entry() && (!cache || ownership))
          throw std::invalid_argument("--writer-entry requires a standalone code-cache watch");
        if (vector != !request->element_key_field().empty() || vector != request->has_element_key() ||
            logical_range != !request->mutation_symbol().empty() ||
            (request->first_seen() && (ownership || !request->code_module().empty())) ||
            (logical_range && !request->first_seen()) || (map && vector) ||
            (pointee && (map || vector)) ||
            (!map && !request->map_key().empty()))
          throw std::invalid_argument(
              "first-seen scopes require a standalone watch; "
              "a logical collection requires a verified mutation and a supported key");
        if (cache && logical_range &&
            request->synchronization() != api::ObjectWatchRequest::THREAD_CONFINED &&
            request->synchronization() != api::ObjectWatchRequest::CALLER_LOCK)
          throw std::invalid_argument("native collection capture requires a declared synchronization policy");
        if (ownership != !request->base_type().empty() ||
            ownership != !request->collection().empty())
          throw std::invalid_argument("--owner, --base and --collection must be supplied together");
        if (cache) {
          if ((!ownership && !request->borrow_caller().empty()) || !request->snapshot_field().empty())
            throw std::invalid_argument("--caller requires ownership; code-cache ownership supports scalar fields");
          if (ownership && request->borrow_caller().empty() &&
              request->synchronization() != api::ObjectWatchRequest::THREAD_CONFINED &&
              request->synchronization() != api::ObjectWatchRequest::CALLER_LOCK)
            throw std::invalid_argument("native ownership capture requires a declared synchronization policy");
        } else if (ownership != !request->snapshot_field().empty() ||
                   !request->code_module().empty() || !request->borrow_caller().empty()) {
          throw std::invalid_argument("ptrace ownership requires --snapshot-field; --module and --caller require code-cache");
        }
        if (request->stack_depth() > 64 || !request->max_elements() ||
            request->max_elements() > 65536 || !request->max_string_bytes() ||
            request->max_string_bytes() > 65536 || request->max_instances() > 65536)
          throw std::invalid_argument("object watch limits exceed the supported range");
        if (request->backend() != api::ObjectWatchRequest::CODE_CACHE &&
            request->backend() != api::ObjectWatchRequest::PTRACE)
          throw std::invalid_argument("unknown object watch backend");
        if (request->action() == api::ObjectWatchRequest::START)
          do {
            id = next_probe_id_.fetch_add(1);
          } while (!id);
      }
      if (prepare) {
        *response = Session::prepare_object_watch(*request);
        return grpc::Status::OK;
      }
      *response = session->object_watch(*request, id, [this](auto event) {
        events_.publish_object_observation(std::move(event));
      });
      if (request->action() == api::ObjectWatchRequest::START) {
        std::lock_guard lock(mutex_);
        snapshot_groups_.erase(session->id());
      }
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status PreviewFunctionEntry(grpc::ServerContext *context,
                                   const api::PreviewFunctionEntryRequest *request,
                                   api::PreviewFunctionEntryResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    return memory::preview_entry(*context, *session, timeout_, *request, *response);
  }

  grpc::Status ListThreads(
      grpc::ServerContext *context, const clangtracer::v1::ListThreadsRequest *request,
      clangtracer::v1::ListThreadsResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    try {
      std::map<std::uint32_t, clangtracer::v1::TargetThread> threads;
      const auto tasks = proc::directory(session->pid()) / "task";
      for (const auto &entry : std::filesystem::directory_iterator(tasks)) {
        if (context->IsCancelled())
          return {grpc::StatusCode::CANCELLED, "thread discovery cancelled"};
        const auto name = entry.path().filename().string();
        std::uint32_t tid = 0;
        const auto end = name.data() + name.size();
        const auto parsed = std::from_chars(name.data(), end, tid);
        if (parsed.ec != std::errc{} || parsed.ptr != end || !tid)
          continue;
        try {
          const auto start = proc::start_time(entry.path() / "stat");
          std::ifstream comm(entry.path() / "comm");
          std::string thread_name;
          if (!std::getline(comm, thread_name, '\0') ||
              proc::start_time(entry.path() / "stat") != start)
            continue;
          if (!thread_name.empty() && thread_name.back() == '\n')
            thread_name.pop_back();
          auto &thread = threads[tid];
          thread.set_tid(tid);
          thread.set_name(std::move(thread_name));
          thread.set_start_time(start);
        } catch (const std::runtime_error &) {
          // A thread can exit or be replaced while its row is being read.
        }
        if (threads.size() > 4096)
          return {grpc::StatusCode::RESOURCE_EXHAUSTED, "thread listing limit exceeded"};
      }
      session->refresh_process_state();
      if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "target process exited during thread discovery"};
      for (auto &[tid, thread] : threads) {
        (void)tid;
        response->add_threads()->Swap(&thread);
      }
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ListSymbols(
      grpc::ServerContext *context,
      const clangtracer::v1::ListSymbolsRequest *request,
      clangtracer::v1::ListSymbolsResponse *response) override {
    if (request->session_id().empty() ||
        request->session_id().find('\0') != std::string::npos ||
        request->query().find('\0') != std::string::npos ||
        request->module().find('\0') != std::string::npos ||
        request->page_token().find('\0') != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid symbol listing request"};
    }
    const std::size_t page_size = request->page_size();
    if (request->address() &&
        (request->mapping_token().empty() || page_size || !request->page_token().empty()))
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "address lookup requires a mapping token and does not support paging"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED) {
      return {grpc::StatusCode::FAILED_PRECONDITION,
              "symbol listing requires a live attached session"};
    }
    const auto process_start = session->process_start_time();
    try {
      if (Session::ProcessStartTime(session->pid()) != process_start) {
        session->refresh_process_state();
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "target process exited or PID was reused"};
      }
    } catch (const std::exception &error) {
      return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
    }

    clangtracer::v1::ListSymbolsPageToken continuation;
    if (!request->page_token().empty()) {
      if (!decode_symbol_page_token(request->page_token(), &continuation) ||
          continuation.session_id() != request->session_id() ||
          continuation.pid() != session->pid() ||
          continuation.process_start_time() != process_start ||
          continuation.query() != request->query() ||
          continuation.module() != request->module() ||
          continuation.page_size() != page_size ||
          continuation.mapping_fingerprint().empty()) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "invalid or mismatched symbol page token"};
      }
    }
    auto expected_mapping = continuation.mapping_fingerprint();
    if (!request->mapping_token().empty()) {
      const auto prefix = session->id() + ":";
      const auto &token = request->mapping_token();
      if (token.compare(0, prefix.size(), prefix) != 0 ||
          token.size() != prefix.size() + 64)
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "mapping token does not identify this session"};
      const auto fingerprint = token.substr(prefix.size());
      if (!expected_mapping.empty() && expected_mapping != fingerprint)
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "mapping token does not match the symbol page"};
      expected_mapping = fingerprint;
    }

    try {
      const auto page = client::list_debug_symbols(
          session->pid(), request->query(), request->module(), page_size,
          continuation.last_selector(), continuation.last_linkage_symbol(),
          continuation.last_module_path(), expected_mapping,
          [&] { return context->IsCancelled(); }, request->include_addresses(),
          request->address());
      if (page.cancelled || context->IsCancelled())
        return {grpc::StatusCode::CANCELLED, "symbol discovery cancelled"};
      session->refresh_process_state();
      if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED) {
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "target process exited during symbol discovery"};
      }
      try {
        if (Session::ProcessStartTime(session->pid()) != process_start) {
          session->refresh_process_state();
          return {grpc::StatusCode::FAILED_PRECONDITION,
                  "target process exited or PID was reused during symbol discovery"};
        }
      } catch (const std::exception &error) {
        session->refresh_process_state();
        return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
      }
      for (const auto &item : page.symbols) {
        auto *symbol = response->add_symbols();
        symbol->set_selector(item.selector);
        symbol->set_linkage_symbol(item.linkage_symbol);
        symbol->set_module_path(item.module_path);
        symbol->set_runtime_address(item.runtime_address);
        symbol->set_elf_address(item.elf_address);
      }
      if (request->include_addresses() || request->address())
        response->set_mapping_token(session->id() + ":" + page.mapping_fingerprint);
      if (page.has_more && !page.symbols.empty()) {
        continuation.set_session_id(request->session_id());
        continuation.set_pid(session->pid());
        continuation.set_process_start_time(process_start);
        continuation.set_query(request->query());
        continuation.set_module(request->module());
        continuation.set_page_size(page_size);
        continuation.set_mapping_fingerprint(page.mapping_fingerprint);
        continuation.set_last_selector(page.symbols.back().selector);
        continuation.set_last_linkage_symbol(page.symbols.back().linkage_symbol);
        continuation.set_last_module_path(page.symbols.back().module_path);
        response->set_next_page_token(encode_symbol_page_token(continuation));
      }
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      const std::string detail = error.what();
      if (detail.find("mapped images changed") != std::string::npos) {
        return {grpc::StatusCode::ABORTED, detail};
      }
      return {grpc::StatusCode::INVALID_ARGUMENT, detail};
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status PreviewBulkTrace(
      grpc::ServerContext *context, const clangtracer::v1::PreviewBulkTraceRequest *request,
      clangtracer::v1::PreviewBulkTraceResponse *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    return preview_bulk_trace(context, session, request->rules(), response);
  }

  grpc::Status ApplyBulkTrace(grpc::ServerContext *context,
                              const api::ApplyBulkTraceRequest *request,
                              api::BulkTraceReport *response) override {
    using Entry = api::BulkTraceEntry;
    const auto &batch_id = request->batch_id();
    if (!valid_batch_id(batch_id))
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid bulk trace batch ID"};
    const std::string callback = request->has_callback_source()
        ? request->callback_source()
        : "struct Interceptor { void pre(const Call&) {} void post(const Call&) {} };";
    if (callback.empty() || callback.size() > kMaxCallbackSource ||
        callback.find('\0') != std::string::npos)
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "callback source must be 1 to 65536 bytes without NUL"};

    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    const auto snapshot = session->snapshot();
    if (std::any_of(snapshot.probes().begin(), snapshot.probes().end(),
                    [&](const auto &probe) { return probe.bulk_batch_id() == batch_id; }))
      return {grpc::StatusCode::ALREADY_EXISTS, "bulk trace batch ID is already retained"};

    api::PreviewBulkTraceResponse preview;
    const auto status = preview_bulk_trace(context, session, request->rules(), &preview);
    if (!status.ok())
      return status;
    response->set_batch_id(batch_id);
    *response->mutable_unmatched_exact_selectors() = preview.unmatched_exact_selectors();
    if (preview.has_available_slots())
      response->set_available_slots(preview.available_slots());

    bool blocked = !preview.unmatched_exact_selectors().empty();
    std::set<std::uint64_t> entries;
    for (const auto &candidate : preview.candidates()) {
      auto &row = *response->add_entries();
      const auto &function = candidate.function();
      *row.mutable_function() = function;
      if (candidate.abi_status() != api::BulkTraceCandidate::DWARF_SUPPORTED) {
        row.set_status(Entry::BLOCKED);
        row.set_diagnostic(api::BulkTraceCandidate::AbiStatus_Name(candidate.abi_status()) +
                           ": " + candidate.diagnostic());
      } else if (!entries.insert(function.runtime_address()).second) {
        row.set_status(Entry::BLOCKED);
        row.set_diagnostic("multiple selections refer to the same runtime entry");
      } else if (std::any_of(snapshot.probes().begin(), snapshot.probes().end(),
                            [&](const auto &probe) {
        return probe.active() && probe.entry_address() == function.runtime_address();
      })) {
        row.set_status(Entry::BLOCKED);
        row.set_diagnostic("entry is already hooked");
      }
      blocked |= row.status() == Entry::BLOCKED;
    }
    if (blocked)
      response->set_diagnostic("selection contains blocked or unmatched entries");
    else if (response->entries().empty())
      response->set_diagnostic("selection matched no entries");
    else if (!preview.has_available_slots())
      response->set_diagnostic("agent capacity could not be verified");
    else if (static_cast<unsigned>(response->entries_size()) > preview.available_slots())
      response->set_diagnostic("selection exceeds available retained probe slots");
    if (!response->diagnostic().empty())
      return grpc::Status::OK;

    // Resolve all exact bindings before changing target code. The agent checks
    // the binding again at installation, including the mapped image identity.
    std::vector<api::ProbeSpec> specs;
    specs.reserve(response->entries_size());
    client::DebugLookupOptions lookup;
    lookup.is_cancelled = [&] { return context->IsCancelled(); };
    for (auto &row : *response->mutable_entries()) {
      const auto &function = row.function();
      lookup.module = function.module_path();
      try {
        auto spec = client::resolve_debug_callable(session->pid(), function.selector(), lookup).spec;
        const auto &info = spec.debug_info();
        if (info.module() != function.module_path() || spec.symbol() != function.linkage_symbol() ||
            info.runtime_entry() != function.runtime_address() ||
            info.entry_virtual_address() != function.elf_address())
          throw std::runtime_error("function binding changed since preview");
        spec.set_callback_source(callback);
        const auto validation = validate_probe(spec);
        if (!validation.ok())
          throw std::runtime_error(validation.error_message());
        specs.push_back(std::move(spec));
      } catch (const std::exception &error) {
        if (context->IsCancelled())
          return {grpc::StatusCode::CANCELLED, "bulk trace apply cancelled during preflight"};
        row.set_status(Entry::BLOCKED);
        row.set_diagnostic(error.what());
        blocked = true;
      }
    }
    if (blocked) {
      response->set_diagnostic("one or more function bindings could not be validated");
      return grpc::Status::OK;
    }

    // Keep the attempted IDs even when installation throws after preparing
    // or writing an entry. Only IDs retained by this batch can be restored.
    std::vector<std::uint64_t> attempted(specs.size());
    for (std::size_t i = 0; i < specs.size() && !context->IsCancelled(); ++i) {
      auto &row = *response->mutable_entries(i);
      api::Probe probe;
      const auto result = install_probe(session, specs[i], batch_id, attempted[i], &probe);
      if (!result.ok()) {
        row.set_status(Entry::INSTALL_FAILED);
        row.set_diagnostic(result.error_message());
        response->set_diagnostic("bulk installation failed");
        break;
      }
      *row.mutable_probe() = std::move(probe);
      row.set_status(Entry::INSTALLED);
    }
    if (context->IsCancelled() && response->diagnostic().empty())
      response->set_diagnostic("bulk trace apply cancelled");
    if (response->diagnostic().empty()) {
      response->set_complete(true);
      return grpc::Status::OK;
    }
    const auto retained = session->snapshot();
    for (int i = response->entries_size() - 1; i >= 0; --i) {
      const auto probe = std::find_if(retained.probes().begin(), retained.probes().end(),
                                     [&](const auto &item) {
        return item.id() == attempted[i] && item.bulk_batch_id() == batch_id;
      });
      if (probe != retained.probes().end())
        restore_bulk_entry(session, *probe, Entry::ROLLED_BACK, response->mutable_entries(i));
    }
    return grpc::Status::OK;
  }

  grpc::Status RemoveBulkTrace(grpc::ServerContext *,
                               const api::RemoveBulkTraceRequest *request,
                               api::BulkTraceReport *response) override {
    using Entry = api::BulkTraceEntry;
    if (!valid_batch_id(request->batch_id()))
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid bulk trace batch ID"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    response->set_batch_id(request->batch_id());
    const auto snapshot = session->snapshot();
    bool complete = true;
    for (auto it = snapshot.probes().rbegin(); it != snapshot.probes().rend(); ++it) {
      if (it->bulk_batch_id() != request->batch_id())
        continue;
      auto *row = response->add_entries();
      row->mutable_function()->set_linkage_symbol(it->symbol());
      row->mutable_function()->set_runtime_address(it->entry_address());
      restore_bulk_entry(session, *it, Entry::REMOVED, row);
      complete &= row->status() == Entry::REMOVED;
    }
    if (response->entries().empty())
      return {grpc::StatusCode::NOT_FOUND, "bulk trace batch has no retained probes in this session"};
    response->set_complete(complete);
    if (!complete)
      response->set_diagnostic("bulk trace removal is incomplete");
    return grpc::Status::OK;
  }

  grpc::Status WatchEvents(
      grpc::ServerContext *context,
      const clangtracer::v1::WatchEventsRequest *request,
      grpc::ServerWriter<clangtracer::v1::TraceEvent> *writer) override {
    if (request->names_size() > 64) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "at most 64 name filters are allowed"};
    }
    for (const auto &name : request->names()) {
      if (name.empty() || name.size() > 4096 || name.find('\0') != std::string::npos) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "each name filter must be 1 to 4096 bytes without NUL"};
      }
    }
    if (!events_.stream(context, request, writer)) {
      return {grpc::StatusCode::OUT_OF_RANGE,
              "event cursor is older than retained history"};
    }
    return grpc::Status::OK;
  }

  grpc::Status
  ReadEvents(grpc::ServerContext *,
             const clangtracer::v1::ReadEventsRequest *request,
             clangtracer::v1::ReadEventsResponse *response) override {
    if (request->limit() > 256 || request->filter().names_size() > 64 ||
        request->filter().session_ids_size() > 64) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "drain limit is at most 256; filters at most 64"};
    }
    for (const auto &name : request->filter().names()) {
      if (name.empty() || name.size() > 4096 ||
          name.find('\0') != std::string::npos) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "invalid event name filter"};
      }
    }
    return events_.read(*request, *response);
  }

  grpc::Status AcknowledgeEvents(
      grpc::ServerContext *,
      const clangtracer::v1::AcknowledgeEventsRequest *request,
      clangtracer::v1::AcknowledgeEventsResponse *response) override {
    if (request->server_instance_id().empty() ||
        request->sequences_size() > 256) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "server instance and at most 256 sequences required"};
    }
    return events_.acknowledge(*request, *response);
  }

  bool shutdown_sessions() {
    std::vector<std::shared_ptr<Session>> copy;
    {
      std::lock_guard lock(mutex_);
      for (const auto &[id, session] : sessions_) {
        (void)id;
        copy.push_back(session);
      }
    }
    std::vector<std::thread> workers;
    workers.reserve(copy.size());
    try {
      for (const auto &session : copy) {
        workers.emplace_back([session] { session->shutdown(); });
      }
    } catch (...) {
      for (auto &worker : workers)
        if (worker.joinable())
          worker.join();
      throw;
    }
    for (auto &worker : workers)
      worker.join();
    bool clean = true;
    const auto drain_deadline = std::chrono::steady_clock::now() + timeout_;
    for (const auto &session : copy) {
      const auto snapshot = session->snapshot();
      if (snapshot.state() == clangtracer::v1::Session::FAILED) {
        clean = false;
        std::cerr << "cleanup incomplete for session " << snapshot.id() << ": "
                  << snapshot.error() << '\n';
      }
      for (const auto &probe : snapshot.probes()) {
        if (!probe.active() && !probe.draining() &&
            !events_.retire_probe(
                probe.session_id(), probe.id(),
                session->events_sent(probe.id()),
                std::max(
                    std::chrono::milliseconds(0),
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        drain_deadline - std::chrono::steady_clock::now())))) {
          clean = false;
          std::cerr << "event drain incomplete for probe " << probe.id()
                    << '\n';
        }
      }
    }
    events_.stop();
    return clean;
  }

private:
  struct SnapshotGroup {
    std::shared_ptr<EventHub::SnapshotWatch> events;
    std::vector<std::uint64_t> probes;
  };
  struct SnapshotMethod {
    api::ProbeSpec spec;
    api::ObjectObservation origin;
  };
  std::string agent_path_;
  std::string event_path_;
  std::string source_directory_;
  std::chrono::seconds timeout_;
  std::string context_endpoint_;
  std::string context_durable_directory_;
  ContextTransportBudget context_budget_;
  std::condition_variable context_endpoint_cv_;
  bool context_endpoint_ready_ = false;
  const bool local_control_;
  EventHub events_;
  std::atomic<std::uint64_t> next_probe_id_;
  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<Session>> sessions_;
  std::map<std::string, std::shared_ptr<SnapshotGroup>> snapshot_groups_;
  std::map<std::uint32_t, std::string> active_by_pid_;
  std::set<std::uint32_t> pending_pids_;

  std::shared_ptr<Session> find_session(const std::string &id) {
    std::lock_guard lock(mutex_);
    const auto found = sessions_.find(id);
    return found == sessions_.end() ? nullptr : found->second;
  }

  std::shared_ptr<SnapshotGroup> snapshot_group(const std::string &id) {
    std::lock_guard lock(mutex_);
    const auto found = snapshot_groups_.find(id);
    return found == snapshot_groups_.end() ? nullptr : found->second;
  }

  static bool snapshot_group_active(const SnapshotGroup &group, const api::Session &session) {
    return std::any_of(session.probes().begin(), session.probes().end(), [&](const auto &probe) {
      return (probe.active() || probe.draining()) &&
          std::find(group.probes.begin(), group.probes.end(), probe.id()) != group.probes.end();
    });
  }

  std::vector<SnapshotMethod> prepare_snapshot_watch(const api::ObjectWatchRequest &request,
                                                     Session *session,
                                                     api::ObjectWatchStatus &response) {
    if (request.methods().empty() || request.methods_size() > 16 ||
        request.concrete_type().empty() || request.concrete_type().size() > 512 ||
        request.stack_depth() > 64 || request.no_values() || !request.base_type().empty() ||
        request.first_seen() || request.writer_entry() || request.writer_tid() || request.event_buffer() ||
        !request.vector_field().empty() ||
        request.has_element_key() || !request.map_field().empty() || !request.map_key().empty() ||
        !request.pointee_field().empty() ||
        !request.union_tag().empty() || !request.union_cases().empty() ||
        !request.async_signals().empty() ||
        !request.element_key_field().empty() || !request.mutation_symbol().empty() ||
        !request.additional_mutation_symbols().empty() ||
        !request.owner_type().empty() || !request.collection().empty() ||
        !request.owner_add().empty() || !request.owner_get().empty() ||
        !request.owner_remove().empty() || !request.owner_snapshot().empty() ||
        !request.code_symbol().empty() || !request.code_module().empty() ||
        !request.additional_code_symbols().empty() || request.syscall_output() || request.cfi_stack() ||
        request.serialize_wait_us() || request.follow_calls() || request.runtime_limit() ||
        !request.shared_file().empty() ||
        request.overflow_policy() != api::ObjectWatchRequest::DROP_NEW ||
        !request.borrow_caller().empty() || !request.snapshot_field().empty() ||
        request.backend() != api::ObjectWatchRequest::PTRACE ||
        !request.max_elements() || request.max_elements() > context_engine::max_capture_items ||
        !request.max_string_bytes() || request.max_string_bytes() > max_snapshot_bytes ||
        !request.snapshot_max_bytes() || request.snapshot_max_bytes() > max_snapshot_bytes ||
        !request.snapshot_max_depth() || request.snapshot_max_depth() > context_engine::max_capture_depth ||
        !request.max_instances() || request.max_instances() > 65536)
      throw std::invalid_argument("snapshot watches require 1 to 16 prepared methods and a concrete receiver type");
    if (request.snapshot_module().size() > 4096 ||
        request.snapshot_module().find('\0') != std::string::npos ||
        (!session && !request.snapshot_module().empty()))
      throw std::invalid_argument("snapshot module must select a live mapped ELF");
    if (request.synchronization() != api::ObjectWatchRequest::THREAD_CONFINED &&
        request.synchronization() != api::ObjectWatchRequest::CALLER_LOCK &&
        request.synchronization() != api::ObjectWatchRequest::STABLE_AT_BOUNDARIES)
      throw std::invalid_argument("snapshot watch requires a declared receiver synchronization policy");
    if (request.snapshot_fields_size() > 128 || request.snapshot_keys_size() > 128)
      throw std::invalid_argument("too many selected snapshot paths or identity rules");
    std::set<std::string> fields;
    for (const auto &field : request.snapshot_fields())
      if (field.empty() || field.size() > 512 || field.find('\0') != std::string::npos ||
          !fields.insert(field).second)
        throw std::invalid_argument("snapshot field paths must be nonempty and unique");
    std::vector<std::string> keys(request.snapshot_keys().begin(), request.snapshot_keys().end());
    for (const auto &key : keys)
      if (key.size() > 512 || key.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid snapshot identity rule");
    tracer_repl::validate_snapshot_keys(keys);
    response.set_mode(api::ObjectWatchRequest::SNAPSHOTS);
    auto &plan = *response.mutable_plan();
    plan.set_field_encoding("typed receiver");
    std::vector<SnapshotMethod> methods;
    std::set<std::uint64_t> entries;
    for (const auto &prepared : request.methods()) {
      std::string_view receiver = prepared.header_types().record_name();
      if (::tracer::compat::starts_with(receiver, "::"))
        receiver.remove_prefix(2);
      std::string_view type = request.concrete_type();
      if (::tracer::compat::starts_with(type, "::"))
        type.remove_prefix(2);
      if (prepared.ByteSizeLong() > 65536 || prepared.module_sha256().size() != 32 ||
          !tracer::prepared::is_typed_version(prepared.format_version()) ||
          prepared.signature().kind() != api::METHOD || receiver != type)
        throw std::invalid_argument("snapshot method requires a pinned typed adapter for the selected receiver");
      api::ProbeSpec spec;
      std::string module = request.executable_file();
      if (session) {
        client::DebugLookupOptions lookup;
        lookup.module = request.snapshot_module();
        try {
          spec = client::bind_prepared_callable(session->pid(), prepared, lookup).spec;
        } catch (const std::runtime_error &error) {
          if (!::tracer::compat::starts_with(std::string_view(error.what()), "PREPARED_MODULE_MISMATCH"))
            throw;
          std::vector<std::string> candidates;
          const auto wanted = lookup.module.empty() ? prepared.module_name() : lookup.module;
          for (const auto &path : client::list_debug_modules(session->pid()))
            if (path == wanted || std::filesystem::path(path).filename() == wanted)
              candidates.push_back(path);
          if (candidates.size() != 1)
            throw std::runtime_error("compatible snapshot rebinding needs one mapped module");
          auto compatible = object_watch::Plan::method_boundary(candidates.front(), prepared, nullptr, true);
          auto updated = prepared;
          updated.set_module_sha256(std::move(compatible.file_sha256));
          updated.set_module_name(std::filesystem::path(candidates.front()).filename().string());
          lookup.module = candidates.front();
          spec = client::bind_prepared_callable(session->pid(), updated, lookup).spec;
        }
        module = spec.debug_info().module();
      }
      const auto &bound_adapter = session ? spec.prepared() : prepared;
      auto boundary = object_watch::Plan::method_boundary(module, bound_adapter,
          session ? &spec.debug_info() : nullptr);
      if (!entries.insert(boundary.binding.elf_pc()).second)
        throw std::invalid_argument("duplicate or aliased method entry in snapshot plan");
      if (plan.binary_sha256().empty()) {
        plan.set_binary_sha256(boundary.origin.module_sha256());
        plan.set_writer_build_id(boundary.binding.module_build_id());
        plan.set_executable_build_id(boundary.binding.module_build_id());
      } else if (plan.binary_sha256() != boundary.origin.module_sha256()) {
        throw std::invalid_argument("all snapshot methods must belong to the same prepared ELF");
      }
      *plan.add_bindings() = std::move(boundary.binding);
      if (session) {
        const auto &info = spec.debug_info();
        (void)client::verified_function_code(session->pid(), info, 1ULL << 20, bound_adapter.module_sha256());
        spec.set_callback_source(R"cpp(
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void pre(const Call &) {}
  void post(const Call &) {}
};
)cpp");
        spec.set_native_backend("entry");
        spec.set_snapshot_receiver_at_return(true);
        spec.set_snapshot_max_items(request.max_elements());
        spec.set_snapshot_max_string_bytes(request.max_string_bytes());
        spec.set_snapshot_max_depth(request.snapshot_max_depth());
        spec.set_snapshot_max_bytes(request.snapshot_max_bytes());
        spec.set_snapshot_report_unavailable(true);
        *spec.mutable_snapshot_receiver_fields() = request.snapshot_fields();
        for (const auto &[begin, end] : memory::readonly_image_data(*session, module, timeout_)) {
          auto &range = *spec.add_snapshot_rtti_ranges();
          range.set_begin(begin);
          range.set_end(end);
        }
        spec.set_stack_depth(request.stack_depth());
        const auto valid = validate_probe(spec);
        if (!valid.ok())
          throw std::invalid_argument(valid.error_message());
      }
      methods.push_back({std::move(spec), std::move(boundary.origin)});
    }
    if (session) {
      if (!request.has_prepared_bindings())
        throw std::invalid_argument("snapshot watch requires prepared ELF bindings");
      response.set_binding_status(object_watch::compare_prepared(request.prepared_bindings(), plan, false));
    } else {
      response.set_binding_status("offline");
    }
    response.set_diagnostic("typed method boundaries; receiver lifetime and thread safety require separate evidence");
    return methods;
  }

  grpc::Status snapshot_watch_status(const std::shared_ptr<Session> &session,
      const SnapshotGroup &group, api::ObjectWatchRequest::Action action,
      api::ObjectWatchStatus &response) {
    session->refresh_process_state();
    auto current = session->snapshot();
    if (action == api::ObjectWatchRequest::STOP && current.state() == api::Session::ATTACHED) {
      for (const auto id : group.probes) {
        api::Probe restored;
        const auto result = remove_probe(session, id, &restored);
        if (!result.ok()) {
          response.set_restoration_pending(true);
          response.set_diagnostic(response.diagnostic() + " " + result.error_message());
        }
      }
      current = session->snapshot();
    }
    sync_event_probes(session);
    response.set_mode(api::ObjectWatchRequest::SNAPSHOTS);
    response.set_watch_id(group.events->id);
    response.set_active(snapshot_group_active(group, current));
    for (const auto &probe : current.probes())
      if (std::find(group.probes.begin(), group.probes.end(), probe.id()) != group.probes.end())
        *response.add_method_probes() = probe;
    events_.snapshot_status(*group.events, response);
    return grpc::Status::OK;
  }

  grpc::Status start_snapshot_watch(const std::shared_ptr<Session> &session,
      const api::ObjectWatchRequest &request, api::ObjectWatchStatus &response) {
    session->refresh_process_state();
    const auto current = session->snapshot();
    if (current.state() != api::Session::ATTACHED)
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    if (request.action() != api::ObjectWatchRequest::START &&
        request.action() != api::ObjectWatchRequest::PREVIEW)
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid snapshot watch action"};
    if (request.action() == api::ObjectWatchRequest::START) {
      if (const auto previous = snapshot_group(session->id()); previous &&
          snapshot_group_active(*previous, current))
        return {grpc::StatusCode::ALREADY_EXISTS, "a snapshot watch is already active"};
    }
    auto methods = prepare_snapshot_watch(request, session.get(), response);
    if (request.action() == api::ObjectWatchRequest::PREVIEW)
      return grpc::Status::OK;
    auto group = std::make_shared<SnapshotGroup>();
    group->events = std::make_shared<EventHub::SnapshotWatch>();
    do {
      group->events->id = next_probe_id_.fetch_add(1);
    } while (!group->events->id);
    group->events->type = request.concrete_type();
    group->events->fields.assign(request.snapshot_fields().begin(), request.snapshot_fields().end());
    group->events->keys.assign(request.snapshot_keys().begin(), request.snapshot_keys().end());
    group->events->synchronization = request.synchronization();
    group->events->max_instances = request.max_instances();
    group->events->max_events = request.max_events();
    group->events->timeout = std::chrono::seconds(request.timeout_seconds());
    group->probes.reserve(methods.size());
    for (auto &method : methods) {
      std::uint64_t id = 0;
      api::Probe probe;
      const auto result = install_probe(session, method.spec, {}, id, &probe, 0, {},
          EventHub::SnapshotBinding{group->events, std::move(method.origin), {}, nullptr});
      if (!result.ok()) {
        const auto latest = session->snapshot();
        if (std::any_of(latest.probes().begin(), latest.probes().end(),
                        [&](const auto &item) { return item.id() == id; }))
          group->probes.push_back(id);
        snapshot_watch_status(session, *group, api::ObjectWatchRequest::STOP, response);
        if (response.restoration_pending()) {
          const std::lock_guard lock(mutex_);
          snapshot_groups_[session->id()] = group;
          response.set_diagnostic(result.error_message() + response.diagnostic());
          return grpc::Status::OK;
        }
        return result;
      }
      group->probes.push_back(probe.id());
    }
    {
      const std::lock_guard lock(mutex_);
      snapshot_groups_[session->id()] = group;
    }
    events_.activate_snapshot_watch(*group->events);
    return snapshot_watch_status(session, *group, api::ObjectWatchRequest::STATUS, response);
  }

  void erase_active_pid_if_inactive(const std::shared_ptr<Session> &session) {
    const auto state = session->snapshot().state();
    if (state != clangtracer::v1::Session::DETACHED &&
        state != clangtracer::v1::Session::EXITED)
      return;
    std::lock_guard lock(mutex_);
    const auto found = active_by_pid_.find(session->pid());
    if (found != active_by_pid_.end() && found->second == session->id()) {
      active_by_pid_.erase(found);
    }
  }

  static bool valid_batch_id(const std::string &id) {
    return id.size() == 32 && id.find_first_not_of("0123456789abcdef") == std::string::npos;
  }

  void restore_bulk_entry(const std::shared_ptr<Session> &session, const api::Probe &probe,
                          api::BulkTraceEntry::Status success, api::BulkTraceEntry *row) {
    auto *result = row->mutable_probe();
    *result = probe;
    const auto status = remove_probe(session, probe.id(), result);
    if (status.ok()) {
      row->set_status(success);
      return;
    }
    const auto snapshot = session->snapshot();
    for (const auto &current : snapshot.probes())
      if (current.id() == probe.id())
        *result = current;
    row->set_status(api::BulkTraceEntry::RESTORE_PENDING);
    const auto previous = row->diagnostic();
    row->set_diagnostic((previous.empty() ? "" : previous + " | ") +
                        "restore: " + status.error_message());
  }

  // The caller holds the session operation lock throughout these operations.
  grpc::Status install_probe(const std::shared_ptr<Session> &session,
                             const api::ProbeSpec &spec, const std::string &batch_id,
                             std::uint64_t &pending_probe_id, api::Probe *response,
                             std::uint64_t reload_probe_id = 0,
                             const std::function<bool()> &cancelled = {},
                             EventHub::SnapshotBinding snapshot = {}) {
    pending_probe_id = 0;
    try {
      pending_probe_id = next_probe_id_.fetch_add(1);
      if (pending_probe_id == 0)
        pending_probe_id = next_probe_id_.fetch_add(1);
      const auto &selector = spec.debug_info().selector();
      const auto function_name =
          spec.has_debug_info() && !selector.empty() && selector.size() <= 4096 &&
                  selector.find('\0') == std::string::npos
              ? selector
              : spec.symbol();
      events_.set_probe(session->id(), pending_probe_id, session->pid(),
                        function_name, true, &spec.prepared(), std::move(snapshot));
      const auto lineage = admit_probe_package(spec);
      if (!lineage.retained) {
        std::cerr << "clang-tracer: shared-context source lineage isolated: "
                  << lineage.diagnostic.substr(0, 240) << '\n';
      }
      auto probe = session->add_probe(spec, source_directory_, pending_probe_id,
                                      lineage.id, batch_id, reload_probe_id, cancelled);
      sync_event_probes(session);
      response->CopyFrom(probe);
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      sync_event_probes(session, pending_probe_id);
      return {grpc::StatusCode::INVALID_ARGUMENT, error.what()};
    } catch (const std::exception &error) {
      sync_event_probes(session, pending_probe_id);
      return status_for(error);
    }
  }

  grpc::Status remove_probe(const std::shared_ptr<Session> &session,
                            std::uint64_t probe_id, api::Probe *response) {
    try {
      auto probe = session->remove_probe(probe_id);
      if (!events_.retire_probe(
              probe.session_id(), probe.id(), session->events_sent(probe.id()),
              std::chrono::duration_cast<std::chrono::milliseconds>(timeout_)))
        throw std::runtime_error("timed out waiting for emitted probe events");
      response->CopyFrom(probe);
      erase_active_pid_if_inactive(session);
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      sync_event_probes(session);
      return {grpc::StatusCode::NOT_FOUND, error.what()};
    } catch (const std::exception &error) {
      sync_event_probes(session);
      return status_for(error);
    }
  }

  grpc::Status preview_bulk_trace(grpc::ServerContext *context,
                                  const std::shared_ptr<Session> &session,
                                  const api::BulkTraceRules &rules,
                                  api::PreviewBulkTraceResponse *response) {
    if (rules.version() != 1)
      return {grpc::StatusCode::INVALID_ARGUMENT, "unsupported bulk manifest version"};
    if (rules.include_globs().empty() && rules.exact_selectors().empty())
      return {grpc::StatusCode::INVALID_ARGUMENT, "bulk preview requires an include glob or exact signature"};
    std::size_t count = 0;
    for (const auto *group : {&rules.module_globs(), &rules.include_globs(),
                              &rules.exclude_globs(), &rules.exact_selectors()}) {
      count += group->size();
      if (count > 128)
        return {grpc::StatusCode::INVALID_ARGUMENT, "bulk preview allows at most 128 rules"};
      for (const auto &rule : *group)
        if (rule.empty() || rule.size() > 4096 || rule.find('\0') != std::string::npos)
          return {grpc::StatusCode::INVALID_ARGUMENT, "bulk rules must contain 1 to 4096 bytes without NUL"};
    }
    for (const auto &selector : rules.exact_selectors())
      if (selector.find('(') == std::string::npos || selector.find(')') == std::string::npos)
        return {grpc::StatusCode::INVALID_ARGUMENT, "exact selection requires a full C++ signature"};

    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    try {
      auto preview = client::preview_bulk_trace(
          session->pid(), rules, [&] { return context->IsCancelled(); });
      const auto agent_retained = session->agent_retained_count();
      session->refresh_process_state();
      const auto snapshot = session->snapshot();
      if (snapshot.state() != clangtracer::v1::Session::ATTACHED)
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "target process exited during bulk preview"};
      preview.set_retained_limit(kMaxProbes);
      preview.set_session_retained(std::count_if(snapshot.probes().begin(), snapshot.probes().end(),
          [](const auto &probe) { return !probe.reclaimed(); }));
      if (agent_retained) {
        preview.set_agent_retained(*agent_retained);
        const auto retained = std::max(*agent_retained, preview.session_retained());
        preview.set_available_slots(kMaxProbes - std::min(kMaxProbes, retained));
      }
      response->Swap(&preview);
      return grpc::Status::OK;
    } catch (const client::DebugLookupLimit &error) {
      return {grpc::StatusCode::RESOURCE_EXHAUSTED, error.what()};
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::ABORTED, error.what()};
    } catch (const std::exception &error) {
      if (context->IsCancelled())
        return {grpc::StatusCode::CANCELLED, "bulk trace preview cancelled"};
      return status_for(error);
    }
  }

  void sync_event_probes(const std::shared_ptr<Session> &session,
                         std::uint64_t pending_probe_id = 0) {
    const auto snapshot = session->snapshot();
    if (pending_probe_id &&
        std::none_of(
            snapshot.probes().begin(), snapshot.probes().end(),
            [&](const auto &probe) { return probe.id() == pending_probe_id; }))
      events_.set_probe(session->id(), pending_probe_id, session->pid(), {}, false);
    for (const auto &probe : snapshot.probes()) {
      if (probe.active() || probe.draining())
        events_.set_probe(probe.session_id(), probe.id(), session->pid(),
                          probe.symbol(), true);
      else
        (void)events_.retire_probe(probe.session_id(), probe.id(),
                                   session->events_sent(probe.id()),
                                   std::chrono::milliseconds(0));
    }
  }

  grpc::Status validate_probe(const clangtracer::v1::ProbeSpec &spec) const {
    if ((has_scalar_overrides(spec) || spec.reclaimable()) && !local_control_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "scalar overrides require a local Unix control socket"};
    if (const auto *error = scalar_override_error(spec))
      return {grpc::StatusCode::INVALID_ARGUMENT, error};
    if ((!spec.native_backend().empty() && spec.native_backend() != "auto" &&
         spec.native_backend() != "entry") ||
        (!spec.direct_caller().empty() &&
         (spec.native_backend() != "auto" || spec.direct_caller().size() > 4096 ||
          spec.direct_caller().find('\0') != std::string::npos))) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid native probe backend or caller"};
    }
    if (spec.has_prepared() && !spec.has_debug_info())
      return {grpc::StatusCode::INVALID_ARGUMENT, "prepared probe requires a live binding"};
    const bool typed_prepared = tracer::prepared::is_typed_version(spec.prepared().format_version());
    if (typed_prepared && !spec.include_headers().empty() && !spec.has_header_context())
      return {grpc::StatusCode::INVALID_ARGUMENT, "prepared injection requires a callback compilation context for optional includes"};
    if (spec.ByteSizeLong() > 256 * 1024) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "probe specification exceeds 256 KiB"};
    }
    if (spec.symbol().empty() ||
        spec.symbol().size() >= sizeof(Request{}.symbol) ||
        spec.symbol().find('\0') != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "symbol must be 1 to 511 bytes without NUL"};
    }
    if (spec.callback_source().empty() ||
        spec.callback_source().size() > kMaxCallbackSource ||
        spec.callback_source().find('\0') != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "callback source must be 1 to 65536 bytes without NUL"};
    }
    const auto &signature = spec.signature();
    if (signature.kind() != clangtracer::v1::FUNCTION &&
        signature.kind() != clangtracer::v1::METHOD) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "callable kind must be FUNCTION or METHOD"};
    }
    const auto valid_text = [](const std::string &value, std::size_t limit,
                               bool allow_empty = false) {
      return (allow_empty || !value.empty()) && value.size() <= limit &&
             value.find('\0') == std::string::npos;
    };
    if (!valid_text(spec.callback_package_path(), 4095, true) ||
        (!spec.callback_package_path().empty() &&
         spec.callback_package_path().front() != '/')) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "callback package path must be an absolute path up to 4095 bytes"};
    }
    if (!valid_text(spec.receiver_unavailable_reason(), 4096, true)) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "invalid receiver diagnostic"};
    }
    if (spec.compile_options_size() > 128) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "at most 128 compiler options are allowed"};
    }
    std::size_t compile_option_bytes = 0;
    for (const auto &option : spec.compile_options()) {
      compile_option_bytes += option.size() + 1;
      if (!valid_text(option, 4096) || compile_option_bytes > 65536) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "compiler options must be nonempty and total at most 64 KiB"};
      }
    }
    if (!safe_preflight_compiler_arguments(spec.compile_options().begin(),
                                          spec.compile_options().end())) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "compiler options contain an unsupported preflight option"};
    }
    if (!spec.extra_compile_arguments().empty()) {
      try {
        const auto &extra = spec.extra_compile_arguments();
        validate_extra_compile_arguments({extra.begin(), extra.end()},
                                         primary_probe_compile_arguments(spec),
                                         primary_probe_compile_directory(spec));
      } catch (const std::invalid_argument &error) {
        return {grpc::StatusCode::INVALID_ARGUMENT, error.what()};
      }
    }
    if (spec.argument_names_size() > 8 || spec.include_headers_size() > 16 ||
        (spec.has_debug_info() &&
         (spec.debug_info().ByteSizeLong() > 64 * 1024 ||
          spec.debug_info().module().empty() ||
          spec.debug_info().selector().empty() ||
          spec.debug_info().runtime_entry() == 0 ||
          spec.debug_info().entry_size() == 0 ||
          spec.debug_info().entry_size() >
              std::numeric_limits<std::uint64_t>::max() -
                  spec.debug_info().runtime_entry() ||
          spec.debug_info().record_calling_convention() <
              clangtracer::v1::RECORD_CALLING_CONVENTION_UNSPECIFIED ||
          spec.debug_info().record_calling_convention() >
              clangtracer::v1::RECORD_PASS_BY_REFERENCE ||
          spec.debug_info().fields_size() > 128 ||
          spec.debug_info().arguments_size() > 8))) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "invalid debug callable metadata"};
    }
    for (const auto &name : spec.argument_names()) {
      if (!valid_text(name, 63))
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "invalid debug argument name"};
    }
    for (const auto &header : spec.include_headers()) {
      if (!valid_text(header, 4095) || header.front() != '/' ||
          header.find_first_of("\"\r\n") != std::string::npos)
        return {grpc::StatusCode::INVALID_ARGUMENT, "invalid optional include"};
    }
    if (spec.has_header_context()) {
      const auto &context = spec.header_context();
      if (!spec.has_debug_info() || (!typed_prepared && spec.include_headers().empty()) ||
          !valid_text(context.working_directory(), 4095) ||
          context.working_directory().front() != '/' ||
          !valid_text(context.inference(), 4096, true) ||
          context.ByteSizeLong() > 64 * 1024 ||
          context.compiler_arguments_size() > 512)
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "invalid header compilation context"};
      for (const auto &argument : context.compiler_arguments())
        if (!valid_text(argument, 65536, true))
          return {grpc::StatusCode::INVALID_ARGUMENT,
                  "invalid header compiler argument"};
      const auto begin = context.compiler_arguments().begin();
      const auto end = context.compiler_arguments().end();
      if (typed_prepared ? !safe_prepared_callback_compiler_arguments(begin, end)
                         : !safe_preflight_compiler_arguments(begin, end, true))
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "header compiler arguments contain an unsupported preflight option"};
    }
    if (spec.has_receiver()) {
      const auto &receiver = spec.receiver();
      if (signature.kind() != clangtracer::v1::METHOD ||
          !spec.receiver_unavailable_reason().empty() ||
          receiver.ByteSizeLong() > 64 * 1024 ||
          !valid_text(receiver.translation_unit(), 4095) ||
          receiver.translation_unit().front() != '/' ||
          !valid_text(receiver.working_directory(), 4095) ||
          receiver.working_directory().front() != '/' ||
          !valid_text(receiver.record_name(), 4096) ||
          !valid_text(receiver.clang_version(), 256) ||
          !valid_text(receiver.target_triple(), 256) ||
          receiver.compiler_arguments_size() == 0 ||
          receiver.compiler_arguments_size() > 512 ||
          receiver.fields_size() > 128 || receiver.size() == 0 ||
          receiver.alignment() == 0) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "invalid Clang receiver context"};
      }
      std::size_t argument_bytes = 0;
      for (const auto &argument : receiver.compiler_arguments()) {
        if (!valid_text(argument, 65536, true)) {
          return {grpc::StatusCode::INVALID_ARGUMENT,
                  "invalid receiver compiler argument"};
        }
        argument_bytes += argument.size() + 1;
      }
      if (argument_bytes > 65536 || receiver.compiler_arguments(0).empty()) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "receiver compiler arguments exceed 64 KiB or omit driver"};
      }
      if (!safe_preflight_compiler_arguments(
              receiver.compiler_arguments().begin() + 1,
              receiver.compiler_arguments().end(), true)) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "receiver compiler arguments contain an unsupported preflight option"};
      }
      for (const auto &field : receiver.fields()) {
        if (!valid_text(field.name(), 4096, true) ||
            !valid_text(field.canonical_type(), 4096)) {
          return {grpc::StatusCode::INVALID_ARGUMENT,
                  "invalid receiver field metadata"};
        }
      }
    }
    const auto valid_value = [](clangtracer::v1::ValueType type,
                                bool allow_void) {
      return type >= clangtracer::v1::VOID &&
             (type <= (allow_void ? clangtracer::v1::POINTER
                                  : clangtracer::v1::OBJECT) ||
              type == clangtracer::v1::BOOL) &&
             (allow_void || type != clangtracer::v1::VOID);
    };
    const bool object_result_proven =
        signature.result() == clangtracer::v1::OBJECT &&
        spec.has_debug_info() && spec.debug_info().result_is_object_value() &&
        !spec.debug_info().result_is_reference() &&
        !spec.debug_info().result_cpp_type().empty() &&
        spec.debug_info().has_result_object_type() &&
        !spec.debug_info().result_object_type().record_name().empty();
    if ((!valid_value(signature.result(), true) && !object_result_proven) ||
        signature.arguments_size() > static_cast<int>(kMaxArguments)) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "signature has unsupported result or too many arguments"};
    }
    for (const int type_value : signature.arguments()) {
      const auto type = static_cast<clangtracer::v1::ValueType>(type_value);
      if (!valid_value(type, false)) {
        return {grpc::StatusCode::INVALID_ARGUMENT,
                "arguments must use supported scalar types"};
      }
    }
    return grpc::Status::OK;
  }

  static grpc::Status status_for(const std::exception &error) {
    const std::string message = error.what();
    if (dynamic_cast<const injector::IdlePointUnavailable *>(&error)) {
      return {grpc::StatusCode::FAILED_PRECONDITION, message};
    }
    if (message.find("already hooked") != std::string::npos ||
        message.find("overlaps an already") != std::string::npos) {
      return {grpc::StatusCode::ALREADY_EXISTS, message};
    }
    if (message.find("target symbol was not found") != std::string::npos) {
      return {grpc::StatusCode::NOT_FOUND, message};
    }
    if (message.find("retained-probe limit") != std::string::npos) {
      return {grpc::StatusCode::RESOURCE_EXHAUSTED, message};
    }
    if (message.find("unsupported or PC-relative") != std::string::npos ||
        message.find("CET landing") != std::string::npos) {
      return {grpc::StatusCode::FAILED_PRECONDITION, message};
    }
    if (message.find("agent rejected probe:") != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT, message};
    }
    if (message.find("does not exist") != std::string::npos) {
      return {grpc::StatusCode::NOT_FOUND, message};
    }
    if (message.find("target process is no longer available") !=
            std::string::npos ||
        message.find("PID was reused") != std::string::npos) {
      return {grpc::StatusCode::FAILED_PRECONDITION, message};
    }
    if (message.find("not attached") != std::string::npos ||
        message.find("failed state") != std::string::npos ||
        message.find("remains stopped") != std::string::npos ||
        message.find("left stopped") != std::string::npos) {
      return {grpc::StatusCode::FAILED_PRECONDITION, message};
    }
    if (message.find("timeout") != std::string::npos ||
        message.find("timed out") != std::string::npos ||
        message.find("idle point") != std::string::npos) {
      return {grpc::StatusCode::DEADLINE_EXCEEDED, message};
    }
    return {grpc::StatusCode::INTERNAL, message};
  }
};

} // namespace

int RunServer(const std::string &agent_path, const std::string &listen_uri,
              std::chrono::seconds operation_timeout,
              const std::string &context_durable_directory,
              EventHistoryLimits history_limits, ContextTransportBudget context_budget) {
  const auto stream_limits = context_delivery::stream_limits_for_context(context_budget);
  if (!history_limits.context_bytes) {
    history_limits.context_bytes = 16 * 1024 * 1024;
    if (context_budget.event_bytes > kDefaultContextEventBytes) {
      // The retained TraceEvent has the original event and projected fields.
      const auto envelope_bytes = 2ull * context_budget.event_bytes +
                                  stream_limits.max_epoch_schema_bytes + 4096;
      history_limits.context_bytes =
          std::max<std::size_t>(history_limits.context_bytes, envelope_bytes);
    }
  }
  if (history_limits.events == 0 || history_limits.context_events == 0 ||
      history_limits.context_bytes == 0 ||
      history_limits.context_bytes >
          std::numeric_limits<std::size_t>::max() / 2) {
    throw std::invalid_argument(
        "history limits must be positive and fit size_t / 2");
  }
  if (agent_path.empty() || operation_timeout.count() < 1 ||
      operation_timeout.count() > 300) {
    throw std::invalid_argument(
        "agent path and a timeout from 1 to 300 seconds are required");
  }
  const auto canonical_agent = std::filesystem::canonical(agent_path).string();
  PrivateDirectory directory;
  const auto endpoint =
      listen_uri.empty()
          ? "unix:///" + directory.path().substr(1) + "/control.sock"
          : listen_uri;
  const auto socket_path = unix_path_from_uri(endpoint);
  if (socket_path.empty() && !valid_loopback_uri(endpoint)) {
    throw std::invalid_argument("listen endpoint must be a Unix path or "
                                "explicit 127.0.0.1/::1 address");
  }
  if (socket_path.size() >= sizeof(((sockaddr_un *)nullptr)->sun_path)) {
    throw std::invalid_argument("Unix socket path is too long");
  }
  std::unique_ptr<EndpointLock> endpoint_lock;
  if (!socket_path.empty())
    endpoint_lock = std::make_unique<EndpointLock>(socket_path);
  const std::string event_path = directory.path() + "/events.sock";
  std::string durable_root;
  std::shared_ptr<DurableStoreCatalog> durable_catalog;
  if (!context_durable_directory.empty()) {
    std::filesystem::create_directories(context_durable_directory);
    durable_root = std::filesystem::canonical(context_durable_directory).string();
    durable_catalog = std::make_shared<DurableStoreCatalog>(
        std::filesystem::path(durable_root) / "collector",
        context_delivery::journal_limits_for_packet(stream_limits.max_packet_bytes,
                                                   stream_limits.max_epoch_schema_bytes));
  }
  TracerService service(canonical_agent, event_path, directory.path(),
                        operation_timeout, endpoint, durable_root,
                        history_limits, context_budget, !socket_path.empty());
  LiveContextCapture live_context;
  context_delivery::ContextCollector context_collector(
      {}, stream_limits,
      [&service, &live_context](const auto &open, const auto &envelope, auto decoded) {
        return service.publish_context(open, envelope) &&
               live_context.accept(open, envelope, std::move(decoded));
      },
      [catalog = durable_catalog](std::string_view recording_id,
                                 std::string_view sink_id) {
        return catalog ? catalog->acquire(recording_id, sink_id) : nullptr;
      });
  grpc::ServerBuilder builder;
  builder.SetMaxSendMessageSize(-1);
  builder.SetMaxReceiveMessageSize(-1);
  int selected_port = 0;
  builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials(),
                           &selected_port);
  builder.RegisterService(&service);
  builder.RegisterService(context_collector.service());
  auto server = builder.BuildAndStart();
  if (!server) {
    throw std::runtime_error(
        "gRPC server could not bind the requested endpoint");
  }
  std::string actual_endpoint = endpoint;
  if (socket_path.empty() && selected_port > 0) {
    const auto separator = endpoint.rfind(':');
    if (separator != std::string::npos)
      actual_endpoint = endpoint.substr(0, separator + 1) +
                        std::to_string(selected_port);
  }
  service.set_context_endpoint(std::move(actual_endpoint));
  if (!socket_path.empty()) {
    struct stat socket_info{};
    if (::lstat(socket_path.c_str(), &socket_info) != 0 ||
        !S_ISSOCK(socket_info.st_mode)) {
      server->Shutdown(std::chrono::system_clock::now() +
                       std::chrono::seconds(2));
      server->Wait();
      throw std::runtime_error(
          "gRPC server did not create the requested Unix socket");
    }
  }
  if (!socket_path.empty() && ::chmod(socket_path.c_str(), 0600) != 0) {
    server->Shutdown(std::chrono::system_clock::now() +
                     std::chrono::seconds(2));
    server->Wait();
    throw std::runtime_error("cannot secure Unix server socket");
  }
  std::cerr << "clang-tracer gRPC server listening on " << endpoint << '\n';
  const auto previous_int = std::signal(SIGINT, signal_handler);
  const auto previous_term = std::signal(SIGTERM, signal_handler);
  std::thread signal_watcher([&] {
    while (interrupted == 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server->Shutdown(std::chrono::system_clock::now() +
                     std::chrono::seconds(5));
  });
  server->Wait();
  interrupted = 1;
  if (signal_watcher.joinable())
    signal_watcher.join();
  std::signal(SIGINT, previous_int);
  std::signal(SIGTERM, previous_term);
  const bool clean_shutdown = service.shutdown_sessions();
  if (clean_shutdown)
    std::cerr << "clang-tracer shutdown complete\n";
  return clean_shutdown ? 0 : 1;
}

} // namespace tracer

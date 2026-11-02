#include "server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "session.hpp"
#include "debug_resolver.hpp"
#include "header_context.hpp"
#include "injector.hpp"
#include "context_collector.hpp"
#include "context_journal.hpp"
#include "context_package.hpp"

namespace tracer {
namespace {

constexpr std::size_t kSessionLimit = 64;
constexpr std::size_t kMaxCallbackSource = 64 * 1024;
constexpr std::size_t kMaxEventBytes = 64 * 1024;

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
  explicit DurableStoreCatalog(std::filesystem::path root)
      : root_(std::move(root)) {}

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
        directory);
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
  if (encoded.empty() || encoded.size() > 65536 || encoded.size() % 2 != 0)
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
  struct ProbeIdentity {
    std::uint32_t pid = 0;
    std::string function_name;
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
                 std::uint32_t pid, std::string function_name, bool active) {
    std::lock_guard lock(mutex_);
    const auto key = std::make_pair(session_id, probe_id);
    if (active) {
      const auto existing = active_probes_.find(key);
      if (existing != active_probes_.end()) {
        function_name = existing->second.function_name;
      } else if (const auto known = known_probes_.find(key);
                 known != known_probes_.end()) {
        function_name = known->second.function_name;
      }
      ProbeIdentity identity{pid, std::move(function_name)};
      active_probes_[key] = identity;
      constexpr std::size_t kKnownProbeLimit = 4096;
      bool retain_probe_identity = true;
      if (!known_probes_.contains(key) &&
          known_probes_.size() >= kKnownProbeLimit) {
        const auto stale = std::find_if(known_probes_.begin(), known_probes_.end(),
            [&](const auto &entry) { return !active_probes_.contains(entry.first); });
        if (stale != known_probes_.end())
          known_probes_.erase(stale);
        else
          retain_probe_identity = false;
      }
      if (retain_probe_identity)
        known_probes_[key] = std::move(identity);
      received_.try_emplace(key, 0);
    } else {
      active_probes_.erase(key);
      const auto found = received_.find(key);
      if (found != received_.end() && found->second == 0)
        received_.erase(found);
    }
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
      switch (in.value_case()) {
      case clangtracer::context::v1::TypedTraceValue::kSignedValue:
        out->set_signed_value(in.signed_value());
        return true;
      case clangtracer::context::v1::TypedTraceValue::kUnsignedValue:
        out->set_unsigned_value(in.unsigned_value());
        return true;
      case clangtracer::context::v1::TypedTraceValue::kRealValue:
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
      if (in.requested_depth() < 1 || in.requested_depth() > kMaxStackDepth ||
          in.frames_size() > static_cast<int>(in.requested_depth()) ||
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
            const auto bytes = event.ByteSizeLong();
            if (!batch.empty() && batch_bytes + bytes > 32 * 1024 * 1024)
              break;
            batch.push_back(event);
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
      const auto charge = event.ByteSizeLong() + 16;
      if (charge > 31 * 1024 * 1024) {
        return {grpc::StatusCode::RESOURCE_EXHAUSTED,
                "event exceeds drain response budget"};
      }
      if (bytes + charge > 31 * 1024 * 1024) {
        break;
      }
      response.add_events()->CopyFrom(event);
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
  std::deque<clangtracer::v1::TraceEvent> history_;
  std::size_t context_bytes_ = 0;
  std::size_t context_event_count_ = 0;
  std::uint64_t next_sequence_ = 1;
  std::uint64_t dropped_ = 0;

  void receive_loop() {
    std::array<char, kMaxEventBytes> bytes{};
    while (!stopping_.load()) {
      const auto count = ::recv(fd_, bytes.data(), bytes.size(), 0);
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
      clangtracer::v1::TraceEvent event;
      if (!event.ParseFromArray(bytes.data(), static_cast<int>(count)) ||
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
            stack.requested_depth() <= kMaxStackDepth &&
            (stack.application_only() || stack.frames_size() >= 1) &&
            stack.frames_size() <= static_cast<int>(stack.requested_depth()) &&
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
      std::lock_guard lock(mutex_);
      const auto active =
          active_probes_.find({event.session_id(), event.probe_id()});
      if (active == active_probes_.end() || active->second.pid != event.pid())
        continue;
      // Function identity comes from the registered probe, not the agent packet.
      event.clear_function_name();
      event.set_function_name(active->second.function_name);
      ++received_[active->first];
      event.set_sequence(next_sequence_++);
      event.set_server_instance_id(instance_id_);
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
      history_.push_back(std::move(event));
      changed_.notify_all();
    }
  }
};

class TracerService final : public clangtracer::v1::Tracer::Service {
public:
  TracerService(std::string agent_path, std::string event_path,
                std::string source_directory, std::chrono::seconds timeout,
                std::string context_endpoint,
                std::string context_durable_directory,
                EventHistoryLimits limits)
      : agent_path_(std::move(agent_path)), event_path_(std::move(event_path)),
        source_directory_(std::move(source_directory)), timeout_(timeout),
        context_endpoint_(std::move(context_endpoint)),
        context_durable_directory_(std::move(context_durable_directory)),
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
        timeout_);
    try {
      session->attach();
      std::lock_guard lock(mutex_);
      pending_pids_.erase(pid);
      sessions_.emplace(id, session);
      active_by_pid_[pid] = id;
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

  grpc::Status ResolveProbe(grpc::ServerContext *,
                            const clangtracer::v1::ResolveProbeRequest *request,
                            clangtracer::v1::ProbeSpec *response) override {
    if (request->selector().empty() || request->selector().size() > 4096 ||
        request->selector().find('\0') != std::string::npos)
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid C++ selector"};
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    session->refresh_process_state();
    if (session->snapshot().state() != clangtracer::v1::Session::ATTACHED)
      return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
    try {
      auto resolved =
          client::resolve_debug_callable(session->pid(), request->selector());
      for (const auto &header : request->include_headers())
        resolved.spec.add_include_headers(header);
      client::resolve_header_context(resolved.spec,
                                     request->compilation_database());
      response->Swap(&resolved.spec);
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      return {grpc::StatusCode::INVALID_ARGUMENT, error.what()};
    } catch (const std::exception &error) {
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
    std::uint64_t pending_probe_id = 0;
    try {
      pending_probe_id = next_probe_id_.fetch_add(1);
      if (pending_probe_id == 0)
        pending_probe_id = next_probe_id_.fetch_add(1);
      const auto &spec = request->spec();
      const auto &selector = spec.debug_info().selector();
      const auto function_name =
          spec.has_debug_info() && !selector.empty() && selector.size() <= 4096 &&
                  selector.find('\0') == std::string::npos
              ? selector
              : spec.symbol();
      events_.set_probe(session->id(), pending_probe_id, session->pid(),
                        function_name, true);
      const auto lineage = admit_probe_package(request->spec());
      if (!lineage.retained) {
        std::cerr << "clang-tracer: shared-context source lineage isolated: "
                  << lineage.diagnostic.substr(0, 240) << '\n';
      }
      auto probe = session->add_probe(request->spec(), source_directory_,
                                      pending_probe_id, lineage.id);
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

  grpc::Status RemoveProbe(grpc::ServerContext *,
                           const clangtracer::v1::RemoveProbeRequest *request,
                           clangtracer::v1::Probe *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
      auto probe = session->remove_probe(request->probe_id());
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

  grpc::Status Detach(grpc::ServerContext *,
                      const clangtracer::v1::DetachRequest *request,
                      clangtracer::v1::Session *response) override {
    auto session = find_session(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    std::lock_guard operation_lock(session->operation_mutex());
    try {
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

  grpc::Status ListSymbols(
      grpc::ServerContext *context,
      const clangtracer::v1::ListSymbolsRequest *request,
      clangtracer::v1::ListSymbolsResponse *response) override {
    if (request->session_id().empty() || request->session_id().size() > 128 ||
        request->session_id().find('\0') != std::string::npos ||
        request->query().size() > 4096 || request->query().find('\0') != std::string::npos ||
        request->module().size() > 4096 || request->module().find('\0') != std::string::npos ||
        request->page_token().size() > 65536 ||
        request->page_token().find('\0') != std::string::npos) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "invalid symbol listing request"};
    }
    const std::size_t page_size = request->page_size() == 0 ? 100 : request->page_size();
    if (page_size > 500) {
      return {grpc::StatusCode::INVALID_ARGUMENT,
              "symbol page size must be between 1 and 500"};
    }
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

    try {
      const auto page = client::list_debug_symbols(
          session->pid(), request->query(), request->module(), page_size,
          continuation.last_selector(), continuation.last_linkage_symbol(),
          continuation.last_module_path(), continuation.mapping_fingerprint(),
          [&] { return context->IsCancelled(); });
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
      }
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
      if (response->ByteSizeLong() > 8 * 1024 * 1024) {
        return {grpc::StatusCode::RESOURCE_EXHAUSTED,
                "symbol page exceeds the 8 MiB response budget; reduce page size"};
      }
      return grpc::Status::OK;
    } catch (const std::invalid_argument &error) {
      const std::string detail = error.what();
      if (detail.find("mapped images changed") != std::string::npos) {
        return {grpc::StatusCode::ABORTED, detail};
      }
      return {grpc::StatusCode::INVALID_ARGUMENT, detail};
    } catch (const std::exception &error) {
      const std::string detail = error.what();
      if (detail.find("work limit exceeded") != std::string::npos) {
        return {grpc::StatusCode::RESOURCE_EXHAUSTED, detail};
      }
      return status_for(error);
    }
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
        if (!probe.active() &&
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
  std::string agent_path_;
  std::string event_path_;
  std::string source_directory_;
  std::chrono::seconds timeout_;
  std::string context_endpoint_;
  std::string context_durable_directory_;
  std::condition_variable context_endpoint_cv_;
  bool context_endpoint_ready_ = false;
  EventHub events_;
  std::atomic<std::uint64_t> next_probe_id_;
  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<Session>> sessions_;
  std::map<std::uint32_t, std::string> active_by_pid_;
  std::set<std::uint32_t> pending_pids_;

  std::shared_ptr<Session> find_session(const std::string &id) {
    std::lock_guard lock(mutex_);
    const auto found = sessions_.find(id);
    return found == sessions_.end() ? nullptr : found->second;
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

  void sync_event_probes(const std::shared_ptr<Session> &session,
                         std::uint64_t pending_probe_id = 0) {
    const auto snapshot = session->snapshot();
    if (pending_probe_id &&
        std::none_of(
            snapshot.probes().begin(), snapshot.probes().end(),
            [&](const auto &probe) { return probe.id() == pending_probe_id; }))
      events_.set_probe(session->id(), pending_probe_id, session->pid(), {}, false);
    for (const auto &probe : snapshot.probes()) {
      if (probe.active())
        events_.set_probe(probe.session_id(), probe.id(), session->pid(),
                          probe.symbol(), true);
      else
        (void)events_.retire_probe(probe.session_id(), probe.id(),
                                   session->events_sent(probe.id()),
                                   std::chrono::milliseconds(0));
    }
  }

  grpc::Status validate_probe(const clangtracer::v1::ProbeSpec &spec) const {
    if (spec.stack_depth() > tracer::kMaxStackDepth)
      return {grpc::StatusCode::INVALID_ARGUMENT, "stack depth must be between 0 and 64"};
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
      if (!spec.has_debug_info() || spec.include_headers().empty() ||
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
             type <= (allow_void ? clangtracer::v1::POINTER
                                 : clangtracer::v1::OBJECT) &&
             (allow_void || type != clangtracer::v1::VOID);
    };
    if (!valid_value(signature.result(), true) ||
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
    if (dynamic_cast<const tracer::injector::IdlePointUnavailable *>(&error)) {
      return {grpc::StatusCode::FAILED_PRECONDITION, error.what()};
    }
    const std::string message = error.what();
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
              EventHistoryLimits history_limits) {
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
        std::filesystem::path(durable_root) / "collector");
  }
  TracerService service(canonical_agent, event_path, directory.path(),
                        operation_timeout, endpoint, durable_root,
                        history_limits);
  LiveContextCapture live_context;
  context_delivery::ContextCollector context_collector(
      {}, {},
      [&service, &live_context](const auto &open, const auto &envelope, auto decoded) {
        return service.publish_context(open, envelope) &&
               live_context.accept(open, envelope, std::move(decoded));
      },
      [catalog = durable_catalog](std::string_view recording_id,
                                 std::string_view sink_id) {
        return catalog ? catalog->acquire(recording_id, sink_id) : nullptr;
      });
  grpc::ServerBuilder builder;
  builder.SetMaxSendMessageSize(32 * 1024 * 1024);
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

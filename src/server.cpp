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
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <grpcpp/grpcpp.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "session.hpp"

namespace tracer {
namespace {

constexpr std::size_t kSessionLimit = 64;
constexpr std::size_t kEventHistoryLimit = 4096;
constexpr std::size_t kMaxCallbackSource = 64 * 1024;
constexpr std::size_t kMaxEventBytes = 64 * 1024;

volatile std::sig_atomic_t interrupted = 0;

void signal_handler(int) { interrupted = 1; }

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
    if (fd_ < 0 || ::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
      if (fd_ >= 0)
        ::close(fd_);
      fd_ = -1;
      throw std::runtime_error(
          "another clang-tracer server owns this endpoint");
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

class EventHub final {
public:
  explicit EventHub(std::string path) : path_(std::move(path)) {
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
                 std::uint32_t pid, bool active) {
    std::lock_guard lock(mutex_);
    const auto key = std::make_pair(session_id, probe_id);
    if (active) {
      active_probes_[key] = pid;
      received_.try_emplace(key, 0);
    } else {
      active_probes_.erase(key);
      const auto found = received_.find(key);
      if (found != received_.end() && found->second == 0)
        received_.erase(found);
    }
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
    std::uint64_t cursor = request->after_sequence();
    for (;;) {
      std::vector<clangtracer::v1::TraceEvent> batch;
      {
        std::unique_lock lock(mutex_);
        if (context->IsCancelled())
          return true;
        if (cursor != 0 && !history_.empty() &&
            history_.front().sequence() > cursor + 1)
          return false;
        bool found = false;
        for (const auto &event : history_) {
          if (event.sequence() <= cursor)
            continue;
          cursor = event.sequence();
          found = true;
          if (filter.empty() || filter.count(event.session_id()))
            batch.push_back(event);
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
  int fd_ = -1;
  std::thread receiver_;
  std::atomic<bool> stopping_{false};
  std::mutex mutex_;
  std::condition_variable changed_;
  std::map<std::pair<std::string, std::uint64_t>, std::uint32_t> active_probes_;
  std::map<std::pair<std::string, std::uint64_t>, std::uint64_t> received_;
  std::deque<clangtracer::v1::TraceEvent> history_;
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
      std::lock_guard lock(mutex_);
      const auto active =
          active_probes_.find({event.session_id(), event.probe_id()});
      if (active == active_probes_.end() || active->second != event.pid())
        continue;
      ++received_[active->first];
      event.set_sequence(next_sequence_++);
      if (history_.size() == kEventHistoryLimit) {
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
                std::string source_directory, std::chrono::seconds timeout)
      : agent_path_(std::move(agent_path)), event_path_(std::move(event_path)),
        source_directory_(std::move(source_directory)), timeout_(timeout),
        events_(event_path_), next_probe_id_(random_probe_seed()) {}

  grpc::Status Attach(grpc::ServerContext *,
                      const clangtracer::v1::AttachRequest *request,
                      clangtracer::v1::Session *response) override {
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
    auto session = std::make_shared<Session>(
        id, pid, process_start, agent_path_, event_path_, timeout_);
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
      events_.set_probe(session->id(), pending_probe_id, session->pid(), true);
      auto probe = session->add_probe(request->spec(), source_directory_,
                                      pending_probe_id);
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

  grpc::Status WatchEvents(
      grpc::ServerContext *context,
      const clangtracer::v1::WatchEventsRequest *request,
      grpc::ServerWriter<clangtracer::v1::TraceEvent> *writer) override {
    if (!events_.stream(context, request, writer)) {
      return {grpc::StatusCode::OUT_OF_RANGE,
              "event cursor is older than retained history"};
    }
    return grpc::Status::OK;
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
      events_.set_probe(session->id(), pending_probe_id, session->pid(), false);
    for (const auto &probe : snapshot.probes()) {
      if (probe.active())
        events_.set_probe(probe.session_id(), probe.id(), session->pid(), true);
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
              std::chrono::seconds operation_timeout) {
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
  TracerService service(canonical_agent, event_path, directory.path(),
                        operation_timeout);
  grpc::ServerBuilder builder;
  int selected_port = 0;
  builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials(),
                           &selected_port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  if (!server) {
    throw std::runtime_error(
        "gRPC server could not bind the requested endpoint");
  }
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

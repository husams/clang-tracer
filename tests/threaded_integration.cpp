#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <grpcpp/grpcpp.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/linux_syscalls.hpp"
#include "tracer.grpc.pb.h"
#include "tracer/compat.hpp"

extern char **environ;

namespace {

using namespace std::chrono_literals;
namespace api = clangtracer::v1;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void set_deadline(grpc::ClientContext &context) {
  context.set_deadline(std::chrono::system_clock::now() + 20s);
}

class ChildProcess {
public:
  explicit ChildProcess(pid_t pid) : pid_(pid) {}

  ~ChildProcess() { stop(); }

  ChildProcess(const ChildProcess &) = delete;
  ChildProcess &operator=(const ChildProcess &) = delete;

  pid_t pid() const { return pid_; }

  bool running() {
    if (pid_ <= 0) {
      return false;
    }
    int status = 0;
    const auto result = ::waitpid(pid_, &status, WNOHANG);
    if (result == pid_ || (result < 0 && errno == ECHILD)) {
      pid_ = -1;
      return false;
    }
    return result == 0;
  }

  void stop(std::chrono::milliseconds grace = 5s) noexcept {
    if (pid_ <= 0) {
      return;
    }
    if (::kill(pid_, SIGTERM) != 0 && errno != ESRCH) {
      pid_ = -1;
      return;
    }
    const auto attempts = std::max<std::int64_t>(1, grace.count() / 20);
    for (std::int64_t attempt = 0; attempt < attempts; ++attempt) {
      int status = 0;
      const auto result = ::waitpid(pid_, &status, WNOHANG);
      if (result == pid_ || (result < 0 && errno == ECHILD)) {
        pid_ = -1;
        return;
      }
      std::this_thread::sleep_for(20ms);
    }
    (void)::kill(pid_, SIGKILL);
    int status = 0;
    (void)::waitpid(pid_, &status, 0);
    pid_ = -1;
  }

private:
  pid_t pid_ = -1;
};

ChildProcess spawn(const std::vector<std::string> &arguments,
                   const std::filesystem::path &log_path) {
  posix_spawn_file_actions_t actions;
  require(::posix_spawn_file_actions_init(&actions) == 0,
          "could not initialize spawn file actions");
  int result = ::posix_spawn_file_actions_addopen(
      &actions, STDOUT_FILENO, log_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
      0600);
  if (result == 0) {
    result = ::posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO,
                                                STDERR_FILENO);
  }
  if (result != 0) {
    ::posix_spawn_file_actions_destroy(&actions);
    throw std::runtime_error("could not configure child output redirection");
  }
  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1);
  for (const auto &argument : arguments) {
    argv.push_back(const_cast<char *>(argument.c_str()));
  }
  argv.push_back(nullptr);
  pid_t pid = -1;
  result = ::posix_spawn(&pid, argv.front(), &actions, nullptr, argv.data(),
                         ::environ);
  ::posix_spawn_file_actions_destroy(&actions);
  require(result == 0, "could not start child process (error " +
                           std::to_string(result) + ")");
  return ChildProcess(pid);
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void report(std::ofstream &output, const std::string &line) {
  output << line << '\n';
  output.flush();
}

void save_proto(const std::filesystem::path &path,
                const google::protobuf::MessageLite &message) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  require(static_cast<bool>(output) && message.SerializeToOstream(&output),
          "cannot write protobuf evidence " + path.string());
}

struct TargetInfo {
  std::uintptr_t free_entry = 0;
  std::uintptr_t method_entry = 0;
  std::uintptr_t callback_gate = 0;
  std::uintptr_t callbacks_active = 0;
  bool busy_main = false;
  std::string mode;
};

std::optional<TargetInfo> find_target_info(const std::filesystem::path &path) {
  std::istringstream input(read_file(path));
  std::string line;
  while (std::getline(input, line)) {
    long pid = 0;
    void *free_entry = nullptr;
    void *method_entry = nullptr;
    void *callback_gate = nullptr;
    void *callbacks_active = nullptr;
    int busy_main = 0;
    char mode[16]{};
    if (std::sscanf(line.c_str(),
                    "TARGET pid=%ld free=%p method=%p gate=%p active=%p "
                    "busy=%d mode=%15s",
                    &pid, &free_entry, &method_entry, &callback_gate,
                    &callbacks_active, &busy_main, mode) == 7 &&
        free_entry && method_entry && callback_gate && callbacks_active) {
      return TargetInfo{reinterpret_cast<std::uintptr_t>(free_entry),
                        reinterpret_cast<std::uintptr_t>(method_entry),
                        reinterpret_cast<std::uintptr_t>(callback_gate),
                        reinterpret_cast<std::uintptr_t>(callbacks_active),
                        busy_main != 0,
                        mode};
    }
  }
  return std::nullopt;
}

TargetInfo wait_target_info(const std::filesystem::path &path,
                            std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (auto info = find_target_info(path)) {
      return *info;
    }
    std::this_thread::sleep_for(20ms);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("threaded target did not report its entry points");
}

std::vector<unsigned char>
read_process_memory(pid_t pid, std::uintptr_t address, std::size_t size) {
  std::vector<unsigned char> bytes(size);
  iovec local{bytes.data(), bytes.size()};
  iovec remote{reinterpret_cast<void *>(address), bytes.size()};
  require(::tracer::linux_api::process_vm_readv(pid, &local, 1, &remote, 1, 0) ==
              static_cast<ssize_t>(size),
          "could not read target function bytes");
  return bytes;
}

void write_process_memory(pid_t pid, std::uintptr_t address, const void *data,
                          std::size_t size) {
  iovec local{const_cast<void *>(data), size};
  iovec remote{reinterpret_cast<void *>(address), size};
  require(::tracer::linux_api::process_vm_writev(pid, &local, 1, &remote, 1, 0) ==
              static_cast<ssize_t>(size),
          "could not write target process memory");
}

std::uint32_t read_process_u32(pid_t pid, std::uintptr_t address) {
  const auto bytes = read_process_memory(pid, address, sizeof(std::uint32_t));
  std::uint32_t value = 0;
  std::memcpy(&value, bytes.data(), sizeof(value));
  return value;
}

class ScopedCallbackGate {
public:
  ScopedCallbackGate(pid_t pid, std::uintptr_t address)
      : pid_(pid), address_(address) {}

  ~ScopedCallbackGate() { release(); }

  ScopedCallbackGate(const ScopedCallbackGate &) = delete;
  ScopedCallbackGate &operator=(const ScopedCallbackGate &) = delete;

  void arm() {
    const std::uint32_t value = 1;
    write_process_memory(pid_, address_, &value, sizeof(value));
    armed_ = true;
  }

  void release() noexcept {
    if (!armed_) {
      return;
    }
    const std::uint32_t value = 0;
    iovec local{const_cast<std::uint32_t *>(&value), sizeof(value)};
    iovec remote{reinterpret_cast<void *>(address_), sizeof(value)};
    (void)::tracer::linux_api::process_vm_writev(pid_, &local, 1, &remote, 1, 0);
    armed_ = false;
  }

private:
  pid_t pid_;
  std::uintptr_t address_;
  bool armed_ = false;
};

void wait_for_active_callbacks(pid_t pid, std::uintptr_t active_address,
                               std::uint32_t minimum,
                               std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (read_process_u32(pid, active_address) >= minimum) {
      return;
    }
    std::this_thread::sleep_for(5ms);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("four callbacks did not enter the armed gate");
}

struct WorkerSnapshot {
  std::uint32_t tid = 0;
  std::uintptr_t object_address = 0;
  std::uint64_t calls = 0;
  std::uint64_t failures = 0;
};

using WorkerSnapshots = std::array<WorkerSnapshot, 4>;

struct MainSnapshot {
  std::uint64_t calls = 0;
  std::uint64_t failures = 0;
};

void report_main_checkpoint(std::ofstream &output, const std::string &label,
                            bool busy_main,
                            const std::optional<MainSnapshot> &snapshot) {
  if (!busy_main) {
    return;
  }
  require(snapshot.has_value(), label + " has no busy-main progress snapshot");
  output << "busy-main checkpoint=" << label << " calls=" << snapshot->calls
         << " failures=" << snapshot->failures << '\n';
  output.flush();
}

std::optional<MainSnapshot>
parse_main_snapshot(const std::filesystem::path &path) {
  std::istringstream input(read_file(path));
  std::string line;
  std::optional<MainSnapshot> latest;
  while (std::getline(input, line)) {
    unsigned long long calls = 0;
    unsigned long long failures = 0;
    if (std::sscanf(line.c_str(), "MAIN calls=%llu failures=%llu", &calls,
                    &failures) == 2) {
      latest = MainSnapshot{calls, failures};
    }
  }
  return latest;
}

MainSnapshot wait_for_main_progress(const std::filesystem::path &path,
                                    const std::optional<MainSnapshot> &after,
                                    std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  auto baseline = after ? after->calls : 0;
  if (after) {
    if (const auto current = parse_main_snapshot(path)) {
      baseline = std::max(baseline, current->calls);
    }
  }
  do {
    if (auto snapshot = parse_main_snapshot(path)) {
      require(snapshot->failures == 0,
              "busy main thread reported a calculation or errno failure");
      if (snapshot->calls > baseline) {
        return *snapshot;
      }
    }
    std::this_thread::sleep_for(10ms);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("busy main thread did not advance its heartbeat");
}

std::uint64_t read_main_thread_signal_mask(pid_t pid) {
  const auto status_path = std::filesystem::path("/proc") /
                           std::to_string(pid) / "task" / std::to_string(pid) /
                           "status";
  std::ifstream status(status_path);
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("SigBlk:", 0) == 0) {
      unsigned long long mask = 0;
      require(std::sscanf(line.c_str(), "SigBlk:%llx", &mask) == 1,
              "could not parse main-thread SigBlk mask");
      return static_cast<std::uint64_t>(mask);
    }
  }
  throw std::runtime_error("could not read main-thread SigBlk mask");
}

void require_main_signal_mask(pid_t pid, std::uint64_t expected,
                              const std::string &checkpoint) {
  const auto actual = read_main_thread_signal_mask(pid);
  std::ostringstream message;
  message << checkpoint << " changed the main-thread signal mask from 0x"
          << std::hex << expected << " to 0x" << actual;
  require(actual == expected, message.str());
}

std::optional<WorkerSnapshots>
parse_worker_snapshots(const std::filesystem::path &path) {
  const auto contents = read_file(path);
  std::istringstream input(contents);
  std::string line;
  WorkerSnapshots snapshots{};
  std::array<bool, 4> seen{};
  while (std::getline(input, line)) {
    std::size_t id = 0;
    unsigned tid = 0;
    void *object_address = nullptr;
    unsigned long long calls = 0;
    unsigned long long failures = 0;
    if (std::sscanf(line.c_str(),
                    "WORKER id=%zu tid=%u object=%p calls=%llu failures=%llu",
                    &id, &tid, &object_address, &calls, &failures) == 5 &&
        id >= 1 && id <= 4) {
      snapshots[id - 1] = {tid,
                           reinterpret_cast<std::uintptr_t>(object_address),
                           calls, failures};
      seen[id - 1] = true;
    }
  }
  if (std::all_of(seen.begin(), seen.end(), [](bool value) { return value; })) {
    return snapshots;
  }
  return std::nullopt;
}

WorkerSnapshots
wait_for_worker_progress(const std::filesystem::path &path,
                         const std::optional<WorkerSnapshots> &after,
                         std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (auto snapshot = parse_worker_snapshots(path)) {
      bool ready = true;
      for (std::size_t index = 0; index < snapshot->size(); ++index) {
        ready &= (*snapshot)[index].tid > 0 &&
                 (*snapshot)[index].object_address != 0 &&
                 (*snapshot)[index].failures == 0 &&
                 (!after || (*snapshot)[index].calls > (*after)[index].calls);
      }
      if (ready) {
        return *snapshot;
      }
    }
    std::this_thread::sleep_for(25ms);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error(after ? "one or more stable workers did not resume "
                                   "correct progress"
                                 : "stable workers did not start correctly");
}

std::set<pid_t> read_target_tids(pid_t pid) {
  std::set<pid_t> tids;
  const auto task_path =
      std::filesystem::path("/proc") / std::to_string(pid) / "task";
  for (const auto &entry : std::filesystem::directory_iterator(task_path)) {
    try {
      tids.insert(
          static_cast<pid_t>(std::stol(entry.path().filename().string())));
    } catch (const std::exception &) {
    }
  }
  return tids;
}

void require_all_threads_untraced(pid_t pid) {
  const auto tids = read_target_tids(pid);
  require(tids.size() >= 5, "threaded target lost its worker threads");
  std::size_t checked = 0;
  for (const auto tid : tids) {
    const auto task_path = std::filesystem::path("/proc") /
                           std::to_string(pid) / "task" / std::to_string(tid);
    std::ifstream status(task_path / "status");
    std::string line;
    bool found = false;
    while (std::getline(status, line)) {
      if (line.rfind("TracerPid:", 0) == 0) {
        int tracer_pid = -1;
        require(std::sscanf(line.c_str(), "TracerPid:%d", &tracer_pid) == 1,
                "invalid TracerPid for TID " + std::to_string(tid));
        require(tracer_pid == 0,
                "TID " + std::to_string(tid) + " remains ptrace-traced");
        found = true;
        break;
      }
    }
    if (!found) {
      // A short-lived worker may exit after the task directory snapshot.
      std::error_code error;
      const bool still_exists = std::filesystem::exists(task_path, error);
      if (!still_exists && !error) {
        continue;
      }
    }
    require(found, "could not read TracerPid for TID " + std::to_string(tid));
    ++checked;
  }
  require(checked >= 5, "too few live target threads checked after detach");
}

std::size_t count_server_module_fds(
    pid_t server_pid,
    const std::vector<std::filesystem::path> &application_modules) {
  std::set<std::filesystem::path> expected;
  for (const auto &module : application_modules) {
    expected.insert(std::filesystem::canonical(module));
  }
  std::size_t count = 0;
  const auto fd_directory =
      std::filesystem::path("/proc") / std::to_string(server_pid) / "fd";
  for (const auto &entry : std::filesystem::directory_iterator(fd_directory)) {
    std::error_code error;
    const auto target = std::filesystem::read_symlink(entry.path(), error);
    if (error) {
      continue;
    }
    const auto canonical = std::filesystem::weakly_canonical(target, error);
    if (!error && ::tracer::compat::contains(expected, canonical)) {
      ++count;
    }
  }
  return count;
}

void wait_for_server_module_fd_count(
    pid_t server_pid,
    const std::vector<std::filesystem::path> &application_modules,
    std::size_t expected_count, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (count_server_module_fds(server_pid, application_modules) ==
        expected_count) {
      return;
    }
    std::this_thread::sleep_for(10ms);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("server retained an unexpected number of discovered "
                           "application-module file descriptors");
}

void require_session_modules(
    const api::Session &session,
    const std::vector<std::filesystem::path> &expected_modules,
    const std::string &label) {
  std::set<std::filesystem::path> discovered;
  for (const auto &module : session.application_modules()) {
    const std::filesystem::path path(module);
    require(path.is_absolute(), label + " returned a non-absolute module path");
    discovered.insert(std::filesystem::canonical(path));
    const auto name = path.filename().string();
    require(name.find("libc.so") == std::string::npos &&
                name.find("ld-linux") == std::string::npos,
            label + " included a protected runtime module");
  }
  for (const auto &module : expected_modules) {
    require(::tracer::compat::contains(discovered, std::filesystem::canonical(module)),
            label + " did not discover expected DSO " + module.string());
  }
}

std::vector<std::filesystem::path>
session_module_paths(const api::Session &session) {
  std::vector<std::filesystem::path> modules;
  modules.reserve(session.application_modules_size());
  for (const auto &module : session.application_modules()) {
    modules.emplace_back(module);
  }
  return modules;
}

void require_session_history(const api::Session &attached,
                             const api::Session &detached) {
  require(detached.application_modules_size() ==
              attached.application_modules_size(),
          "Detach discarded discovered-module session history");
  for (int index = 0; index < attached.application_modules_size(); ++index) {
    require(detached.application_modules(index) ==
                attached.application_modules(index),
            "Detach changed discovered-module session history");
  }
}

api::Session attach(api::Tracer::Stub &stub, pid_t pid,
                    const std::filesystem::path &evidence,
                    const std::string &label) {
  api::AttachRequest request;
  request.set_pid(static_cast<std::uint32_t>(pid));
  api::Session response;
  grpc::ClientContext context;
  set_deadline(context);
  const auto status = stub.Attach(&context, request, &response);
  require(status.ok(), label + " Attach failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  return response;
}

api::ProbeSpec make_spec(const std::string &symbol, api::CallableKind kind,
                         const std::string &callback) {
  api::ProbeSpec spec;
  spec.set_symbol(symbol);
  spec.set_callback_source(callback);
  spec.mutable_signature()->set_kind(kind);
  spec.mutable_signature()->set_result(api::INT32);
  spec.mutable_signature()->add_arguments(api::INT32);
  return spec;
}

api::Probe add_probe(api::Tracer::Stub &stub, const std::string &session_id,
                     const api::ProbeSpec &spec,
                     const std::filesystem::path &evidence,
                     const std::string &label) {
  api::AddProbeRequest request;
  request.set_session_id(session_id);
  *request.mutable_spec() = spec;
  api::Probe response;
  grpc::ClientContext context;
  set_deadline(context);
  const auto status = stub.AddProbe(&context, request, &response);
  require(status.ok(), label + " AddProbe failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  return response;
}

api::Probe remove_probe(api::Tracer::Stub &stub, const std::string &session_id,
                        std::uint64_t probe_id,
                        const std::filesystem::path &evidence,
                        const std::string &label) {
  api::RemoveProbeRequest request;
  request.set_session_id(session_id);
  request.set_probe_id(probe_id);
  api::Probe response;
  grpc::ClientContext context;
  set_deadline(context);
  const auto status = stub.RemoveProbe(&context, request, &response);
  require(status.ok(),
          label + " RemoveProbe failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  require(response.original_bytes_restored(),
          label + " did not report original bytes restored");
  return response;
}

api::Session detach(api::Tracer::Stub &stub, const std::string &session_id,
                    const std::filesystem::path &evidence,
                    const std::string &label) {
  api::DetachRequest request;
  request.set_session_id(session_id);
  api::Session response;
  grpc::ClientContext context;
  set_deadline(context);
  const auto status = stub.Detach(&context, request, &response);
  require(status.ok(), label + " Detach failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  require(response.state() == api::Session::DETACHED,
          label + " did not report DETACHED");
  return response;
}

class EventCollector {
public:
  EventCollector(api::Tracer::Stub &stub, const std::string &session_id,
                 std::filesystem::path evidence)
      : evidence_(std::move(evidence)) {
    std::filesystem::create_directories(evidence_);
    request_.add_session_ids(session_id);
    context_.set_deadline(std::chrono::system_clock::now() + 60s);
    auto reader = stub.WatchEvents(&context_, request_);
    worker_ = std::thread([this, reader = std::move(reader)]() mutable {
      api::TraceEvent event;
      std::size_t saved = 0;
      while (reader->Read(&event)) {
        {
          std::lock_guard lock(mutex_);
          events_.push_back(event);
        }
        if (saved < 512) {
          ++saved;
          save_proto(evidence_ / ("event-" + std::to_string(saved) + ".pb"),
                     event);
        }
      }
      status_ = reader->Finish();
    });
  }

  ~EventCollector() { cancel(); }

  EventCollector(const EventCollector &) = delete;
  EventCollector &operator=(const EventCollector &) = delete;

  std::vector<api::TraceEvent> snapshot() {
    std::lock_guard lock(mutex_);
    return events_;
  }

  bool wait_for_probe_threads(std::uint64_t probe_id, std::size_t thread_count,
                              std::size_t object_count,
                              std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
      std::set<std::uint32_t> tids;
      std::set<std::uint64_t> objects;
      for (const auto &event : snapshot()) {
        if (event.probe_id() == probe_id) {
          tids.insert(event.tid());
          if (event.object_address() != 0) {
            objects.insert(event.object_address());
          }
        }
      }
      if (tids.size() >= thread_count && objects.size() >= object_count) {
        return true;
      }
      std::this_thread::sleep_for(20ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  }

  bool wait_for_objects(std::uint64_t probe_id,
                        const std::set<std::uint64_t> &expected_objects,
                        std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
      std::set<std::uint64_t> objects;
      for (const auto &event : snapshot()) {
        if (event.probe_id() == probe_id && event.object_address() != 0) {
          objects.insert(event.object_address());
        }
      }
      if (std::includes(objects.begin(), objects.end(),
                        expected_objects.begin(), expected_objects.end())) {
        return true;
      }
      std::this_thread::sleep_for(20ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
  }

  void cancel() {
    if (worker_.joinable()) {
      context_.TryCancel();
      worker_.join();
      require(status_.ok() ||
                  status_.error_code() == grpc::StatusCode::CANCELLED,
              "event stream failed: " + status_.error_message());
    }
  }

private:
  std::filesystem::path evidence_;
  api::WatchEventsRequest request_;
  grpc::ClientContext context_;
  std::mutex mutex_;
  std::vector<api::TraceEvent> events_;
  grpc::Status status_;
  std::thread worker_;
};

void verify_events(const std::vector<api::TraceEvent> &events,
                   std::uint32_t pid, const std::string &session_id,
                   const std::unordered_map<std::uint64_t, bool> &kinds,
                   const std::set<std::uint64_t> &stable_objects) {
  std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>>
      sequences;
  std::unordered_map<std::uint64_t, std::set<std::uint32_t>> tids;
  std::unordered_map<std::uint64_t, std::set<std::uint64_t>> objects;
  for (const auto &event : events) {
    const auto probe = kinds.find(event.probe_id());
    if (probe == kinds.end()) {
      continue;
    }
    require(event.protocol_version() == 2 && event.pid() == pid &&
                event.session_id() == session_id && event.tid() > 0,
            "event identity or protocol version is incorrect");
    require(
        event.probe_sequence() > 0 &&
            sequences[event.probe_id()].insert(event.probe_sequence()).second,
        "probe_sequence was zero or repeated");
    require(event.arguments_size() == 1 &&
                event.arguments(0).type() == api::INT32 &&
                event.arguments(0).data_case() == api::Value::kSignedValue,
            "event did not preserve the native int argument");
    const auto argument = event.arguments(0).signed_value();
    const bool is_method = probe->second;
    require((is_method && event.object_address() != 0) ||
                (!is_method && event.object_address() == 0),
            "event object address does not match callable kind");
    const auto expected = argument + (is_method ? 1007 : 7);
    require(event.callback_value() == expected,
            "callback result does not match event arguments and object");
    tids[event.probe_id()].insert(event.tid());
    if (is_method) {
      objects[event.probe_id()].insert(event.object_address());
    }
  }
  for (const auto &[probe_id, is_method] : kinds) {
    require(tids[probe_id].size() >= 4,
            "probe did not report calls from four distinct worker TIDs");
    if (is_method) {
      require(objects[probe_id].size() >= 4,
              "method events did not capture four distinct object addresses");
      for (const auto object_address : stable_objects) {
        require(::tracer::compat::contains(objects[probe_id], object_address),
                "method event did not capture a stable worker object address");
      }
    }
  }
}

void run(const std::filesystem::path &server_path,
         const std::filesystem::path &agent_path,
         const std::filesystem::path &target_path,
         const std::filesystem::path &output_dir, bool busy_main,
         bool nested_main, bool shared_main,
         const std::vector<std::filesystem::path> &expected_modules) {
  std::filesystem::create_directories(output_dir);
  std::ofstream report_file(output_dir / "report.txt", std::ios::trunc);
  require(static_cast<bool>(report_file), "cannot create report.txt");

  const auto endpoint = "unix://" + (output_dir / "tracer.sock").string();
  ChildProcess server =
      spawn({server_path.string(), "--agent", agent_path.string(), "--listen",
             endpoint, "--timeout", "10"},
            output_dir / "server.log");
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  const auto ready_by = std::chrono::system_clock::now() + 10s;
  while (!channel->WaitForConnected(std::chrono::system_clock::now() + 100ms)) {
    require(server.running(), "server exited before becoming ready");
    require(std::chrono::system_clock::now() < ready_by,
            "server did not become ready");
  }
  auto stub = api::Tracer::NewStub(channel);

  std::vector<std::string> target_arguments{target_path.string()};
  if (shared_main) {
    target_arguments.push_back("--shared-main");
  } else if (nested_main) {
    target_arguments.push_back("--nested-main");
  } else if (busy_main) {
    target_arguments.push_back("--busy-main");
  }
  ChildProcess target = spawn(target_arguments, output_dir / "target.log");
  require(target.running(), "threaded target failed to start");
  const auto target_info = wait_target_info(output_dir / "target.log", 3s);
  require(target_info.busy_main == busy_main,
          "target did not report the requested main-thread mode");
  const std::string expected_mode =
      shared_main
          ? "shared"
          : (nested_main ? "nested" : (busy_main ? "busy" : "sleeping"));
  require(target_info.mode == expected_mode,
          "target did not report the requested execution mode");
  ScopedCallbackGate callback_gate(target.pid(), target_info.callback_gate);
  const auto initial =
      wait_for_worker_progress(output_dir / "target.log", std::nullopt, 3s);
  std::optional<MainSnapshot> last_main;
  std::uint64_t main_signal_mask = 0;
  if (busy_main) {
    last_main =
        wait_for_main_progress(output_dir / "target.log", std::nullopt, 3s);
    main_signal_mask = read_main_thread_signal_mask(target.pid());
    require((main_signal_mask & (std::uint64_t{1} << (SIGUSR1 - 1))) != 0,
            "busy target main thread did not block SIGUSR1 before its loop");
    report(report_file, "busy-main SIGUSR1 blocked in initial SigBlk mask");
  }
  report(report_file,
         std::string("target reported main mode=") + target_info.mode);
  report_main_checkpoint(report_file, "started", busy_main, last_main);
  std::set<std::uint64_t> stable_objects;
  for (const auto &worker : initial) {
    stable_objects.insert(worker.object_address);
  }
  const auto original_free =
      read_process_memory(target.pid(), target_info.free_entry, 16);
  const auto original_method =
      read_process_memory(target.pid(), target_info.method_entry, 16);
  report(report_file, "target started with four stable workers and a "
                      "short-lived worker churn thread");

  const auto session = attach(*stub, target.pid(), output_dir, "attach");
  require_session_modules(session, expected_modules, "Attach response");
  const auto pinned_modules = session_module_paths(session);
  wait_for_server_module_fd_count(server.pid(), pinned_modules,
                                  pinned_modules.size(), 3s);
  report(report_file, "server retained discovered application-module pins");
  if (busy_main) {
    last_main =
        wait_for_main_progress(output_dir / "target.log", last_main, 5s);
    require_main_signal_mask(target.pid(), main_signal_mask, "Attach");
  }
  report_main_checkpoint(report_file, "attached", busy_main, last_main);
  require(session.pid() == static_cast<std::uint32_t>(target.pid()),
          "Attach returned a different PID");
  EventCollector collector(*stub, session.id(), output_dir / "events");
  const std::string callback =
      "extern \"C\" int usleep(unsigned int);\n"
      "extern \"C\" void threaded_callback_enter();\n"
      "extern \"C\" void threaded_callback_leave();\n"
      "extern \"C\" long long trace_callback(const TraceContext& context) {\n"
      "  auto* gate = reinterpret_cast<volatile unsigned int*>(" +
      std::to_string(target_info.callback_gate) +
      "ULL);\n"
      "  threaded_callback_enter();\n"
      "  while (__atomic_load_n(gate, __ATOMIC_ACQUIRE) != 0) {\n"
      "    (void)usleep(1000);\n"
      "  }\n"
      "  threaded_callback_leave();\n"
      "  return context.arguments[0].signed_value + "
      "(context.object_address ? 1000 : 0) + 7;\n}\n";
  const auto free_spec =
      make_spec("_ZN8threaded15namespaced_workEi", api::FUNCTION, callback);
  const auto method_spec =
      make_spec("_ZN8threaded6Worker4workEi", api::METHOD, callback);
  std::unordered_map<std::uint64_t, bool> observed_kinds;
  WorkerSnapshots last = initial;

  for (int cycle = 1; cycle <= 3; ++cycle) {
    const std::string suffix = "cycle-" + std::to_string(cycle);
    auto free_probe =
        add_probe(*stub, session.id(), free_spec, output_dir, suffix + "-free");
    require(free_probe.entry_address() == target_info.free_entry &&
                free_probe.active(),
            suffix + " free probe entry or active state is incorrect");
    if (busy_main) {
      require_main_signal_mask(target.pid(), main_signal_mask,
                               suffix + " add free probe");
    }
    observed_kinds.emplace(free_probe.id(), false);
    auto method_probe = add_probe(*stub, session.id(), method_spec, output_dir,
                                  suffix + "-method");
    require(method_probe.entry_address() == target_info.method_entry &&
                method_probe.active(),
            suffix + " method probe entry or active state is incorrect");
    if (busy_main) {
      require_main_signal_mask(target.pid(), main_signal_mask,
                               suffix + " add method probe");
    }
    observed_kinds.emplace(method_probe.id(), true);

    require(
        collector.wait_for_probe_threads(free_probe.id(), 4, 0, 8s) &&
            collector.wait_for_probe_threads(method_probe.id(), 4, 4, 8s) &&
            collector.wait_for_objects(method_probe.id(), stable_objects, 8s),
        suffix + " did not observe both probes");
    if (busy_main) {
      last_main =
          wait_for_main_progress(output_dir / "target.log", last_main, 5s);
    }
    last = wait_for_worker_progress(output_dir / "target.log", last, 5s);

    const auto removed_free = remove_probe(*stub, session.id(), free_probe.id(),
                                           output_dir, suffix + "-remove-free");
    (void)removed_free;
    require(read_process_memory(target.pid(), target_info.free_entry, 16) ==
                original_free,
            suffix + " free function bytes were not restored");
    if (busy_main) {
      require_main_signal_mask(target.pid(), main_signal_mask,
                               suffix + " remove free probe");
    }
    if (busy_main) {
      last_main =
          wait_for_main_progress(output_dir / "target.log", last_main, 5s);
    }
    last = wait_for_worker_progress(output_dir / "target.log", last, 5s);

    const auto removed_method =
        remove_probe(*stub, session.id(), method_probe.id(), output_dir,
                     suffix + "-remove-method");
    (void)removed_method;
    require(read_process_memory(target.pid(), target_info.method_entry, 16) ==
                original_method,
            suffix + " method bytes were not restored");
    if (busy_main) {
      require_main_signal_mask(target.pid(), main_signal_mask,
                               suffix + " remove method probe");
    }
    if (busy_main) {
      last_main =
          wait_for_main_progress(output_dir / "target.log", last_main, 5s);
    }
    last = wait_for_worker_progress(output_dir / "target.log", last, 5s);
    require_all_threads_untraced(target.pid());
    report(report_file,
           suffix + " attached, captured concurrent calls, removed both hooks, "
                    "and resumed all stable workers");
    report_main_checkpoint(report_file, suffix + "-complete", busy_main,
                           last_main);
  }

  const auto validation_events = collector.snapshot();
  verify_events(validation_events, static_cast<std::uint32_t>(target.pid()),
                session.id(), observed_kinds, stable_objects);
  report(report_file, "events contain unique per-probe sequences, four worker "
                      "TIDs, correct arguments, "
                      "callback values, and distinct real object addresses");

  // A rejected callback must leave the live session and all native workers
  // usable.
  auto invalid = free_spec;
  invalid.set_symbol("_ZN8threaded15namespaced_workEi");
  invalid.set_callback_source("extern \"C\" long long trace_callback(const "
                              "TraceContext&) { return ;\n");
  api::AddProbeRequest invalid_request;
  invalid_request.set_session_id(session.id());
  *invalid_request.mutable_spec() = invalid;
  api::Probe invalid_response;
  grpc::ClientContext invalid_context;
  set_deadline(invalid_context);
  const auto invalid_status =
      stub->AddProbe(&invalid_context, invalid_request, &invalid_response);
  require(!invalid_status.ok(), "malformed callback source was accepted");
  save_proto(output_dir / "malformed-callback-status.pb", invalid_response);
  last = wait_for_worker_progress(output_dir / "target.log", last, 5s);
  report(report_file, "malformed callback rejected while target session and "
                      "workers remained usable");

  auto final_free =
      add_probe(*stub, session.id(), free_spec, output_dir, "detach-free");
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask,
                             "detach add free probe");
  }
  auto final_method =
      add_probe(*stub, session.id(), method_spec, output_dir, "detach-method");
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask,
                             "detach add method probe");
  }
  require(collector.wait_for_probe_threads(final_free.id(), 4, 0, 8s) &&
              collector.wait_for_probe_threads(final_method.id(), 4, 4, 8s) &&
              collector.wait_for_objects(final_method.id(), stable_objects, 8s),
          "final hooks did not become active before detach");
  callback_gate.arm();
  wait_for_active_callbacks(target.pid(), target_info.callbacks_active, 4, 5s);
  const auto detach_start = std::chrono::steady_clock::now();
  auto detach_future = std::async(std::launch::async, [&] {
    return detach(*stub, session.id(), output_dir, "detach-active-hooks");
  });
  try {
    const auto restore_deadline = detach_start + 8s;
    bool restored = false;
    while (std::chrono::steady_clock::now() < restore_deadline) {
      restored = read_process_memory(target.pid(), target_info.free_entry,
                                     16) == original_free;
      if (restored) {
        break;
      }
      require(detach_future.wait_for(0ms) == std::future_status::timeout,
              "Detach completed before restoring entry bytes under the gate");
      std::this_thread::sleep_for(10ms);
    }
    require(
        restored,
        "Detach did not restore the first entry while callbacks were gated");
    require(
        detach_future.wait_for(100ms) == std::future_status::timeout,
        "Detach completed while four callbacks were still held in the gate");
  } catch (...) {
    callback_gate.release();
    if (detach_future.valid()) {
      (void)detach_future.wait_for(20s);
    }
    throw;
  }
  callback_gate.release();
  const auto detached_session = detach_future.get();
  require_session_history(session, detached_session);
  wait_for_server_module_fd_count(server.pid(), pinned_modules, 0, 3s);
  report(report_file, "Detach released discovered-module pins while retaining "
                      "session history");
  const auto detach_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - detach_start)
                             .count();
  const auto active_deadline = std::chrono::steady_clock::now() + 5s;
  while (read_process_u32(target.pid(), target_info.callbacks_active) != 0 &&
         std::chrono::steady_clock::now() < active_deadline) {
    std::this_thread::sleep_for(5ms);
  }
  require(read_process_u32(target.pid(), target_info.callbacks_active) == 0,
          "callbacks did not leave after Detach released the gate");
  require(read_process_memory(target.pid(), target_info.free_entry, 16) ==
                  original_free &&
              read_process_memory(target.pid(), target_info.method_entry, 16) ==
                  original_method,
          "detach did not restore both threaded target entry points");
  require_all_threads_untraced(target.pid());
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask, "Detach");
  }
  last = wait_for_worker_progress(output_dir / "target.log", last, 5s);
  if (busy_main) {
    last_main =
        wait_for_main_progress(output_dir / "target.log", last_main, 5s);
  }
  report_main_checkpoint(report_file, "detached", busy_main, last_main);
  report(
      report_file,
      "Detach restored the first entry but stayed pending with four callbacks "
      "held at the gate; releasing the gate drained callbacks in " +
          std::to_string(detach_ms) + " ms and resumed every target TID");
  collector.cancel();

  // Shut down the server with live probes and verify its cleanup path restores
  // the target.
  const auto shutdown_session =
      attach(*stub, target.pid(), output_dir, "shutdown-attach");
  require_session_modules(shutdown_session, expected_modules,
                          "shutdown Attach response");
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask, "shutdown Attach");
  }
  auto shutdown_free = add_probe(*stub, shutdown_session.id(), free_spec,
                                 output_dir, "shutdown-free");
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask,
                             "shutdown add free probe");
  }
  auto shutdown_method = add_probe(*stub, shutdown_session.id(), method_spec,
                                   output_dir, "shutdown-method");
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask,
                             "shutdown add method probe");
  }
  EventCollector shutdown_collector(*stub, shutdown_session.id(),
                                    output_dir / "shutdown-events");
  require(
      shutdown_collector.wait_for_probe_threads(shutdown_free.id(), 4, 0, 8s) &&
          shutdown_collector.wait_for_probe_threads(shutdown_method.id(), 4, 4,
                                                    8s) &&
          shutdown_collector.wait_for_objects(shutdown_method.id(),
                                              stable_objects, 8s),
      "shutdown hooks did not become active");
  std::this_thread::sleep_for(20ms);
  if (busy_main) {
    last_main =
        wait_for_main_progress(output_dir / "target.log", last_main, 5s);
  }
  shutdown_collector.cancel();
  server.stop(8s);
  require(read_process_memory(target.pid(), target_info.free_entry, 16) ==
                  original_free &&
              read_process_memory(target.pid(), target_info.method_entry, 16) ==
                  original_method,
          "SIGTERM server shutdown did not restore active threaded hooks");
  require(target.running(), "server shutdown terminated the target process");
  require_all_threads_untraced(target.pid());
  if (busy_main) {
    require_main_signal_mask(target.pid(), main_signal_mask, "server shutdown");
  }
  last = wait_for_worker_progress(output_dir / "target.log", last, 5s);
  if (busy_main) {
    last_main =
        wait_for_main_progress(output_dir / "target.log", last_main, 5s);
  }
  report_main_checkpoint(report_file, "server-shutdown", busy_main, last_main);
  report(report_file, "server SIGTERM with active hooks restored bytes and "
                      "left every target TID usable");

  verify_events(shutdown_collector.snapshot(),
                static_cast<std::uint32_t>(target.pid()), shutdown_session.id(),
                {{shutdown_free.id(), false}, {shutdown_method.id(), true}},
                stable_objects);
  require(target.running(), "target exited unexpectedly after server shutdown");
  report(report_file, "integration passed");
}

} // namespace

int main(int argc, char **argv) {
  try {
    std::filesystem::path server;
    std::filesystem::path agent;
    std::filesystem::path target;
    std::filesystem::path output_dir;
    bool busy_main = false;
    bool nested_main = false;
    bool shared_main = false;
    std::vector<std::filesystem::path> expected_modules;
    for (int index = 1; index < argc; ++index) {
      const std::string key = argv[index];
      if (key == "--busy-main") {
        busy_main = true;
        continue;
      }
      if (key == "--nested-main") {
        busy_main = true;
        nested_main = true;
        continue;
      }
      if (key == "--shared-main") {
        busy_main = true;
        shared_main = true;
        continue;
      }
      require(index + 1 < argc, "missing value for " + key);
      const std::string value = argv[++index];
      if (key == "--server") {
        server = value;
      } else if (key == "--agent") {
        agent = value;
      } else if (key == "--target") {
        target = value;
      } else if (key == "--output-dir") {
        output_dir = value;
      } else if (key == "--expected-module") {
        expected_modules.emplace_back(value);
      } else {
        throw std::runtime_error("unknown option: " + key);
      }
    }
    require(!server.empty() && !agent.empty() && !target.empty() &&
                !output_dir.empty(),
            "usage: threaded-integration --server PATH --agent PATH "
            "--target PATH --output-dir DIR");
    require(expected_modules.size() == 2,
            "threaded integration requires two --expected-module paths");
    for (const auto &module : expected_modules) {
      require(module.is_absolute(), "--expected-module paths must be absolute");
    }
    run(server, agent, target, output_dir, busy_main, nested_main, shared_main,
        expected_modules);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "threaded integration failed: " << error.what() << '\n';
    return 1;
  }
}

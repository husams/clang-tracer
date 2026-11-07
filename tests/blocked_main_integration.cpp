#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>

#include "tracer.grpc.pb.h"

extern char **environ;

namespace {

using namespace std::chrono_literals;
namespace api = clangtracer::v1;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class Child {
public:
  Child() = default;
  explicit Child(pid_t pid) : pid_(pid) {}
  ~Child() { stop(); }
  Child(const Child &) = delete;
  Child &operator=(const Child &) = delete;
  Child(Child &&other) noexcept : pid_(other.pid_) { other.pid_ = -1; }
  Child &operator=(Child &&other) noexcept {
    if (this != &other) {
      stop();
      pid_ = other.pid_;
      other.pid_ = -1;
    }
    return *this;
  }

  pid_t pid() const { return pid_; }
  bool running() const {
    int status = 0;
    return pid_ > 0 && ::waitpid(pid_, &status, WNOHANG) == 0;
  }
  int stop() noexcept {
    if (pid_ <= 0) {
      return 0;
    }
    (void)::kill(pid_, SIGTERM);
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
      if (::waitpid(pid_, &status, WNOHANG) == pid_) {
        pid_ = -1;
        return status;
      }
      std::this_thread::sleep_for(10ms);
    }
    (void)::kill(pid_, SIGKILL);
    (void)::waitpid(pid_, &status, 0);
    pid_ = -1;
    return status;
  }

private:
  pid_t pid_ = -1;
};

Child spawn(const std::vector<std::string> &args,
            const std::filesystem::path &log) {
  posix_spawn_file_actions_t actions;
  require(::posix_spawn_file_actions_init(&actions) == 0,
          "could not initialize spawn actions");
  int result = ::posix_spawn_file_actions_addopen(
      &actions, STDOUT_FILENO, log.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (result == 0) {
    result = ::posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO,
                                               STDERR_FILENO);
  }
  if (result != 0) {
    ::posix_spawn_file_actions_destroy(&actions);
    throw std::runtime_error("could not configure process output redirection");
  }
  std::vector<char *> argv;
  for (const auto &arg : args) {
    argv.push_back(const_cast<char *>(arg.c_str()));
  }
  argv.push_back(nullptr);
  pid_t pid = -1;
  result = ::posix_spawn(&pid, argv.front(), &actions, nullptr, argv.data(),
                         environ);
  ::posix_spawn_file_actions_destroy(&actions);
  require(result == 0, "could not start process (error " +
                           std::to_string(result) + ")");
  return Child(pid);
}

std::string read_text(const std::filesystem::path &path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

struct TargetInfo {
  pid_t worker = 0;
  std::uintptr_t entry = 0;
  std::uintptr_t calls = 0;
  std::string mode;
};

TargetInfo wait_target(const std::filesystem::path &log, pid_t pid) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto text = read_text(log);
    long seen_pid = 0;
    long worker = 0;
    void *entry = nullptr;
    void *calls = nullptr;
    char mode[32]{};
    if (std::sscanf(text.c_str(),
                    "READY pid=%ld worker=%ld entry=%p calls=%p mode=%31s",
                    &seen_pid, &worker, &entry, &calls, mode) == 5 &&
        seen_pid == pid) {
      return {static_cast<pid_t>(worker),
              reinterpret_cast<std::uintptr_t>(entry),
              reinterpret_cast<std::uintptr_t>(calls), mode};
    }
    std::this_thread::sleep_for(10ms);
  }
  throw std::runtime_error("blocked target did not publish readiness");
}

std::vector<pid_t> tids(pid_t pid) {
  std::vector<pid_t> result;
  const auto task_dir = std::filesystem::path("/proc") /
                        std::to_string(pid) / "task";
  for (const auto &entry : std::filesystem::directory_iterator(task_dir)) {
    try {
      result.push_back(static_cast<pid_t>(
          std::stol(entry.path().filename().string())));
    } catch (...) {
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::string syscall_state(pid_t pid, pid_t tid) {
  std::ifstream input(std::filesystem::path("/proc") / std::to_string(pid) /
                      "task" / std::to_string(tid) / "syscall");
  std::string value;
  std::getline(input, value);
  return value;
}

bool is_futex_wait(const std::string &state) {
  return state.rfind("202 ", 0) == 0;
}

std::uint64_t read_remote_calls(pid_t pid, std::uintptr_t address) {
  std::uint64_t value = 0;
  iovec local{&value, sizeof(value)};
  iovec remote{reinterpret_cast<void *>(address), sizeof(value)};
  require(::process_vm_readv(pid, &local, 1, &remote, 1, 0) ==
              static_cast<ssize_t>(sizeof(value)),
          "could not read target call counter");
  return value;
}

void wait_main_syscall(pid_t pid, int syscall_number,
                       const std::string &syscall_name) {
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto state = syscall_state(pid, pid);
    if (state.rfind(std::to_string(syscall_number) + " ", 0) == 0) {
      return;
    }
    std::this_thread::sleep_for(5ms);
  }
  throw std::runtime_error("leader did not enter " + syscall_name);
}

void wait_main_futex(pid_t pid) {
  wait_main_syscall(pid, SYS_futex, "raw FUTEX_WAIT");
}

std::string wait_for_eligible_worker(pid_t pid, pid_t tid,
                                     const std::string &mode) {
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  std::string last_state;
  while (std::chrono::steady_clock::now() < deadline) {
    last_state = syscall_state(pid, tid);
    if (mode == "sleep-worker") {
      if (last_state.rfind("35 ", 0) == 0 ||
          last_state.rfind("230 ", 0) == 0) {
        return last_state;
      }
    } else if (last_state == "running" || last_state.rfind("-1 ", 0) == 0) {
      return last_state;
    }
    std::this_thread::sleep_for(2ms);
  }
  throw std::runtime_error("worker was not observed in an eligible state: " +
                           last_state);
}

void require_untraced(pid_t pid) {
  const auto live_tids = tids(pid);
  require(!live_tids.empty(), "target has no live tasks");
  for (const auto tid : live_tids) {
    std::ifstream status(std::filesystem::path("/proc") /
                         std::to_string(pid) / "task" / std::to_string(tid) /
                         "status");
    std::string line;
    bool found = false;
    while (std::getline(status, line)) {
      if (line.rfind("TracerPid:", 0) == 0) {
        int tracer = -1;
        require(std::sscanf(line.c_str(), "TracerPid:%d", &tracer) == 1 &&
                    tracer == 0,
                "TID " + std::to_string(tid) + " still has ptrace owner " +
                    std::to_string(tracer));
        found = true;
        break;
      }
    }
    require(found, "could not read TracerPid for TID " + std::to_string(tid));
  }
}

void wait_server_ready(const std::shared_ptr<grpc::Channel> &channel,
                       Child &server) {
  const auto deadline = std::chrono::system_clock::now() + 10s;
  while (!channel->WaitForConnected(std::chrono::system_clock::now() + 100ms)) {
    require(server.running(), "server exited before becoming ready");
    require(std::chrono::system_clock::now() < deadline,
            "server did not become ready");
  }
}

grpc::Status attach(api::Tracer::Stub &stub, pid_t pid, api::Session *session) {
  api::AttachRequest request;
  request.set_pid(static_cast<std::uint32_t>(pid));
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + 8s);
  return stub.Attach(&context, request, session);
}

void check_failed_attach(api::Tracer::Stub &stub, pid_t pid,
                         const std::vector<pid_t> &blocked_tids) {
  api::Session session;
  const auto status = attach(stub, pid, &session);
  require(status.error_code() == grpc::StatusCode::FAILED_PRECONDITION,
          "all-blocked Attach should return FAILED_PRECONDITION, got " +
              std::to_string(status.error_code()) + ": " +
              status.error_message());
  const auto detail = status.error_message();
  require(detail.find("futex") != std::string::npos,
          "failed admission omitted blocked syscall details: " + detail);
  for (const auto tid : blocked_tids) {
    require(detail.find(std::to_string(tid)) != std::string::npos,
            "failed admission omitted blocked TID " + std::to_string(tid) +
                ": " + detail);
  }
}

void run_positive(api::Tracer::Stub &stub, Child &target, pid_t pid,
                  const TargetInfo &info, const std::string &leader) {
  require((info.mode == "hot-worker" || info.mode == "sleep-worker") &&
              info.worker > 0 && info.entry != 0 && info.calls != 0,
          "positive target did not report its active worker");
  if (leader == "futex") {
    wait_main_futex(pid);
  } else if (leader == "epoll") {
    wait_main_syscall(pid, SYS_epoll_wait, "epoll_wait");
  } else if (leader == "read") {
    wait_main_syscall(pid, SYS_read, "pipe read");
  } else {
    throw std::runtime_error("unsupported leader syscall mode " + leader);
  }
  const auto worker_state =
      wait_for_eligible_worker(pid, info.worker, info.mode);
  if (info.mode == "sleep-worker") {
    require(worker_state.rfind("35 ", 0) == 0 ||
                worker_state.rfind("230 ", 0) == 0,
            "designated sleeping worker was not in nanosleep/clock_nanosleep: " +
                worker_state);
  } else {
    require(worker_state == "running" || worker_state.rfind("-1 ", 0) == 0,
            "designated hot worker was not proven outside a syscall: " +
                worker_state);
  }

  api::Session session;
  const auto attached = attach(stub, pid, &session);
  require(attached.ok(), "Attach with blocked leader and eligible worker failed: " +
                              attached.error_message());
  require(session.pid() == static_cast<std::uint32_t>(pid) &&
              session.state() == api::Session::ATTACHED,
          "Attach response did not preserve process PID identity");

  api::ProbeSpec spec;
  spec.set_symbol("blocked_worker_probe");
  spec.mutable_signature()->set_kind(api::FUNCTION);
  spec.mutable_signature()->set_result(api::INT32);
  spec.mutable_signature()->add_arguments(api::INT32);
  spec.set_callback_source(
      "extern \"C\" long long trace_callback(const TraceContext& context) {\n"
      "  return context.arguments[0].signed_value + 7;\n"
      "}\n");
  api::AddProbeRequest add;
  add.set_session_id(session.id());
  *add.mutable_spec() = spec;
  api::Probe probe;
  grpc::ClientContext add_context;
  add_context.set_deadline(std::chrono::system_clock::now() + 15s);
  const auto add_status = stub.AddProbe(&add_context, add, &probe);
  require(add_status.ok(), "AddProbe failed: " + add_status.error_message());
  require(probe.active() && probe.entry_address() == info.entry,
          "probe did not resolve to the target function entry");

  api::ReadEventsRequest read;
  read.mutable_filter()->add_session_ids(session.id());
  read.set_limit(256);
  api::ReadEventsResponse events;
  bool observed = false;
  const auto event_deadline = std::chrono::steady_clock::now() + 10s;
  while (std::chrono::steady_clock::now() < event_deadline && !observed) {
    grpc::ClientContext read_context;
    read_context.set_deadline(std::chrono::system_clock::now() + 2s);
    const auto read_status = stub.ReadEvents(&read_context, read, &events);
    require(read_status.ok(), "ReadEvents failed: " + read_status.error_message());
    for (const auto &event : events.events()) {
      if (event.probe_id() == probe.id()) {
        require(event.pid() == static_cast<std::uint32_t>(pid) &&
                    event.tid() == static_cast<std::uint32_t>(info.worker),
                "event did not preserve process PID and selected worker TID");
        require(event.arguments_size() == 1 &&
                    event.arguments(0).signed_value() > 0 &&
                    event.callback_value() ==
                        event.arguments(0).signed_value() + 7,
                "probe event or callback result is incorrect");
        observed = true;
        break;
      }
    }
    if (!observed) {
      std::this_thread::sleep_for(25ms);
    }
  }
  require(observed, "probe did not emit an event from the designated worker");

  api::DetachRequest detach;
  detach.set_session_id(session.id());
  api::Session detached;
  grpc::ClientContext detach_context;
  detach_context.set_deadline(std::chrono::system_clock::now() + 8s);
  const auto detach_status = stub.Detach(&detach_context, detach, &detached);
  require(detach_status.ok() && detached.state() == api::Session::DETACHED,
          "Detach failed to restore the target: " + detach_status.error_message());
  require(detached.probes_size() == 1 &&
              detached.probes(0).original_bytes_restored(),
          "Detach did not report the probe bytes restored");
  require_untraced(pid);
  const auto before = read_remote_calls(pid, info.calls);
  const auto progress_deadline = std::chrono::steady_clock::now() + 3s;
  bool progressed = false;
  while (std::chrono::steady_clock::now() < progress_deadline) {
    if (read_remote_calls(pid, info.calls) > before) {
      progressed = true;
      break;
    }
    std::this_thread::sleep_for(20ms);
  }
  require(progressed && target.running(),
          "target did not continue making progress after Detach");
}

void run_all_blocked(api::Tracer::Stub &stub, Child &server, pid_t pid,
                     const std::filesystem::path &server_log) {
  wait_main_futex(pid);
  const auto live_tids = tids(pid);
  require(live_tids.size() >= 3, "all-blocked fixture did not create its tasks");
  for (const auto tid : live_tids) {
    const auto state = syscall_state(pid, tid);
    require(is_futex_wait(state), "TID " + std::to_string(tid) +
                                      " was not blocked in futex: " + state);
  }
  check_failed_attach(stub, pid, live_tids);
  require_untraced(pid);
  // A second attempt proves the failed admission left no stale session or
  // ptrace ownership behind.
  check_failed_attach(stub, pid, live_tids);
  require_untraced(pid);
  const auto status = server.stop();
  require(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "server did not exit cleanly after failed Attach");
  const auto log = read_text(server_log);
  require(log.find("cleanup incomplete") == std::string::npos &&
              log.find("shutdown complete") != std::string::npos,
          "failed admission left server shutdown cleanup incomplete");
}

bool is_job_stopped(pid_t pid, pid_t tid) {
  std::ifstream status(std::filesystem::path("/proc") / std::to_string(pid) /
                       "task" / std::to_string(tid) / "status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("State:", 0) == 0) {
      return line.find('T') != std::string::npos;
    }
  }
  return false;
}

void wait_group_stopped(pid_t pid) {
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto live_tids = tids(pid);
    if (!live_tids.empty() &&
        std::all_of(live_tids.begin(), live_tids.end(),
                    [&](pid_t tid) { return is_job_stopped(pid, tid); })) {
      return;
    }
    std::this_thread::sleep_for(5ms);
  }
  throw std::runtime_error("SIGSTOP did not stop every target task");
}

void run_job_stopped(api::Tracer::Stub &stub, Child &server, Child &target,
                     pid_t pid, const std::filesystem::path &server_log) {
  wait_main_futex(pid);
  require(::kill(pid, SIGSTOP) == 0, "could not SIGSTOP target process");
  wait_group_stopped(pid);
  const auto stopped_tids = tids(pid);
  api::Session session;
  const auto status = attach(stub, pid, &session);
  require(status.error_code() == grpc::StatusCode::FAILED_PRECONDITION,
          "Attach to externally job-stopped target should fail safely, got " +
              std::to_string(status.error_code()) + ": " +
              status.error_message());
  wait_group_stopped(pid);
  require_untraced(pid);
  for (const auto tid : stopped_tids) {
    require(is_job_stopped(pid, tid),
            "Attach thawed externally stopped TID " + std::to_string(tid));
  }
  const auto server_status = server.stop();
  require(WIFEXITED(server_status) && WEXITSTATUS(server_status) == 0,
          "server did not exit cleanly after job-stopped Attach failure");
  const auto log = read_text(server_log);
  require(log.find("cleanup incomplete") == std::string::npos &&
              log.find("shutdown complete") != std::string::npos,
          "job-stopped Attach left server shutdown cleanup incomplete");
  // Resume only in fixture cleanup; the Attach implementation must preserve
  // the external job-control stop.
  require(::kill(target.pid(), SIGCONT) == 0,
          "fixture could not SIGCONT target during cleanup");
}

} // namespace

int main(int argc, char **argv) {
  try {
    std::filesystem::path server_path;
    std::filesystem::path agent_path;
    std::filesystem::path target_path;
    std::filesystem::path output_dir;
    std::string mode;
    std::string leader = "futex";
    for (int index = 1; index < argc; ++index) {
      require(index + 1 < argc, "missing value for " + std::string(argv[index]));
      const std::string value = argv[++index];
      if (std::string(argv[index - 1]) == "--server") server_path = value;
      else if (std::string(argv[index - 1]) == "--agent") agent_path = value;
      else if (std::string(argv[index - 1]) == "--target") target_path = value;
      else if (std::string(argv[index - 1]) == "--output-dir") output_dir = value;
      else if (std::string(argv[index - 1]) == "--mode") mode = value;
      else if (std::string(argv[index - 1]) == "--leader") leader = value;
      else
        throw std::runtime_error(std::string("unknown argument: ") +
                                 argv[index - 1]);
    }
    require(!server_path.empty() && !agent_path.empty() &&
                !target_path.empty() && !output_dir.empty() &&
                (mode == "positive-hot" || mode == "positive-sleeping" ||
                 mode == "all-blocked" || mode == "job-stopped") &&
                (leader == "futex" || leader == "epoll" || leader == "read"),
            "usage: blocked-main-integration --server PATH --agent PATH "
            "--target PATH --output-dir DIR "
            "--mode positive-hot|positive-sleeping|all-blocked|job-stopped "
            "--leader futex|epoll|read");
    std::filesystem::create_directories(output_dir);
    const auto endpoint = "unix://" + (output_dir / "tracer.sock").string();
    Child server = spawn({server_path.string(), "--agent", agent_path.string(),
                          "--listen", endpoint, "--timeout", "3"},
                         output_dir / "server.log");
    auto channel = grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
    wait_server_ready(channel, server);
    auto stub = api::Tracer::NewStub(channel);
    const std::string target_mode = mode == "positive-hot"
                                        ? "--hot-worker"
                                        : (mode == "positive-sleeping"
                                               ? "--sleep-worker"
                                               : "--all-blocked");
    const std::string leader_mode = "--leader-" + leader;
    Child target = spawn({target_path.string(), target_mode, leader_mode},
                         output_dir / "target.log");
    require(target.running(), "blocked target failed to start");
    const auto info = wait_target(output_dir / "target.log", target.pid());
    if (mode == "positive-hot" || mode == "positive-sleeping") {
      run_positive(*stub, target, target.pid(), info, leader);
    } else if (mode == "job-stopped") {
      require(info.mode == "all-blocked" && info.worker == 0,
              "job-stop target did not report all-blocked mode");
      run_job_stopped(*stub, server, target, target.pid(),
                      output_dir / "server.log");
    } else {
      require(info.mode == "all-blocked" && info.worker == 0,
              "negative target did not report all-blocked mode");
      run_all_blocked(*stub, server, target.pid(), output_dir / "server.log");
    }
    if (server.running()) {
      const auto status = server.stop();
      require(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "server did not shut down cleanly");
      require(read_text(output_dir / "server.log").find("cleanup incomplete") ==
                  std::string::npos,
              "server reported incomplete cleanup");
    }
    std::cout << "blocked-main " << mode << " integration passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "blocked-main integration failed: " << error.what() << '\n';
    return 1;
  }
}

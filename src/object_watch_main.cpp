#include "injector.hpp"
#include "object_code_cache.hpp"
#include "object_launch.hpp"
#include "object_watch.hpp"
#include "object_watch_plan.hpp"
#if defined(__aarch64__)
#include "arm_object_watch.hpp"
#endif
#include "proc_identity.hpp"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include <google/protobuf/util/json_util.h>
#include <poll.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
namespace api = clangtracer::v1;
namespace json = google::protobuf::util;
using Clock = std::chrono::steady_clock;
constexpr auto operation_timeout = std::chrono::seconds(10);

std::uint32_t process_id(std::string_view value) {
  std::uint32_t pid = 0;
  const auto end = value.data() + value.size();
  const auto parsed = std::from_chars(value.data(), end, pid);
  if (!pid || parsed.ec != std::errc{} || parsed.ptr != end)
    throw std::invalid_argument("invalid target PID");
  return pid;
}

api::ObjectWatchRequest read_request(const std::filesystem::path &file) {
  if (std::filesystem::file_size(file) > 1024 * 1024)
    throw std::invalid_argument("object definition exceeds 1 MiB");
  std::ifstream input(file);
  if (!input)
    throw std::runtime_error("cannot read object definition");
  std::string source((std::istreambuf_iterator<char>(input)), {});
  api::ObjectWatchRequest request;
  if (const auto parsed = json::JsonStringToMessage(source, &request); !parsed.ok())
    throw std::invalid_argument(parsed.ToString());
#if defined(__aarch64__)
  tracer::arm_object_watch::validate(request);
#else
  const bool cache = request.backend() == api::ObjectWatchRequest::CODE_CACHE;
  if ((!cache && request.backend() != api::ObjectWatchRequest::PTRACE) ||
      request.mode() != api::ObjectWatchRequest::WRITES || request.concrete_type().empty() ||
      request.field().empty() || request.code_symbol().empty() ||
      !request.borrow_caller().empty() || !request.application_rendezvous().empty() ||
      (!cache && (request.writer_entry() || !request.async_signals().empty())) ||
      request.stack_depth() > 64 || request.max_elements() > 65536 ||
      request.max_string_bytes() > 65536 || request.max_instances() > 65536)
    throw std::invalid_argument("object capture requires a verified main-image writer and field");
  std::uint64_t signals = 0;
  for (auto number : request.async_signals()) {
    if (!number || number > 64 || number == SIGKILL || number == SIGSTOP ||
        number == SIGSEGV || number == SIGBUS || (signals & (1ULL << (number - 1))))
      throw std::invalid_argument("invalid or duplicate asynchronous signal number");
    signals |= 1ULL << (number - 1);
  }
  if (request.cfi_stack() && (!cache || !request.stack_depth()))
    throw std::invalid_argument("CFI stack capture requires code-cache and a positive stack depth");
  if (!request.max_elements())
    request.set_max_elements(4096);
  if (!request.max_string_bytes())
    request.set_max_string_bytes(4096);
#endif
  return request;
}

void print_json(const google::protobuf::Message &message) {
  std::string line;
  if (const auto formatted = json::MessageToJsonString(message, &line); !formatted.ok())
    throw std::runtime_error(formatted.ToString());
  std::cout << line << std::endl;
}

class StopInput {
  int signal_fd_;
  bool poll_stdin_;

public:
  explicit StopInput(bool poll_stdin = true) : signal_fd_(-1), poll_stdin_(poll_stdin) {
    sigset_t signals;
    ::sigemptyset(&signals);
    ::sigaddset(&signals, SIGINT);
    ::sigaddset(&signals, SIGTERM);
    if (::pthread_sigmask(SIG_BLOCK, &signals, nullptr))
      throw std::runtime_error("cannot block stop signals");
    signal_fd_ = ::signalfd(-1, &signals, SFD_CLOEXEC);
    if (signal_fd_ == -1)
      throw std::runtime_error("cannot watch stop signals");
  }
  ~StopInput() { ::close(signal_fd_); }
  bool requested(int timeout_ms = 100) const {
    pollfd fds[]{{poll_stdin_ ? STDIN_FILENO : -1, POLLIN, 0}, {signal_fd_, POLLIN, 0}};
    const auto status = ::poll(fds, 2, timeout_ms);
    if (status == -1 && errno != EINTR)
      throw std::runtime_error("cannot watch stop input");
    return status > 0;
  }
};

#if defined(__aarch64__)
void run_code_cache(const api::ObjectWatchRequest &request, std::uint32_t pid,
                    const std::string &executable, std::uint64_t start, const char *agent,
                    const std::string &selector, const StopInput &stop) {
  std::mutex output;
  tracer::arm_object_watch::NativeWatch watch(request, 1, pid, start, executable,
      (std::filesystem::temp_directory_path() / "events").string(),
      std::chrono::seconds(10), [&](auto event) {
        const std::lock_guard lock(output);
        event.set_sequence(event.probe_sequence());
        print_json(event);
      });
  watch.start(agent, selector);
  {
    const std::lock_guard lock(output);
    print_json(watch.status());
  }
  auto reason = api::ObjectObservation::WATCH_STOPPED;
  while (tracer::arm_object_watch::same_process(pid, start, executable)) {
    const auto status = watch.status();
    if (!status.active())
      break;
    if (!status.capture_limit().empty()) {
      reason = status.capture_limit() == "timeout" ? api::ObjectObservation::WATCH_TIMEOUT :
               status.capture_limit() == "buffer-overflow" ? api::ObjectObservation::BUFFER_OVERFLOW :
                                                    api::ObjectObservation::EVENT_LIMIT;
      break;
    }
    if (stop.requested())
      break;
  }
  const auto final = watch.stop(reason);
  const std::lock_guard lock(output);
  print_json(final);
}
#endif

int run_native_launch(const api::ObjectWatchRequest &request, std::vector<std::string> argv,
                      const std::string &agent, bool loader_ready) {
  using Observation = api::ObjectObservation;
  if (request.backend() != api::ObjectWatchRequest::CODE_CACHE ||
      !request.application_rendezvous().empty() || request.writer_tid())
    throw std::invalid_argument("launch --agent requires code-cache without a thread or rendezvous");
  const auto executable = std::filesystem::canonical(argv.front());
  const auto agent_file = std::filesystem::canonical(agent).string();
  StopInput stop(false);
  const auto deadline = Clock::now() + operation_timeout;
  auto child = tracer::object_launch::Child::stop_on_exec(argv,
      PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK, deadline,
      [&] { return stop.requested(0); });
  const auto pid = child.pid();
  const auto directory = tracer::proc::directory(pid);
  if (std::filesystem::read_symlink(directory / "exe") != executable)
    throw std::runtime_error("launched executable differs from the selected ELF");
  const auto start = tracer::proc::start_time(directory / "stat");
  tracer::injector::Process process;
  const auto module = request.code_module() == executable.string() ? "" : request.code_module();
  process.adopt_executable_entry(child, deadline, loader_ready, module);
  std::mutex output;
  tracer::object_cache::Watch watch(request, 1, pid, executable.string(),
      (std::filesystem::temp_directory_path() / "events").string(), [&](auto event) {
        const std::lock_guard lock(output);
        event.set_sequence(event.probe_sequence());
        print_json(event);
      });
  try {
    watch.install(process, tracer::object_cache::load_agent(process, agent_file), deadline, loader_ready);
    {
      const std::lock_guard lock(output);
      std::cout << "{\"coverageStart\":\"" << (loader_ready ? "loader-ready" : "executable-entry")
                << "\",\"pid\":" << pid << "}" << std::endl;
      print_json(watch.status());
    }
    process.detach();
    if (process.unsafe() || process.attached())
      throw std::runtime_error("could not safely release the native startup stop");
  } catch (...) {
    if (process.attached())
      process.mark_unsafe();
    throw;
  }
  child.release();

  auto reason = Observation::WATCH_STOPPED;
  std::optional<int> exit_status;
  const auto exited = [&] {
    if (exit_status)
      return true;
    for (;;) {
      int status;
      const auto found = ::waitpid(pid, &status, WNOHANG);
      if (found == pid) {
        if (!WIFEXITED(status) && !WIFSIGNALED(status))
          throw std::runtime_error("unexpected stop after native startup release");
        exit_status = status;
        return true;
      }
      if (!found)
        return false;
      if (errno != EINTR)
        throw std::system_error(errno, std::generic_category(), "wait for launched target");
    }
  };
  while (!exited()) {
    const auto status = watch.status();
    if (!status.capture_limit().empty()) {
      if (status.capture_limit() == "timeout")
        reason = Observation::WATCH_TIMEOUT;
      else if (status.capture_limit() == "buffer-overflow")
        reason = Observation::BUFFER_OVERFLOW;
      else
        reason = Observation::EVENT_LIMIT;
      break;
    }
    if (watch.retire_if_mapping_changed(Clock::now() + operation_timeout)) {
      print_json(watch.status());
      return 0;
    }
    if (stop.requested())
      break;
  }
  const auto restore_deadline = Clock::now() + operation_timeout;
  watch.stop_runtime_admissions();
  try {
    while (!exited()) {
      process.attach_for_patch(pid);
      if (tracer::proc::start_time(directory / "stat") != start)
        throw std::runtime_error("launched target identity changed before restoration");
      if (watch.retire_if_mapping_changed(restore_deadline)) {
        process.detach();
        print_json(watch.status());
        return 0;
      }
      const auto restored = watch.restore(process);
      process.detach();
      if (restored)
        break;
      if (Clock::now() >= restore_deadline)
        throw std::runtime_error("timed out restoring native startup redirects");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  } catch (...) {
    if (process.attached() || !exited())
      throw;
  }
  watch.finish(restore_deadline, exit_status ? Observation::TARGET_EXITED : reason);
  print_json(watch.status());
  if (!exit_status)
    return 0;
  return WIFEXITED(*exit_status) ? WEXITSTATUS(*exit_status) : 128 + WTERMSIG(*exit_status);
}

int run_launch(const api::ObjectWatchRequest &request, std::vector<std::string> argv) {
  if (request.backend() != api::ObjectWatchRequest::PTRACE)
    throw std::invalid_argument("code-cache startup requires launch --agent AGENT");
  StopInput stop(false);
  std::mutex output;
  tracer::object_watch::Watch watch(request, 1, std::move(argv), std::chrono::seconds(10),
      [&](auto event) {
        const std::lock_guard lock(output);
        event.set_sequence(event.probe_sequence());
        print_json(event);
      });
  {
    const std::lock_guard lock(output);
    const auto pid = watch.target_pid();
    if (!pid)
      throw std::runtime_error("target did not arm at the exec stop");
    std::cout << "{\"coverageStart\":\"exec-stop\",\"pid\":" << pid << "}" << std::endl;
  }
  while (watch.status().active() && !stop.requested()) {}
  for (;;) {
    const auto status = watch.stop();
    const std::lock_guard lock(output);
    print_json(status);
    if (!status.restoration_pending())
      break;
  }
  const auto exit = watch.target_exit_status();
  if (!exit)
    return 0; // Stopped the watch while the launched program remained live.
  return WIFEXITED(*exit) ? WEXITSTATUS(*exit) : 128 + WTERMSIG(*exit);
}
}

int main(int argc, char **argv) {
  try {
    constexpr std::string_view usage =
        "usage: clang-tracer-object-watch preview|watch PID DEFINITION.json\n"
        "       clang-tracer-object-watch launch [--agent AGENT [--loader-ready]] DEFINITION.json ELF [ARGS...]";
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
      std::cout << usage << '\n';
      return 0;
    }
    if (argc >= 6 && std::string_view(argv[1]) == "launch" && std::string_view(argv[2]) == "--agent") {
      const bool loader_ready = std::string_view(argv[4]) == "--loader-ready";
      const auto index = loader_ready ? 5 : 4;
      if (argc < index + 2)
        throw std::invalid_argument(std::string(usage));
      return run_native_launch(read_request(argv[index]),
          std::vector<std::string>(argv + index + 1, argv + argc), argv[3], loader_ready);
    }
    if (argc >= 4 && std::string_view(argv[1]) == "launch")
      return run_launch(read_request(argv[2]), std::vector<std::string>(argv + 3, argv + argc));
#if defined(__aarch64__)
    const bool valid_arguments = argc >= 4 && argc <= 6;
#else
    const bool valid_arguments = argc == 4;
#endif
    if (!valid_arguments || (std::string_view(argv[1]) != "preview" &&
                      std::string_view(argv[1]) != "watch"))
      throw std::invalid_argument(std::string(usage));
    const auto pid = process_id(argv[2]);
    auto request = read_request(argv[3]);
    const auto directory = tracer::proc::directory(pid);
    const auto start = tracer::proc::start_time(directory / "stat");
    const auto executable = std::filesystem::read_symlink(directory / "exe").string();
    const bool cache = request.backend() == api::ObjectWatchRequest::CODE_CACHE;
#if defined(__aarch64__)
    if (argc > 4 && (!cache || std::string_view(argv[1]) != "watch"))
      throw std::invalid_argument("agent and rendezvous are used by an AArch64 code-cache watch");
#endif
    if (std::string_view(argv[1]) == "preview") {
      const auto preview = tracer::object_watch::preview(request, pid, executable);
      if (cache)
        print_json(preview);
      else
        print_json(preview.plan());
      return 0;
    }
    StopInput stop;
#if defined(__aarch64__)
    if (cache) {
      if (argc < 5)
        throw std::invalid_argument("AArch64 code-cache watch requires an agent");
      run_code_cache(request, pid, executable, start, argv[4],
                     argc == 6 ? argv[5] : request.application_rendezvous(), stop);
      return 0;
    }
#else
    if (cache)
      throw std::invalid_argument("use the native REPL for a live x86-64 code-cache watch");
#endif
    std::mutex output;
    tracer::object_watch::Watch watch(request, 1, pid, start, executable,
        std::chrono::seconds(10), [&](auto event) {
          const std::lock_guard lock(output);
          event.set_sequence(event.probe_sequence());
          print_json(event);
        });
    {
      const std::lock_guard lock(output);
      print_json(watch.status());
    }
    while (watch.status().active() && !stop.requested()) {}
    for (;;) {
      const auto status = watch.stop();
      const std::lock_guard lock(output);
      print_json(status);
      if (!status.restoration_pending())
        break;
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer-object-watch: " << error.what() << '\n';
    return 1;
  }
}

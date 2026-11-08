#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
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
#include "thread_barrier.hpp"
#include "tracer.grpc.pb.h"

extern char **environ;

namespace {

using namespace std::chrono_literals;
namespace api = clangtracer::v1;

void set_rpc_deadline(grpc::ClientContext &context) {
  context.set_deadline(std::chrono::system_clock::now() + 15s);
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void require_failure_category(
    const grpc::Status &status, const std::string &label,
    std::initializer_list<std::string_view> categories) {
  require(!status.ok(), label + " unexpectedly succeeded");
  std::string detail = status.error_message();
  std::transform(detail.begin(), detail.end(), detail.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  for (const auto category : categories) {
    if (detail.find(category) != std::string::npos) {
      return;
    }
  }
  std::string expected;
  for (const auto category : categories) {
    if (!expected.empty()) {
      expected += " or ";
    }
    expected.append(category);
  }
  throw std::runtime_error(label + " failed for the wrong reason; expected " +
                           expected + ", got: " + status.error_message());
}

class ChildProcess {
public:
  ChildProcess() = default;
  explicit ChildProcess(pid_t pid) : pid_(pid) {}
  ~ChildProcess() { stop(); }
  ChildProcess(const ChildProcess &) = delete;
  ChildProcess &operator=(const ChildProcess &) = delete;
  ChildProcess(ChildProcess &&other) noexcept : pid_(other.pid_) {
    other.pid_ = -1;
  }
  ChildProcess &operator=(ChildProcess &&other) noexcept {
    if (this != &other) {
      stop();
      pid_ = other.pid_;
      other.pid_ = -1;
    }
    return *this;
  }

  pid_t pid() const { return pid_; }
  bool running() const {
    if (pid_ <= 0) {
      return false;
    }
    int status = 0;
    const auto result = ::waitpid(pid_, &status, WNOHANG);
    return result == 0;
  }

  void stop(std::chrono::milliseconds grace = 1s) noexcept {
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
    ::kill(pid_, SIGKILL);
    int status = 0;
    (void)::waitpid(pid_, &status, 0);
    pid_ = -1;
  }

private:
  pid_t pid_ = -1;
};

ChildProcess spawn(const std::vector<std::string> &arguments,
                   const std::string &log_path) {
  posix_spawn_file_actions_t actions;
  if (::posix_spawn_file_actions_init(&actions) != 0) {
    throw std::runtime_error("could not initialize child process file actions");
  }
  const int open_result = ::posix_spawn_file_actions_addopen(
      &actions, STDOUT_FILENO, log_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
      0600);
  const int dup_result = open_result == 0
                             ? ::posix_spawn_file_actions_adddup2(
                                   &actions, STDOUT_FILENO, STDERR_FILENO)
                             : open_result;
  if (dup_result != 0) {
    ::posix_spawn_file_actions_destroy(&actions);
    throw std::runtime_error(
        "could not configure child process output redirection");
  }
  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1);
  for (const auto &argument : arguments) {
    argv.push_back(const_cast<char *>(argument.c_str()));
  }
  argv.push_back(nullptr);
  pid_t pid = -1;
  const int result = ::posix_spawn(&pid, argv.front(), &actions, nullptr,
                                   argv.data(), ::environ);
  ::posix_spawn_file_actions_destroy(&actions);
  if (result != 0) {
    throw std::runtime_error("could not start child process (error " +
                             std::to_string(result) + ")");
  }
  return ChildProcess(pid);
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot read " + path.string());
  }
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::optional<std::uintptr_t>
find_target_entry(const std::filesystem::path &path,
                  std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
      long pid = 0;
      void *address = nullptr;
      if (std::sscanf(line.c_str(), "TARGET pid=%ld entry=%p", &pid,
                      &address) == 2 &&
          address) {
        return reinterpret_cast<std::uintptr_t>(address);
      }
    }
    std::this_thread::sleep_for(20ms);
  } while (std::chrono::steady_clock::now() < deadline);
  return std::nullopt;
}

std::vector<unsigned char>
read_process_memory(pid_t pid, std::uintptr_t address, std::size_t size) {
  std::vector<unsigned char> bytes(size);
  iovec local{bytes.data(), bytes.size()};
  iovec remote{reinterpret_cast<void *>(address), bytes.size()};
  const auto copied = ::tracer::linux_api::process_vm_readv(pid, &local, 1, &remote, 1, 0);
  require(copied == static_cast<ssize_t>(size),
          "could not read target function bytes");
  return bytes;
}

void save_proto(const std::filesystem::path &path,
                const google::protobuf::MessageLite &message) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output || !message.SerializeToOstream(&output)) {
    throw std::runtime_error("cannot save protobuf evidence " + path.string());
  }
}

void report(std::ofstream &output, const std::string &text) {
  output << text << '\n';
  output.flush();
}

api::ProbeSpec make_spec(const std::string &symbol, api::CallableKind kind,
                         api::ValueType result,
                         std::initializer_list<api::ValueType> arguments,
                         const std::string &callback) {
  api::ProbeSpec spec;
  spec.set_symbol(symbol);
  spec.set_callback_source(callback);
  auto *signature = spec.mutable_signature();
  signature->set_kind(kind);
  signature->set_result(result);
  for (const auto argument : arguments) {
    signature->add_arguments(argument);
  }
  return spec;
}

grpc::Status add_probe(api::Tracer::Stub &stub, const std::string &session_id,
                       const api::ProbeSpec &spec, api::Probe *response) {
  api::AddProbeRequest request;
  request.set_session_id(session_id);
  *request.mutable_spec() = spec;
  grpc::ClientContext context;
  set_rpc_deadline(context);
  return stub.AddProbe(&context, request, response);
}

api::Probe add_probe_ok(api::Tracer::Stub &stub, const std::string &session_id,
                        const api::ProbeSpec &spec,
                        const std::filesystem::path &evidence,
                        const std::string &label) {
  api::Probe response;
  const auto status = add_probe(stub, session_id, spec, &response);
  require(status.ok(), label + " AddProbe failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  return response;
}

api::Session attach_ok(api::Tracer::Stub &stub, pid_t pid,
                       const std::filesystem::path &evidence,
                       const std::string &label) {
  api::AttachRequest request;
  request.set_pid(static_cast<std::uint32_t>(pid));
  api::Session response;
  grpc::ClientContext context;
  set_rpc_deadline(context);
  const auto status = stub.Attach(&context, request, &response);
  require(status.ok(), label + " Attach failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  return response;
}

struct AttachAttempt {
  grpc::Status status;
  api::Session session;
};

AttachAttempt attach_once(api::Tracer::Stub &stub, pid_t pid) {
  api::AttachRequest request;
  request.set_pid(static_cast<std::uint32_t>(pid));
  AttachAttempt result;
  grpc::ClientContext context;
  set_rpc_deadline(context);
  result.status = stub.Attach(&context, request, &result.session);
  return result;
}

api::Session detach_ok(api::Tracer::Stub &stub, const std::string &session_id,
                       const std::filesystem::path &evidence,
                       const std::string &label) {
  api::DetachRequest request;
  request.set_session_id(session_id);
  api::Session response;
  grpc::ClientContext context;
  set_rpc_deadline(context);
  const auto status = stub.Detach(&context, request, &response);
  require(status.ok(), label + " Detach failed: " + status.error_message());
  save_proto(evidence / (label + ".pb"), response);
  return response;
}

std::vector<api::TraceEvent>
read_events(api::Tracer::Stub &stub, const api::WatchEventsRequest &request,
            std::size_t count, const std::filesystem::path &evidence) {
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() + 15s);
  auto reader = stub.WatchEvents(&context, request);
  std::vector<api::TraceEvent> events;
  api::TraceEvent event;
  while (events.size() < count && reader->Read(&event)) {
    events.push_back(event);
    save_proto(evidence / ("event-" + std::to_string(events.size()) + ".pb"),
               event);
  }
  if (events.size() == count) {
    context.TryCancel();
  }
  const auto status = reader->Finish();
  require(events.size() == count,
          "WatchEvents ended before requested event count: " +
              status.error_message());
  require(status.ok() || status.error_code() == grpc::StatusCode::CANCELLED,
          "WatchEvents failed: " + status.error_message());
  return events;
}

class EventCollector {
public:
  EventCollector(api::Tracer::Stub &stub, api::WatchEventsRequest request,
                 std::filesystem::path evidence)
      : evidence_(std::move(evidence)) {
    std::filesystem::create_directories(evidence_);
    context_.set_deadline(std::chrono::system_clock::now() + 30s);
    auto reader = stub.WatchEvents(&context_, request);
    worker_ = std::thread([this, reader = std::move(reader)]() mutable {
      api::TraceEvent event;
      std::size_t sequence = 0;
      while (reader->Read(&event)) {
        {
          std::lock_guard lock(mutex_);
          events_.push_back(event);
          ++sequence;
        }
        save_proto(evidence_ / ("event-" + std::to_string(sequence) + ".pb"),
                   event);
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

  bool wait_for_probe_ids(const std::vector<std::uint64_t> &probe_ids,
                          std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      const auto events = snapshot();
      bool complete = true;
      for (const auto probe_id : probe_ids) {
        complete &= std::any_of(events.begin(), events.end(),
                                [probe_id](const auto &event) {
                                  return event.probe_id() == probe_id;
                                });
      }
      if (complete) {
        return true;
      }
      std::this_thread::sleep_for(20ms);
    }
    return false;
  }

  void cancel() {
    if (worker_.joinable()) {
      context_.TryCancel();
      worker_.join();
      require(status_.ok() ||
                  status_.error_code() == grpc::StatusCode::CANCELLED,
              "event collector stream failed: " + status_.error_message());
    }
  }

private:
  std::filesystem::path evidence_;
  grpc::ClientContext context_;
  std::mutex mutex_;
  std::vector<api::TraceEvent> events_;
  grpc::Status status_;
  std::thread worker_;
};

api::ListSessionsResponse list_sessions(api::Tracer::Stub &stub) {
  api::ListSessionsRequest request;
  api::ListSessionsResponse response;
  grpc::ClientContext context;
  set_rpc_deadline(context);
  const auto status = stub.ListSessions(&context, request, &response);
  require(status.ok(), "ListSessions failed: " + status.error_message());
  return response;
}

void verify_callback_result(const api::TraceEvent &event) {
  long long expected = static_cast<long long>(event.arguments_size());
  for (const auto &argument : event.arguments()) {
    switch (argument.type()) {
    case api::INT32:
    case api::INT64:
      require(argument.data_case() == api::Value::kSignedValue,
              "signed argument has wrong oneof value");
      expected += argument.signed_value();
      break;
    case api::UINT64:
    case api::POINTER:
      require(argument.data_case() == api::Value::kUnsignedValue,
              "unsigned argument has wrong oneof value");
      expected += static_cast<long long>(argument.unsigned_value() &
                                         0x7fffffffffffffffULL);
      break;
    case api::FLOAT32:
    case api::FLOAT64:
      require(argument.data_case() == api::Value::kRealValue,
              "floating argument has wrong oneof value");
      expected += static_cast<long long>(argument.real_value() * 100.0);
      break;
    default:
      throw std::runtime_error("unexpected argument value type");
    }
  }
  if (event.object_address() != 0) {
    ++expected;
  }
  require(event.callback_value() == expected,
          "callback value disagrees with native argument mapping");
}

std::size_t verify_target_output(const std::filesystem::path &path) {
  std::istringstream input(read_file(path));
  std::string line;
  std::size_t checked = 0;
  while (std::getline(input, line)) {
    long pid = 0;
    void *entry = nullptr;
    int argument = 0;
    int free_result = 0;
    double mixed_result = 0;
    int method_result = 0;
    int long_method_result = 0;
    int const_result = 0;
    unsigned long static_result = 0;
    int namespaced_result = 0;
    long long int64_result = 0;
    float float32_result = 0;
    int pointer_matches = 0;
    int void_argument = 0;
    const int parsed = std::sscanf(
        line.c_str(),
        "TARGET pid=%ld entry=%p arg=%d free=%d mixed=%lf method=%d "
        "long_method=%d const=%d static=%lu namespaced=%d int64=%lld "
        "float32=%f pointer=%d void=%d",
        &pid, &entry, &argument, &free_result, &mixed_result, &method_result,
        &long_method_result, &const_result, &static_result, &namespaced_result,
        &int64_result, &float32_result, &pointer_matches, &void_argument);
    if (parsed != 14) {
      continue;
    }
    require(free_result == argument * 3 + 7,
            "target free function return changed");
    require(mixed_result == static_cast<double>(argument) * 2.0 + 0.25,
            "target mixed function return changed");
    require(method_result == argument + 11, "target method return changed");
    require(long_method_result == argument * 2,
            "target long overload return changed");
    require(const_result == argument, "target const method return changed");
    require(static_result == static_cast<unsigned long>(argument) + 17,
            "target static method return changed");
    require(namespaced_result == argument * 5 + 3,
            "namespaced function return changed");
    require(int64_result == static_cast<long long>(argument) + 31,
            "int64 return changed");
    require(float32_result == static_cast<float>(argument) + 0.75F,
            "float32 return changed");
    require(pointer_matches == 1, "pointer return changed");
    require(void_argument == argument, "void function did not run");
    ++checked;
  }
  require(checked >= 2, "too few target results were observed after detach");
  return checked;
}

void run(const std::filesystem::path &server_path,
         const std::filesystem::path &agent_path,
         const std::filesystem::path &target_path,
         const std::filesystem::path &output_dir, bool agent_logging) {
  std::filesystem::create_directories(output_dir);
  const auto agent_log = output_dir / "agent-diagnostics.log";
  if (agent_logging) {
    std::filesystem::remove(agent_log);
    require(::setenv("CLANG_TRACER_AGENT_LOG", agent_log.c_str(), 1) == 0,
            "cannot configure agent diagnostics");
  }
  std::ofstream report_file(output_dir / "report.txt", std::ios::trunc);
  require(static_cast<bool>(report_file), "cannot create report.txt");

  const auto socket_path = (output_dir / "tracer.sock").string();
  const auto endpoint = "unix://" + socket_path;
  ChildProcess server =
      spawn({server_path.string(), "--agent", agent_path.string(), "--listen",
             endpoint, "--timeout", "10"},
            (output_dir / "server.log").string());
  auto channel =
      grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials());
  const auto ready_by = std::chrono::system_clock::now() + 10s;
  while (!channel->WaitForConnected(std::chrono::system_clock::now() + 100ms)) {
    require(server.running(), "server exited before becoming ready");
    require(std::chrono::system_clock::now() < ready_by,
            "server did not become ready");
  }
  auto stub = api::Tracer::NewStub(channel);

  ChildProcess target_a =
      spawn({target_path.string()}, (output_dir / "target-a.log").string());
  ChildProcess target_b =
      spawn({target_path.string()}, (output_dir / "target-b.log").string());
  std::this_thread::sleep_for(250ms);
  require(target_a.running() && target_b.running(),
          "fixture target failed to start");
  const auto target_a_entry =
      find_target_entry(output_dir / "target-a.log", 2s);
  require(target_a_entry.has_value(),
          "target did not report traced_work address");
  const auto target_a_original_bytes =
      read_process_memory(target_a.pid(), *target_a_entry, 16);

  const std::string callback =
      read_file(std::filesystem::path(__FILE__).parent_path().parent_path() /
                "tests/fixtures/probe.cpp");
  ThreadBarrier attach_gate(3);
  auto concurrent_attach = [&]() {
    attach_gate.arrive_and_wait();
    return attach_once(*stub, target_a.pid());
  };
  auto attach_one = std::async(std::launch::async, concurrent_attach);
  auto attach_two = std::async(std::launch::async, concurrent_attach);
  attach_gate.arrive_and_wait();
  auto attach_result_one = attach_one.get();
  auto attach_result_two = attach_two.get();
  const bool first_won = attach_result_one.status.ok();
  const bool second_won = attach_result_two.status.ok();
  require(first_won != second_won,
          "concurrent Attach did not produce exactly one winner");
  const auto &losing_attach = first_won ? attach_result_two : attach_result_one;
  require(losing_attach.status.error_code() == grpc::StatusCode::ALREADY_EXISTS,
          "losing concurrent Attach did not report ALREADY_EXISTS");
  const auto session_a =
      first_won ? attach_result_one.session : attach_result_two.session;
  require(session_a.pid() == static_cast<std::uint32_t>(target_a.pid()) &&
              !session_a.id().empty(),
          "concurrent Attach winner returned an invalid session");
  save_proto(output_dir / "attach-a.pb", session_a);
  report(report_file, "concurrent Attach race produced one session and one "
                      "ALREADY_EXISTS response");
  const auto session_b =
      attach_ok(*stub, target_b.pid(), output_dir, "attach-b");
  require(session_a.id() != session_b.id(),
          "two PIDs received the same session ID");
  api::WatchEventsRequest watch_request;
  watch_request.add_session_ids(session_a.id());
  watch_request.add_session_ids(session_b.id());
  EventCollector collector(*stub, watch_request, output_dir / "events");

  api::AttachRequest duplicate_request;
  duplicate_request.set_pid(static_cast<std::uint32_t>(target_a.pid()));
  api::Session ignored_session;
  grpc::ClientContext duplicate_context;
  set_rpc_deadline(duplicate_context);
  const auto duplicate_status =
      stub->Attach(&duplicate_context, duplicate_request, &ignored_session);
  require(duplicate_status.error_code() == grpc::StatusCode::ALREADY_EXISTS,
          "duplicate PID Attach did not return ALREADY_EXISTS");
  report(report_file,
         "duplicate PID rejected: " + duplicate_status.error_message());

  const std::string logging_callback = R"cpp(
    extern "C" long long trace_callback(const TraceContext &context) {
      if (context.call_id == 1) context.log("legacy callback");
      return 1 + context.arguments[0].signed_value;
    }
  )cpp";
  const auto free_spec = make_spec(
      "traced_work", api::FUNCTION, api::INT32, {api::INT32},
      agent_logging ? logging_callback : callback);
  auto free_a =
      add_probe_ok(*stub, session_a.id(), free_spec, output_dir, "free-a");
  require(free_a.entry_address() == *target_a_entry,
          "server resolved traced_work to an unexpected address");
  (void)add_probe_ok(*stub, session_b.id(), free_spec, output_dir, "free-b");
  const auto method_spec = make_spec("_ZN7Fixture10overloadedEi", api::METHOD,
                                     api::INT32, {api::INT32}, callback);
  const auto method_a =
      add_probe_ok(*stub, session_a.id(), method_spec, output_dir, "method-a");
  const auto long_method_spec =
      make_spec("_ZN7Fixture10overloadedEl", api::METHOD, api::INT32,
                {api::INT64}, callback);
  const auto long_method_a = add_probe_ok(
      *stub, session_a.id(), long_method_spec, output_dir, "long-method-a");
  const auto namespaced_spec =
      make_spec("_ZN7fixture15namespaced_workEi", api::FUNCTION, api::INT32,
                {api::INT32}, callback);
  const auto namespaced_a = add_probe_ok(*stub, session_a.id(), namespaced_spec,
                                         output_dir, "namespaced-a");
  const auto mixed_spec = make_spec("traced_mixed", api::FUNCTION, api::FLOAT64,
                                    {api::FLOAT64, api::FLOAT64}, callback);
  const auto mixed_a =
      add_probe_ok(*stub, session_a.id(), mixed_spec, output_dir, "mixed-a");
  const auto const_spec = make_spec("_ZNK7Fixture7inspectEd", api::METHOD,
                                    api::FLOAT64, {api::FLOAT64}, callback);
  const auto const_a = add_probe_ok(*stub, session_a.id(), const_spec,
                                    output_dir, "const-method-a");
  const auto static_spec =
      make_spec("_ZN7Fixture11static_workEm", api::FUNCTION, api::UINT64,
                {api::UINT64}, callback);
  const auto static_a = add_probe_ok(*stub, session_a.id(), static_spec,
                                     output_dir, "static-method-a");
  const auto int64_spec = make_spec("traced_int64", api::FUNCTION, api::INT64,
                                    {api::INT64}, callback);
  const auto int64_a =
      add_probe_ok(*stub, session_a.id(), int64_spec, output_dir, "int64-a");
  const auto float32_spec = make_spec("traced_float32", api::FUNCTION,
                                      api::FLOAT32, {api::FLOAT32}, callback);
  const auto float32_a = add_probe_ok(*stub, session_a.id(), float32_spec,
                                      output_dir, "float32-a");
  const auto pointer_spec = make_spec("traced_pointer", api::FUNCTION,
                                      api::POINTER, {api::POINTER}, callback);
  const auto pointer_a = add_probe_ok(*stub, session_a.id(), pointer_spec,
                                      output_dir, "pointer-a");
  const auto void_spec = make_spec("traced_void", api::FUNCTION, api::VOID,
                                   {api::INT32}, callback);
  auto void_a =
      add_probe_ok(*stub, session_a.id(), void_spec, output_dir, "void-a");

  const auto replace_probe = [&](const api::ProbeSpec &spec, api::Probe &probe) {
    api::Probe replacement;
    const auto status = add_probe(*stub, session_a.id(), spec, &replacement);
    require(status.ok(), "replacement probe failed: " + status.error_message());
    require(replacement.active() && replacement.id() != probe.id() &&
                replacement.entry_address() == probe.entry_address(),
            "replacement did not activate a new probe at the same entry");
    probe = replacement;
  };
  replace_probe(free_spec, free_a);
  // The PIE fixture needs relocation again when replacing this probe.
  replace_probe(void_spec, void_a);
  report(report_file, "same-session probe replaced with a new version");

  api::ProbeSpec missing = free_spec;
  missing.set_symbol("missing_target_symbol");
  api::Probe missing_response;
  const auto missing_status =
      add_probe(*stub, session_a.id(), missing, &missing_response);
  require_failure_category(missing_status, "missing symbol",
                           {"symbol was not found"});
  report(report_file,
         "missing symbol rejected: " + missing_status.error_message());

  api::ProbeSpec invalid_source = free_spec;
  invalid_source.set_symbol("invalid_source_target");
  invalid_source.set_callback_source(
      "extern \"C\" long long trace_callback(const TraceContext&) { return "
      ";\n");
  api::Probe invalid_response;
  const auto invalid_status =
      add_probe(*stub, session_a.id(), invalid_source, &invalid_response);
  require_failure_category(invalid_status, "malformed callback source",
                           {"expected", "syntax", "parse"});
  report(report_file,
         "invalid callback source rejected: " + invalid_status.error_message());

  api::ProbeSpec wrong_callback = free_spec;
  wrong_callback.set_symbol("wrong_callback_target");
  wrong_callback.set_callback_source(
      "extern \"C\" void trace_callback(int) {}\n");
  api::Probe wrong_response;
  const auto wrong_status =
      add_probe(*stub, session_a.id(), wrong_callback, &wrong_response);
  require_failure_category(wrong_status, "wrong callback signature",
                           {"trace_callback must be"});
  report(report_file,
         "wrong callback signature rejected: " + wrong_status.error_message());

  api::ProbeSpec invalid_compile_option = free_spec;
  invalid_compile_option.add_compile_options("");
  api::Probe invalid_compile_option_response;
  const auto invalid_compile_option_status =
      add_probe(*stub, session_a.id(), invalid_compile_option,
                &invalid_compile_option_response);
  require_failure_category(invalid_compile_option_status,
                           "empty compiler option",
                           {"compiler options must be nonempty"});
  report(report_file, "empty compiler option rejected before native preparation");

  api::ValidateProbeRequest invalid_validation_option;
  invalid_validation_option.set_selector("target(int)");
  invalid_validation_option.set_callback_source("struct Interceptor {};");
  invalid_validation_option.add_compile_options("");
  api::ValidationReport invalid_validation_response;
  grpc::ClientContext invalid_validation_context;
  set_rpc_deadline(invalid_validation_context);
  const auto invalid_validation_status = stub->ValidateProbe(
      &invalid_validation_context, invalid_validation_option,
      &invalid_validation_response);
  require(invalid_validation_status.error_code() ==
              grpc::StatusCode::INVALID_ARGUMENT &&
              invalid_validation_status.error_message().find("compiler options") !=
                  std::string::npos,
          "ValidateProbe accepted an empty compiler option");
  report(report_file,
         "ValidateProbe rejects malformed compiler options before resolution");

  const std::vector<std::string> unsafe_preflight_options = {
      "@/tmp/clang-tracer-args.rsp", "--config=/tmp/clang-tracer.cfg",
      "-fplugin=/tmp/plugin.so", "-fpass-plugin=/tmp/pass-plugin.so",
      "-Xclang", "-Xclang=-load", "-Wp,-load,/tmp/plugin.so",
      "-Xpreprocessor", "--driver-mode=cl",
      "/clang:-fplugin=/tmp/plugin.so", "--config-user-dir=/tmp/clang",
      "-mllvm", "-target=x86_64-unknown-linux-gnu", "-m32",
      "-fpack-struct", "-include-pch=/tmp/header.pch", "-imacros",
      "-c",
      "-fsyntax-only"};
  for (const auto &option : unsafe_preflight_options) {
    api::ValidateProbeRequest request;
    request.set_selector("target(int)");
    request.set_callback_source("struct Interceptor {};");
    request.add_compile_options(option);
    api::ValidationReport response;
    grpc::ClientContext context;
    set_rpc_deadline(context);
    const auto status = stub->ValidateProbe(&context, request, &response);
    require(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT &&
                status.error_message().find("compiler options") !=
                    std::string::npos,
            "ValidateProbe accepted unsafe preflight option: " + option);
  }
  report(report_file,
         "ValidateProbe rejects plugin, opaque, response-file, and ABI options before resolution");

  api::ProbeSpec unsafe = free_spec;
  unsafe.set_symbol("unsafe_target");
  api::Probe unsafe_response;
  const auto unsafe_status =
      add_probe(*stub, session_a.id(), unsafe, &unsafe_response);
  require_failure_category(unsafe_status, "unsafe prologue",
                           {"unsupported or pc-relative"});
  report(report_file,
         "unsafe prologue rejected: " + unsafe_status.error_message());

  const auto rejection_sessions = list_sessions(*stub);
  bool session_still_attached = false;
  for (const auto &session : rejection_sessions.sessions()) {
    if (session.id() == session_a.id()) {
      session_still_attached = session.state() == api::Session::ATTACHED;
    }
  }
  require(session_still_attached,
          "known AddProbe rejections changed the session state");
  save_proto(output_dir / "after-rejections.pb", rejection_sessions);
  report(report_file, "all expected rejections preserved the attached session");

  const auto events_ready = collector.wait_for_probe_ids(
      {free_a.id(), method_a.id(), long_method_a.id(), namespaced_a.id(),
       mixed_a.id(), const_a.id(), static_a.id(), int64_a.id(), float32_a.id(),
       pointer_a.id(), void_a.id()},
      10s);
  require(events_ready,
          "watch stream did not receive events for every successful probe");
  collector.cancel();
  const auto events = collector.snapshot();
  bool saw_free = false;
  bool saw_method = false;
  bool saw_long_method = false;
  bool saw_namespaced = false;
  bool saw_double = false;
  bool saw_float = false;
  bool saw_int64 = false;
  bool saw_uint64 = false;
  bool saw_pointer = false;
  for (const auto &event : events) {
    require(event.protocol_version() == 2, "event protocol version mismatch");
    require(event.pid() == static_cast<std::uint32_t>(target_a.pid()) ||
                event.pid() == static_cast<std::uint32_t>(target_b.pid()),
            "event came from an unrelated PID");
    require(event.tid() > 0, "event has invalid native thread ID");
    verify_callback_result(event);
    if (event.probe_id() == free_a.id()) {
      saw_free = true;
      require(event.object_address() == 0,
              "free function unexpectedly captured this");
    }
    if (event.probe_id() == method_a.id()) {
      saw_method = true;
      require(event.object_address() != 0, "method event did not capture this");
    }
    if (event.probe_id() == long_method_a.id()) {
      saw_long_method = true;
      require(event.object_address() != 0,
              "long overload did not capture this");
    }
    if (event.probe_id() == namespaced_a.id()) {
      saw_namespaced = true;
      require(event.object_address() == 0,
              "namespaced free function captured this");
    }
    for (const auto &argument : event.arguments()) {
      saw_double |= argument.type() == api::FLOAT64;
      saw_float |= argument.type() == api::FLOAT32;
      saw_int64 |= argument.type() == api::INT64;
      saw_uint64 |= argument.type() == api::UINT64;
      saw_pointer |= argument.type() == api::POINTER;
    }
  }
  require(saw_free && saw_method && saw_long_method && saw_namespaced &&
              saw_double && saw_float && saw_int64 && saw_uint64 && saw_pointer,
          "events did not cover overloads, mangled free calls, and every "
          "scalar representation");
  report(report_file,
         "captured v2 events with all scalar argument forms and object values");

  api::RemoveProbeRequest remove_request;
  remove_request.set_session_id(session_a.id());
  remove_request.set_probe_id(method_a.id());
  api::Probe removed;
  grpc::ClientContext remove_context;
  set_rpc_deadline(remove_context);
  const auto remove_status =
      stub->RemoveProbe(&remove_context, remove_request, &removed);
  require(remove_status.ok(),
          "RemoveProbe failed: " + remove_status.error_message());
  save_proto(output_dir / "remove-method-a.pb", removed);

  const auto detached_a =
      detach_ok(*stub, session_a.id(), output_dir, "detach-a");
  require(detached_a.state() == api::Session::DETACHED,
          "first session did not detach");
  require(target_b.running(), "detaching first PID affected the second target");

  const auto rebound_session =
      attach_ok(*stub, target_a.pid(), output_dir, "reattach-a");
  require(rebound_session.id() != session_a.id(),
          "reattach reused the detached session ID");
  auto rebound_spec = free_spec;
  if (agent_logging) {
    rebound_spec.set_callback_source(R"cpp(
      class Interceptor {
      public:
        long long pre(const Call &call) {
          if (call.id() % 2 == 0) throw 7;
          call.log("class\ncallback", tracer::LogLevel::Warning);
          return 1LL + call.arg<int>(0);
        }
      };
    )cpp");
  }
  const auto rebound_probe = add_probe_ok(
      *stub, rebound_session.id(), rebound_spec, output_dir, "reattach-probe-a");
  api::WatchEventsRequest rebound_request;
  rebound_request.add_session_ids(rebound_session.id());
  std::filesystem::create_directories(output_dir / "reattach");
  const auto rebound_events =
      read_events(*stub, rebound_request, 2, output_dir / "reattach");
  for (const auto &event : rebound_events) {
    require(event.probe_id() == rebound_probe.id(),
            "reattached PID emitted an unexpected probe");
    require(event.pid() == static_cast<std::uint32_t>(target_a.pid()),
            "reattached probe event came from wrong PID");
    verify_callback_result(event);
  }
  report(report_file, "same live PID detached and reattached with a fresh "
                      "session and working hook");
  if (agent_logging) {
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    std::string lines;
    do {
      lines = read_file(agent_log);
      if (lines.find("message=class?callback\n") != std::string::npos &&
          lines.find("stage=runtime outcome=degraded") != std::string::npos)
        break;
      std::this_thread::sleep_for(50ms);
    } while (std::chrono::steady_clock::now() < deadline);
    for (const auto *expected : {"stage=compile_callback outcome=failed",
                                 "stage=resolve_symbol outcome=failed",
                                 "stage=publish_probe outcome=ok",
                                 "operation=stop stage=drain outcome=ok",
                                 "stage=runtime outcome=degraded",
                                 "message=legacy callback\n",
                                 "message=class?callback\n"})
      require(lines.find(expected) != std::string::npos,
              std::string("missing agent diagnostic: ") + expected);
  }

  api::WatchEventsRequest continue_request;
  continue_request.add_session_ids(session_b.id());
  std::filesystem::create_directories(output_dir / "after-detach");
  const auto continue_events =
      read_events(*stub, continue_request, 3, output_dir / "after-detach");
  for (const auto &event : continue_events) {
    require(event.session_id() == session_b.id(),
            "second session stream leaked another PID");
  }
  report(report_file,
         "second PID continued producing events after first PID detached");

  const auto detached_b =
      detach_ok(*stub, session_b.id(), output_dir, "detach-b");
  require(detached_b.state() == api::Session::DETACHED,
          "second session did not detach");
  std::this_thread::sleep_for(450ms);
  (void)verify_target_output(output_dir / "target-a.log");
  verify_target_output(output_dir / "target-b.log");
  require(target_a.running() && target_b.running(),
          "fixture target exited during detach");

  ChildProcess exiting_target =
      spawn({target_path.string()}, (output_dir / "target-exit.log").string());
  std::this_thread::sleep_for(100ms);
  const auto exiting_session =
      attach_ok(*stub, exiting_target.pid(), output_dir, "attach-exit");
  exiting_target.stop();
  std::this_thread::sleep_for(300ms);
  const auto sessions = list_sessions(*stub);
  bool observed_exit = false;
  for (const auto &session : sessions.sessions()) {
    if (session.id() == exiting_session.id()) {
      observed_exit = session.state() == api::Session::EXITED;
    }
  }
  require(observed_exit,
          "target exit was not isolated and reflected in session state");
  save_proto(output_dir / "list-sessions.pb", sessions);
  report(report_file, "target exit reflected in session list; unrelated "
                      "sessions detached cleanly");
  const auto target_a_rows_before_shutdown =
      verify_target_output(output_dir / "target-a.log");
  server.stop(12s);
  std::this_thread::sleep_for(450ms);
  const auto target_a_rows_after_shutdown =
      verify_target_output(output_dir / "target-a.log");
  require(target_a_rows_after_shutdown > target_a_rows_before_shutdown,
          "target did not continue producing correct results after server "
          "shutdown");
  require(read_process_memory(target_a.pid(), *target_a_entry,
                              target_a_original_bytes.size()) ==
              target_a_original_bytes,
          "server shutdown did not restore original traced_work bytes");
  require(target_a.running() && target_b.running(),
          "server shutdown terminated a fixture target");
  report(report_file,
         "server shutdown restored the active hook and left targets running");
  report(report_file, "integration passed");
}

} // namespace

int main(int argc, char **argv) {
  try {
    std::filesystem::path server;
    std::filesystem::path agent;
    std::filesystem::path target;
    std::filesystem::path output_dir;
    bool agent_logging = false;
    for (int i = 1; i < argc; ++i) {
      const std::string key = argv[i];
      if (key == "--agent-logging") {
        agent_logging = true;
        continue;
      }
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + key);
      }
      const std::string value = argv[++i];
      if (key == "--server") {
        server = value;
      } else if (key == "--agent") {
        agent = value;
      } else if (key == "--target") {
        target = value;
      } else if (key == "--output-dir") {
        output_dir = value;
      } else {
        throw std::runtime_error("unknown option: " + key);
      }
    }
    require(!server.empty() && !agent.empty() && !target.empty() &&
                !output_dir.empty(),
            "usage: server-integration --server PATH --agent PATH --target "
            "PATH --output-dir DIR [--agent-logging]");
    run(server, agent, target, output_dir, agent_logging);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "server integration failed: " << error.what() << '\n';
    return 1;
  }
}

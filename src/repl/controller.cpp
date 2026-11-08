#include "controller.hpp"
#include "local_perf.hpp"
#include "tracer/instruction_trace.h"
#include "../prepared_contract.hpp"
#include "../proc_identity.hpp"
#include "../sqlite_events.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>

#include <grpcpp/grpcpp.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <fcntl.h>
#include <unistd.h>

#include "background_events.hpp"

extern char **environ;

namespace tracer_repl {
namespace api = clangtracer::v1;
static_assert(std::atomic<bool>::is_always_lock_free,
              "signal cancellation requires lock-free bool");
std::atomic<bool> interrupted{false};

namespace {
std::string printable_proc_text(std::string value) {
  for (char &ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    if (byte < 32 || byte == 127)
      ch = '?';
  }
  return value;
}

std::size_t memory_scalar_size(std::string_view type) {
  if (type == "u8" || type == "i8")
    return 1;
  if (type == "u16" || type == "i16")
    return 2;
  if (type == "u32" || type == "i32" || type == "f32")
    return 4;
  if (type == "u64" || type == "i64" || type == "f64")
    return 8;
  throw std::invalid_argument("expected u8/u16/u32/u64, i8/i16/i32/i64, f32 or f64");
}

template <typename Float, typename Bits>
void print_memory_float(std::ostream &out, Bits bits) {
  static_assert(sizeof(Float) == sizeof(Bits) && std::numeric_limits<Float>::is_iec559);
  Float value;
  std::memcpy(&value, &bits, sizeof(value));
  out << std::setprecision(std::numeric_limits<Float>::max_digits10) << value;
}

void print_memory_scalar(std::ostream &out, std::uint64_t address,
                         std::string_view bytes, std::string_view type) {
  std::uint64_t bits = 0;
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bits |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[i])) << (8 * i);
  std::ostringstream result;
  result << "0x" << std::hex << address << std::dec << ' ' << type << '=';
  if (type == "f32") {
    print_memory_float<float>(result, static_cast<std::uint32_t>(bits));
  } else if (type == "f64") {
    print_memory_float<double>(result, bits);
  } else if (type.front() == 'i') {
    const auto width = bytes.size() * 8;
    const auto mask = UINT64_MAX >> (64 - width);
    const auto sign = std::uint64_t{1} << (width - 1);
    const auto value = bits & sign ? -1 - static_cast<std::int64_t>(mask ^ bits)
                                   : static_cast<std::int64_t>(bits);
    result << value;
  } else {
    result << bits;
  }
  out << result.str() << '\n';
}

void print_memory_dump(std::ostream &out, std::uint64_t address, std::string_view bytes) {
  constexpr std::size_t row_width = 16;
  std::ostringstream dump;
  dump << std::hex << std::setfill('0');
  for (std::size_t offset = 0; offset < bytes.size(); offset += row_width) {
    const auto row = bytes.substr(offset, row_width);
    dump << "0x" << address + offset << "  ";
    for (std::size_t i = 0; i < row_width; ++i) {
      if (i < row.size())
        dump << std::setw(2) << static_cast<unsigned>(static_cast<unsigned char>(row[i])) << ' ';
      else
        dump << "   ";
    }
    dump << " |";
    for (const unsigned char ch : row)
      dump << (ch >= 32 && ch <= 126 ? static_cast<char>(ch) : '.');
    dump << "|\n";
  }
  out << dump.str();
}

std::pair<std::string, std::string> parse_memory_pattern(std::string_view text) {
  const auto parse_bytes = [](std::string_view source, bool wildcards) {
    std::istringstream input{std::string(source)};
    std::string token, bytes, mask;
    while (input >> token) {
      if (token.size() != 2)
        throw std::invalid_argument("memory pattern needs two hex digits or wildcards per byte");
      if (bytes.size() == 256)
        throw std::invalid_argument("memory pattern must be at most 256 bytes");
      unsigned value = 0, bits = 0;
      for (const unsigned char ch : token) {
        value <<= 4;
        bits <<= 4;
        if (wildcards && ch == '?')
          continue;
        const auto nibble = std::string_view("0123456789abcdef").find(
            static_cast<char>(std::tolower(ch)));
        if (nibble == std::string_view::npos)
          throw std::invalid_argument("invalid hex digit in memory pattern or mask");
        value |= static_cast<unsigned>(nibble);
        bits |= 0xf;
      }
      bytes.push_back(static_cast<char>(value));
      mask.push_back(static_cast<char>(bits));
    }
    if (bytes.empty())
      throw std::invalid_argument("memory pattern and explicit mask must be nonempty");
    return std::make_pair(std::move(bytes), std::move(mask));
  };
  const auto colon = text.find(':');
  auto [pattern, mask] = parse_bytes(text.substr(0, colon), true);
  if (colon != std::string_view::npos) {
    const auto explicit_mask = parse_bytes(text.substr(colon + 1), false).first;
    if (explicit_mask.size() != pattern.size())
      throw std::invalid_argument("memory pattern and explicit mask must have the same length");
    for (std::size_t i = 0; i < mask.size(); ++i)
      mask[i] = static_cast<char>(mask[i] & explicit_mask[i]);
  }
  return {std::move(pattern), std::move(mask)};
}

void save_bulk_manifest(const std::string &path, const api::BulkTraceRules &rules) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0)
    throw std::system_error(errno, std::generic_category(),
                            "cannot create new bulk trace manifest " + path);
  google::protobuf::io::FileOutputStream stream(fd);
  stream.SetCloseOnDelete(true);
  const bool printed = google::protobuf::TextFormat::Print(rules, &stream);
  stream.SetCloseOnDelete(false);
  const bool closed = stream.Close();
  if (!printed || !closed) {
    ::unlink(path.c_str());
    throw std::runtime_error("cannot write bulk trace manifest: " + path);
  }
}

api::BulkTraceRules read_bulk_manifest(const std::string &path) {
  api::BulkTraceRules rules;
  if (!google::protobuf::TextFormat::ParseFromString(read_source(path), &rules))
    throw std::invalid_argument("invalid bulk trace manifest: " + path);
  return rules;
}

std::string new_bulk_batch_id() {
  std::random_device random;
  std::uniform_int_distribution<unsigned> nibble(0, 15);
  std::string id(32, '0');
  for (auto &ch : id)
    ch = "0123456789abcdef"[nibble(random)];
  return id;
}

void print_bulk_report(std::ostream &out, const char *operation,
                       const api::BulkTraceReport &report) {
  for (const auto &row : report.entries()) {
    const auto &function = row.function();
    out << api::BulkTraceEntry::Status_Name(row.status()) << ' '
        << std::quoted(printable_proc_text(function.selector().empty()
                           ? function.linkage_symbol() : function.selector()));
    if (!function.module_path().empty())
      out << " module=" << std::quoted(printable_proc_text(function.module_path()));
    if (row.has_probe())
      out << " probe=" << row.probe().id()
          << " restored=" << (row.probe().original_bytes_restored() ? "true" : "false");
    if (!row.diagnostic().empty())
      out << " reason=" << std::quoted(printable_proc_text(row.diagnostic()));
    out << '\n';
  }
  for (const auto &selector : report.unmatched_exact_selectors())
    out << "UNMATCHED " << std::quoted(printable_proc_text(selector)) << '\n';
  out << "bulk " << operation << " batch=" << report.batch_id()
      << " complete=" << (report.complete() ? "true" : "false");
  if (report.has_available_slots())
    out << " available=" << report.available_slots();
  out << '\n';
}

template <class Response, class Call>
Response unary(Call call, unsigned timeout = 30) {
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(timeout));
  Response response;
  const auto status = call(context, response);
  if (!status.ok()) {
    throw std::runtime_error("RPC " + std::to_string(status.error_code()) +
                             ": " + status.error_message());
  }
  return response;
}

class Cancellation {
  std::atomic<bool> done_{false};
  std::thread thread_;

public:
  explicit Cancellation(grpc::ClientContext &context)
      : thread_([this, &context] {
          while (!done_.load()) {
            if (interrupted) {
              context.TryCancel();
              return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
        }) {}

  ~Cancellation() {
    done_.store(true);
    thread_.join();
  }
};

void resolution_options(api::ResolveProbeRequest &request,
                        const Command &command) {
  request.set_module(command.option("--module"));
  request.set_compilation_database(command.option("--compdb"));
  auto includes = command.options.find("--include");
  if (includes != command.options.end()) {
    for (const auto &value : includes->second) {
      request.add_include_headers(value);
    }
  }
  auto options = command.options.find("--compile-option");
  if (options != command.options.end()) {
    for (const auto &value : options->second) {
      request.add_compile_options(value);
    }
  }
  if (!request.compilation_database().empty() &&
      request.include_headers().empty()) {
    throw std::invalid_argument(
        "--compdb requires --include with a server-host header path");
  }
}

std::string compact_selector(const std::string &value) {
  std::string result;
  bool space = false;
  const auto identifier = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
  for (unsigned char c : value) {
    if (std::isspace(c)) {
      space = true;
      continue;
    }
    if (space && !result.empty() && identifier(result.back()) && identifier(c))
      result += ' ';
    result += static_cast<char>(c);
    space = false;
  }
  return result;
}

enum class PreparedMode { off, prefer, require };

template <class Request, class Packages>
PreparedMode prepared_options(Request &request, const Command &options,
                              const Packages &packages,
                              std::ostream &out) {
  const auto selector = compact_selector(request.selector());
  auto directory = options.option("--prepared");
  if (!options.has("--prepared")) {
    const auto package = packages.find(selector);
    if (package != packages.end()) directory = package->second.directory;
  }
  const auto mode = options.option("--prepared-mode", directory.empty() ? "off" : "prefer");
  if (mode == "off") return PreparedMode::off;
  if (mode != "require" && mode != "prefer")
    throw std::invalid_argument("--prepared-mode must be off, prefer or require");
  try {
    if (directory.empty())
      throw std::invalid_argument(
          "No prepared package for this selector. Run %probe prepare or pass --prepared DIRECTORY.");
    const auto file = std::filesystem::path(directory) / "prepared.pb";
    if (std::filesystem::file_size(file) > 65536)
      throw std::runtime_error("prepared metadata exceeds 64 KiB");
    std::ifstream input(file, std::ios::binary);
    api::PreparedInjector prepared;
    if (!input || !prepared.ParseFromIstream(&input))
      throw std::runtime_error("invalid prepared contract");
    if (prepared.format_version() == 3)
      throw std::runtime_error("legacy typed prepared package requires regeneration");
    const bool typed = prepared.format_version() == tracer::prepared::typed_version;
    const bool pinned = (prepared.format_version() == 1 || typed) &&
                        prepared.module_sha256().size() == 32 && !prepared.module_name().empty();
    const bool source_only = (prepared.format_version() == 2 || typed) &&
                             prepared.module_sha256().empty() && prepared.module_name().empty();
    if ((!pinned && !source_only) ||
        compact_selector(prepared.selector()) != selector)
      throw std::runtime_error("invalid prepared contract or selector mismatch");
    *request.mutable_prepared() = prepared;
    request.set_selector(prepared.selector());
    return mode == "require" ? PreparedMode::require : PreparedMode::prefer;
  } catch (const std::exception &error) {
    if (mode == "require") throw;
    out << "prepared metadata unavailable; falling back to DWARF: " << error.what() << '\n';
    return PreparedMode::off;
  }
}
} // namespace

Controller::Controller(Settings settings, std::ostream &out)
    : settings_(std::move(settings)), out_(out) {
  if (!settings_.max_events || !settings_.max_bytes) {
    throw std::invalid_argument("event and byte limits must be positive");
  }
}

Controller::~Controller() {
  try {
    shutdown();
  } catch (const std::exception &error) {
    std::cerr << "cleanup: " << error.what() << '\n';
  }
}

void Controller::connect(const std::string &endpoint) {
  grpc::ChannelArguments arguments;
  arguments.SetMaxReceiveMessageSize(32 * 1024 * 1024);
  arguments.SetMaxSendMessageSize(32 * 1024 * 1024);
  auto next = api::Tracer::NewStub(grpc::CreateCustomChannel(
      endpoint, grpc::InsecureChannelCredentials(), arguments));
  api::ListSessionsRequest request;
  unary<api::ListSessionsResponse>(
      [&](auto &context, auto &response) {
        return next->ListSessions(&context, request, &response);
      },
      2);
  endpoint_ = endpoint;
  stop_background();
  stub_ = std::move(next);
  selected_ = {};
  ++connection_;
}

void Controller::start_server(const Command &command) {
  if (server_.running()) {
    throw std::runtime_error("this console already owns a running server");
  }
  require_no_owned_targets();
  if (!owned_sessions_.empty() || !pending_attaches_.empty()) {
    throw std::runtime_error(
        "resolve console-created or uncertain sessions before changing server");
  }
  server_.start(command.option("--executable", settings_.server),
                command.option("--agent", settings_.agent),
                settings_.timeout_seconds);
  connect(server_.endpoint());
  out_ << "server ready endpoint=" << endpoint_
       << " ownership=this-console pid=" << server_.pid() << '\n';
}

void Controller::disconnect() {
  stop_background();
  stub_.reset();
  selected_ = {};
  ++connection_;
  endpoint_.clear();
}

std::string Controller::reconcile_attaches(bool *snapshot_available) {
  if (snapshot_available)
    *snapshot_available = false;
  if (pending_attaches_.empty())
    return {};
  std::ostringstream recovery;
  recovery << "endpoint=" << endpoint_;
  for (const auto &entry : pending_attaches_)
    recovery << " pid=" << entry.first;
  if (!stub_)
    return recovery.str() + " (disconnected; inspect this endpoint manually)";
  try {
    api::ListSessionsRequest request;
    const auto current = unary<api::ListSessionsResponse>(
        [&](auto &c, auto &r) { return stub_->ListSessions(&c, request, &r); },
        2);
    if (snapshot_available)
      *snapshot_available = true;
    for (auto &entry : pending_attaches_) {
      for (const auto &session : current.sessions()) {
        if (session.pid() == entry.first &&
            !entry.second.previous_ids.count(session.id())) {
          entry.second.candidate_ids.insert(session.id());
          recovery << " candidate_session=" << session.id()
                   << " state=" << api::Session::State_Name(session.state());
        }
      }
    }
  } catch (const std::exception &error) {
    recovery << " reconciliation unavailable: " << error.what();
  }
  recovery << "; inspect %sessions and explicitly %detach SESSION if "
              "appropriate; candidate ownership is not assumed";
  return recovery.str();
}

void Controller::ensure_connected() {
  if (stub_) {
    return;
  }
  if (!settings_.endpoint.empty()) {
    connect(settings_.endpoint);
  } else {
    start_server(Command{});
  }
}

std::string Controller::prompt() const {
  return selected_.id.empty()
             ? "tracer> "
             : "tracer[" + std::to_string(selected_.pid) + "]> ";
}

std::uint64_t Controller::require_session(const SessionHandle &session, bool scalar_overrides) {
  if (!stub_ || session.id.empty() || session.connection != connection_) {
    throw std::runtime_error(
        "no valid selected session; use %attach PID or %use SESSION");
  }
  if (scalar_overrides && endpoint_.rfind("unix:", 0) != 0)
    throw std::runtime_error("scalar overrides require a local Unix control socket");
  api::ListSessionsRequest request;
  auto response = unary<api::ListSessionsResponse>(
      [&](auto &c, auto &r) { return stub_->ListSessions(&c, request, &r); });
  for (const auto &entry : response.sessions()) {
    if (entry.id() == session.id && entry.state() == api::Session::ATTACHED) {
      if (scalar_overrides && !entry.jit_runtime().scalar_overrides_supported())
        throw std::runtime_error("the server and agent do not support scalar overrides");
      return entry.process_start_time();
    }
  }
  throw std::runtime_error("session is no longer attached: " + session.id);
}

void Controller::require_idle_target(const SessionHandle &session) const {
  const auto owned = owned_targets_.find(session.pid);
  if (owned != owned_targets_.end() && owned->second.session.id == session.id &&
      owned->second.process->call_pending())
    throw std::runtime_error("target call pending; use %call-result or %terminate");
}

SessionHandle Controller::attach(std::uint32_t pid,
                                 std::uint64_t expected_start_time,
                                 const std::string &expected_executable) {
  if (!pid || pid > INT32_MAX) {
    throw std::invalid_argument("PID must be positive and fit a native PID");
  }
  ensure_connected();
  if (pending_attaches_.count(pid)) {
    throw std::runtime_error(
        "previous attach outcome is uncertain; no automatic retry: " +
        reconcile_attaches());
  }
  api::ListSessionsRequest list;
  const auto before = unary<api::ListSessionsResponse>(
      [&](auto &c, auto &r) { return stub_->ListSessions(&c, list, &r); });
  for (const auto &s : before.sessions()) {
    if (s.pid() == pid && s.state() == api::Session::ATTACHED) {
      if ((expected_start_time && s.process_start_time() != expected_start_time) ||
          (!expected_executable.empty() && s.executable() != expected_executable))
        throw std::runtime_error("target identity changed; refresh process selection");
      selected_ = {s.id(), s.pid(), connection_};
      out_ << "selected existing session=" << s.id() << " pid=" << pid << '\n';
      return selected_;
    }
  }
  api::AttachRequest request;
  request.set_pid(pid);
  request.set_expected_process_start_time(expected_start_time);
  request.set_expected_executable(expected_executable);
  out_ << "attaching pid=" << pid << " endpoint=" << endpoint_ << " ...\n"
       << std::flush;
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(settings_.timeout_seconds));
  api::Session response;
  const auto status = stub_->Attach(&context, request, &response);
  if (!status.ok()) {
    PendingAttach pending;
    for (const auto &entry : before.sessions())
      pending.previous_ids.insert(entry.id());
    pending_attaches_.emplace(pid, std::move(pending));
    bool snapshot_available = false;
    auto recovery = reconcile_attaches(&snapshot_available);
    const auto code = status.error_code();
    const bool may_finish_later = code == grpc::StatusCode::DEADLINE_EXCEEDED ||
                                  code == grpc::StatusCode::CANCELLED ||
                                  code == grpc::StatusCode::UNAVAILABLE ||
                                  code == grpc::StatusCode::UNKNOWN ||
                                  code == grpc::StatusCode::INTERNAL;
    if (snapshot_available && pending_attaches_.at(pid).candidate_ids.empty() &&
        !may_finish_later) {
      pending_attaches_.erase(pid);
      recovery.clear();
    }
    throw std::runtime_error(
        "Attach RPC " + std::to_string(code) + ": " + status.error_message() +
        (recovery.empty() ? "" : "\nattach outcome uncertain; " + recovery));
  }
  if (!response.id().empty()) {
    owned_sessions_.insert(response.id());
  }
  if (response.state() != api::Session::ATTACHED) {
    throw std::runtime_error("attach did not complete; session=" +
                             response.id() + " " + response.error());
  }
  if ((expected_start_time && response.process_start_time() != expected_start_time) ||
      (!expected_executable.empty() && response.executable() != expected_executable)) {
    detach({response.id(), response.pid(), connection_});
    throw std::runtime_error("server did not confirm the selected target identity");
  }
  selected_ = {response.id(), response.pid(), connection_};
  out_ << "attached session=" << selected_.id << " pid=" << selected_.pid
       << '\n';
  if (response.has_jit_runtime()) {
    out_ << "target JIT runtime " << response.jit_runtime().ShortDebugString()
         << '\n';
  }
  return selected_;
}

void Controller::prune_owned_targets() {
  for (auto it = owned_targets_.begin(); it != owned_targets_.end();) {
    if (it->second.process->running())
      ++it;
    else
      it = owned_targets_.erase(it);
  }
}

void Controller::require_no_owned_targets() {
  prune_owned_targets();
  if (!owned_targets_.empty())
    throw std::runtime_error("terminate owned targets before changing server");
}

void Controller::launch(const Command &command) {
  prune_owned_targets();
  if (owned_targets_.size() >= 16)
    throw std::runtime_error("owned target limit reached");
  ensure_connected();
  if (!server_.running() || endpoint_ != server_.endpoint())
    throw std::runtime_error("%launch requires a private local server owned by this REPL");

  LaunchOptions options;
  options.executable = command.arguments.at(0);
  options.cwd = command.option("--cwd");
  options.input = command.option("--stdin");
  options.output = command.option("--stdout");
  options.error = command.option("--stderr");
  options.preload = command.option("--preload");
  options.supervise = command.has("--supervise");
  if (command.has("--arg"))
    options.arguments = command.options.at("--arg");
  if (command.has("--env"))
    options.environment = command.options.at("--env");

  auto target = OwnedTarget::start(options, server_.pid());
  const auto pid = target->pid();
  auto &owned = owned_targets_.emplace(pid, OwnedLaunch{std::move(target), {}}).first->second;
  auto &process = *owned.process;
  try {
    out_ << "launching pid=" << pid
         << " stdout=" << std::quoted(printable_proc_text(process.output_path()))
         << " stderr=" << std::quoted(printable_proc_text(process.error_path()))
         << '\n' << std::flush;
    process.wait_ready(command.number("--timeout", settings_.timeout_seconds), interrupted);
    const auto start = tracer::proc::start_time(tracer::proc::directory(pid) / "stat");
    if (!start)
      throw std::runtime_error("cannot verify owned target identity");
    owned.session = attach(pid, start, process.executable());
    if (interrupted.load())
      throw std::runtime_error("target launch cancelled");
  } catch (const std::exception &error) {
    try {
      process.terminate();
    } catch (const std::exception &cleanup) {
      throw std::runtime_error(std::string(error.what()) + "; " + cleanup.what() +
                               "; retry with %terminate " + std::to_string(pid));
    }
    const auto session = std::move(owned.session);
    owned_targets_.erase(pid);
    pending_attaches_.erase(pid);
    if (!session.id.empty()) {
      try {
        detach(session);
      } catch (const std::exception &cleanup) {
        throw std::runtime_error(std::string(error.what()) +
                                 "; target reaped, session cleanup failed: " + cleanup.what());
      }
    }
    throw;
  }
  out_ << "launched pid=" << pid << " session=" << owned.session.id
       << " paused=true\n";
}

void Controller::control_target(const Command &command) {
  const auto pid = command.arguments.empty()
                       ? selected_.pid
                       : static_cast<std::uint32_t>(unsigned_number(command.arguments[0], "PID", INT32_MAX));
  const auto found = owned_targets_.find(pid);
  if (found == owned_targets_.end())
    throw std::runtime_error("specify a PID launched by this REPL or select its session");
  auto &owned = found->second;
  if (command.kind == CommandKind::resume) {
    require_idle_target(owned.session);
    require_session(owned.session);
    owned.process->resume();
    out_ << "resumed pid=" << pid << '\n';
    return;
  }
  owned.process->terminate();
  const auto session = std::move(owned.session);
  owned_targets_.erase(found);
  pending_attaches_.erase(pid);
  out_ << "terminated pid=" << pid << '\n';
  if (owned_sessions_.count(session.id))
    detach(session);
}

void Controller::call_target(const Command &command) {
  const auto found = owned_targets_.find(selected_.pid);
  if (found == owned_targets_.end() || found->second.session.id != selected_.id ||
      found->second.session.connection != connection_)
    throw std::runtime_error("select a target launched by this REPL");
  auto &target = *found->second.process;
  if (command.kind == CommandKind::target_call) {
    require_idle_target(selected_);
    require_session(selected_);
    const auto args = command.options.find("--arg");
    const std::vector<std::string> no_args;
    if (interrupted.load())
      throw std::runtime_error("target call cancelled before submission");
    target.start_call(command.arguments.at(0), args == command.options.end() ? no_args : args->second);
  }
  const auto result = target.call_result(command.number("--timeout", settings_.timeout_seconds), interrupted);
  out_ << "call result pid=" << found->first << ' ' << result << '\n';
}

void Controller::detach(const SessionHandle &session) {
  if (!stub_ || session.id.empty() || session.connection != connection_) {
    throw std::runtime_error("invalid session handle");
  }
  for (const auto &entry : owned_targets_) {
    auto &target = entry.second;
    if (target.session.id == session.id && target.process->paused() &&
        target.process->running())
      throw std::runtime_error("resume or terminate the paused target before detaching");
  }
  stop_background(session.id);
  api::DetachRequest request;
  request.set_session_id(session.id);
  auto response = unary<api::Session>(
      [&](auto &c, auto &r) { return stub_->Detach(&c, request, &r); },
      settings_.timeout_seconds);
  if (response.state() != api::Session::DETACHED &&
      response.state() != api::Session::EXITED) {
    throw std::runtime_error("detach restoration incomplete: " + session.id +
                             " " + response.error());
  }
  owned_sessions_.erase(session.id);
  for (auto it = pending_attaches_.begin(); it != pending_attaches_.end();) {
    const bool identified = it->second.candidate_ids.erase(session.id) != 0 ||
                            (response.pid() == it->first &&
                             !it->second.previous_ids.count(session.id));
    if (identified)
      it->second.previous_ids.insert(session.id);
    if (identified && it->second.candidate_ids.empty())
      it = pending_attaches_.erase(it);
    else
      ++it;
  }
  if (selected_.id == session.id) {
    selected_ = {};
  }
  out_ << "detached session=" << request.session_id() << '\n';
}

api::ProbeSpec Controller::resolve(const std::string &selector,
                                   const Command &command) {
  require_session(selected_);
  return resolve_for(selected_, selector, command);
}

api::ProbeSpec Controller::resolve_for(const SessionHandle &session,
                                       const std::string &selector,
                                       const Command &command) {
  api::ResolveProbeRequest request;
  request.set_session_id(session.id);
  request.set_selector(selector);
  resolution_options(request, command);
  const auto prepared = prepared_options(request, command, prepared_packages_, out_);
  const auto resolve = [&] {
    return unary<api::ProbeSpec>(
        [&](auto &c, auto &r) { return stub_->ResolveProbe(&c, request, &r); },
        settings_.timeout_seconds);
  };
  try {
    auto spec = resolve();
    if (prepared != PreparedMode::off &&
        (!spec.has_prepared() || spec.prepared().SerializeAsString() !=
                                    request.prepared().SerializeAsString()))
      throw std::runtime_error("server does not support the prepared contract");
    return spec;
  } catch (const std::exception &error) {
    if (prepared != PreparedMode::prefer) throw;
    out_ << "prepared contract rejected; falling back to DWARF: " << error.what() << '\n';
    request.clear_prepared();
    request.set_selector(selector);
    return resolve();
  }
}

void Controller::prepare(const std::string &selector, const Command &options) {
  const auto database = options.option("--compdb", "compile_commands.json");
  if (!options.has("--compdb") && !std::filesystem::is_regular_file(database))
    throw std::invalid_argument(
        "compile_commands.json was not found in the current directory. Pass --compdb FILE.");
  const char *configured = std::getenv("TRACER_PREPARE_EXECUTABLE");
  std::string executable = "clang-tracer-prepare-injectors-native";
  if (configured && *configured) {
    executable = configured;
  }
#ifdef __linux__
  else {
    std::error_code error;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
      const auto sibling = self.parent_path() / executable;
      if (std::filesystem::is_regular_file(sibling, error)) executable = sibling.string();
    }
  }
#endif
  std::vector<std::string> arguments{executable, "--selector", selector, "--compdb", database};
  if (options.has("--metadata-only")) arguments.push_back("--metadata-only");
  for (const auto *key : {"--translation-unit", "--module"}) {
    if (options.has(key)) {
      arguments.emplace_back(key);
      arguments.push_back(options.option(key));
    }
  }
  for (const auto *key : {"--include", "--output"}) {
    const auto found = options.options.find(key);
    if (found == options.options.end())
      throw std::invalid_argument(std::string("preparation requires ") + key);
    for (const auto &value : found->second) {
      arguments.emplace_back(key);
      arguments.push_back(value);
    }
  }
  std::vector<char *> argv;
  for (auto &argument : arguments) argv.push_back(argument.data());
  argv.push_back(nullptr);
  pid_t pid = 0;
  const int started = ::posix_spawnp(&pid, argv.front(), nullptr, nullptr, argv.data(), environ);
  if (started)
    throw std::system_error(started, std::generic_category(), "cannot start native preparer");
  int status = 0;
  while (::waitpid(pid, &status, 0) < 0)
    if (errno != EINTR) throw std::runtime_error("cannot wait for native preparation");
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    throw std::runtime_error("native header preparation failed");
  const auto directory = std::filesystem::absolute(options.option("--output"));
  PreparedPackage package{directory.string(), {}};
  const auto file = directory / "prepared.pb";
  std::ifstream input(file, std::ios::binary);
  if (!input || std::filesystem::file_size(file) > 65536 ||
      !package.artifact.ParseFromIstream(&input))
    throw std::runtime_error("cannot read newly generated prepared package");
  prepared_packages_.insert_or_assign(compact_selector(selector), std::move(package));
  out_ << "prepared directory=" << directory << '\n';
}

std::vector<std::string> Controller::take_pending_imports() {
  return std::exchange(pending_imports_, {});
}

std::vector<api::PreparedInjector> Controller::take_pending_replay_types() {
  return std::exchange(pending_replay_types_, {});
}

std::uint64_t Controller::inject(const SessionHandle &session,
                                 const std::string &selector,
                                 const std::string &source,
                                 const Command &options) {
  require_idle_target(session);
  const bool override_arguments = options.has("--allow-arg-overrides");
  const bool override_result = options.has("--allow-result-override");
  const bool replacement = options.has("--allow-replacement");
  require_session(session, override_arguments || override_result || replacement);
  const auto resolve_started = std::chrono::steady_clock::now();
  auto spec = resolve_for(session, selector, options);
  const auto resolve_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - resolve_started).count();
  spec.set_callback_source(source);
  spec.set_stack_depth(
      static_cast<unsigned>(options.number("--stack-depth", 0)));
  spec.set_print_arguments(options.has("--print-arguments"));
  spec.set_print_result(options.has("--print-result"));
  spec.set_allow_argument_overrides(override_arguments);
  spec.set_allow_result_override(override_result);
  spec.set_allow_replacement(replacement);
  spec.set_reclaimable(options.has("--reclaimable"));
  api::AddProbeRequest request;
  request.set_session_id(session.id);
  *request.mutable_spec() = std::move(spec);
  const auto result = unary<api::Probe>(
      [&](auto &c, auto &r) { return stub_->AddProbe(&c, request, &r); },
      settings_.timeout_seconds);
  if (!result.active()) {
    throw std::runtime_error("probe installation did not become active");
  }
  // Only import declarations generated locally in this REPL and returned
  // intact by resolution.
  const auto &artifact = request.spec().prepared();
  const auto package = prepared_packages_.find(compact_selector(selector));
  if (artifact.format_version() == tracer::prepared::typed_version &&
      package != prepared_packages_.end() &&
      artifact.SerializeAsString() == package->second.artifact.SerializeAsString())
    pending_imports_.push_back(tracer::prepared::adapter_source(package->second.artifact));
  out_ << "installed probe=" << result.id()
       << " session=" << result.session_id();
  if (result.jit_compile_ns() && result.patch_install_ns())
    out_ << " resolve_ns=" << resolve_ns
         << " jit_compile_ns=" << result.jit_compile_ns()
         << " patch_install_ns=" << result.patch_install_ns();
  out_ << '\n';
  return result.id();
}

void Controller::draft(const std::string &name, std::string source) {
  if (source.empty() || source.size() > 4 * 1024 * 1024) {
    throw std::invalid_argument("draft must contain between 1 byte and 4 MiB");
  }
  drafts_[name] = std::move(source);
}

const EventBuffer &Controller::buffer(const std::string &name) const {
  const auto it = batches_.find(name);
  if (it == batches_.end()) {
    throw std::runtime_error("unknown event collection: " + name);
  }
  return it->second;
}

Collection Controller::batch(const std::string &name) const {
  return buffer(name).snapshot();
}

void Controller::print(const Event &event) const {
  out_ << event.describe() << '\n' << std::flush;
}

void Controller::print(const Collection &collection) const {
  out_ << "Collection retained=" << collection.size()
       << " received=" << collection.received
       << " evicted=" << collection.evicted
       << " complete=" << (collection.complete ? "true" : "false")
       << " cancelled=" << (collection.cancelled ? "true" : "false")
       << " timed_out=" << (collection.timed_out ? "true" : "false") << '\n';
  for (const auto &event : collection)
    print(event);
  out_ << std::flush;
}

RecordResult Controller::watch(const Command &command, bool display,
                               const std::string &record_path) {
  ensure_connected();
  api::WatchEventsRequest request;
  if (command.has("--session")) {
    request.add_session_ids(command.option("--session"));
  } else if (!command.has("--all")) {
    require_session(selected_);
    request.add_session_ids(selected_.id);
  }
  request.set_after_sequence(command.number("--after", 0));
  const bool record_sqlite = command.has("--sqlite");
  request.set_include_replay_types(record_sqlite);
  auto names = command.options.find("--name");
  if (names != command.options.end()) {
    for (const auto &name : names->second) {
      request.add_names(name);
    }
  }
  const auto collection_name =
      command.option("--as", display ? "live" : "batch");
  const auto reserved = background_storage();
  std::size_t used_events = reserved.first, used_bytes = reserved.second;
  if (record_path.empty()) {
    for (const auto &entry : batches_) {
      if (entry.first == collection_name) {
        continue;
      }
      used_bytes += entry.second.bytes();
      used_events += entry.second.events().size();
    }
  }
  if (used_bytes >= settings_.max_bytes ||
      used_events >= settings_.max_events) {
    throw std::runtime_error(
        "collection storage is full; clear collections or release event ranges");
  }
  EventBuffer next(settings_.max_events - used_events,
                   settings_.max_bytes - used_bytes);

  struct OutputFile {
    int fd = -1;

    ~OutputFile() {
      if (fd >= 0) {
        ::close(fd);
      }
    }
  } output;

  std::unique_ptr<tracer::client::SQLiteEvents> sqlite;
  if (record_sqlite) {
    sqlite = std::make_unique<tracer::client::SQLiteEvents>(record_path);
  } else if (!record_path.empty()) {
    output.fd = create_recording(record_path);
  }
  grpc::ClientContext context;
  const auto count = command.number("--count", 0);
  const auto timeout =
      command.number("--timeout", count ? settings_.timeout_seconds : 0);
  if (timeout) {
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(timeout));
  }
  if (record_path.empty())
    batches_.erase(collection_name);
  auto reader = stub_->WatchEvents(&context, request);
  Cancellation cancellation(context);
  std::map<std::uint64_t, api::PreparedInjector> pending_types;
  api::TraceEvent event;
  std::uint64_t accepted = 0, cursor = request.after_sequence();
  while ((!count || accepted < count) && reader->Read(&event)) {
    cursor = event.sequence();
    if (command.has("--probe") &&
        event.probe_id() != command.number("--probe", 0)) {
      continue;
    }
    if (command.has("--phase") &&
        api::TracePhase_Name(event.phase()) != command.option("--phase")) {
      // The server can send the adapter once for a batch. Keep it if the
      // event carrying it is filtered out before the next stored event.
      if (sqlite && event.has_replay_types())
        pending_types.insert_or_assign(event.probe_id(),
                                       std::move(*event.mutable_replay_types()));
      continue;
    }
    if (sqlite) {
      auto types = pending_types.extract(event.probe_id());
      if (!types.empty() && !event.has_replay_types())
        *event.mutable_replay_types() = std::move(types.mapped());
      sqlite->write(std::array{event});
    } else if (output.fd >= 0) {
      write_record(output.fd, event);
    } else {
      next.push(event);
    }
    if (display) {
      out_ << event_summary(event) << '\n' << std::flush;
    }
    ++accepted;
  }
  const bool complete = count && accepted == count;
  if (complete) {
    context.TryCancel();
  }
  const auto status = reader->Finish();
  if (output.fd >= 0) {
    const int fd = output.fd;
    output.fd = -1;
    if (::close(fd) != 0) {
      throw std::runtime_error("recording close failed; partial file retained");
    }
  }
  const RecordResult result{accepted, cursor, complete, interrupted != 0,
                            status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED};
  out_ << "received=" << result.received << " cursor=" << result.cursor
       << " complete=" << (result.complete ? "true" : "false")
       << " cancelled=" << (result.cancelled ? "true" : "false")
       << " timed_out=" << (result.timed_out ? "true" : "false");
  if (record_path.empty()) {
    next.complete = result.complete;
    next.cancelled = result.cancelled;
    next.timed_out = result.timed_out;
    out_ << " collection=" << collection_name
         << " retained=" << next.events().size() << " evicted=" << next.evicted;
    batches_.insert_or_assign(collection_name, std::move(next));
  } else {
    out_ << " recording=" << record_path;
  }
  out_ << '\n';
  if (!status.ok() && !complete && !interrupted &&
      status.error_code() != grpc::StatusCode::DEADLINE_EXCEEDED) {
    throw std::runtime_error("WatchEvents " +
                             std::to_string(status.error_code()) + ": " +
                             status.error_message());
  }
  if (status.ok() && !complete && !interrupted) {
    throw std::runtime_error(
        "event stream ended early; partial results retained");
  }
  return result;
}

Collection Controller::collect(const SessionHandle &session,
                               std::uint64_t count, unsigned timeout) {
  require_session(session);
  if (!count || !timeout || timeout > 86400) {
    throw std::invalid_argument(
        "collection count and timeout must be positive");
  }
  Command command;
  command.options = {{"--session", {session.id}},
                     {"--count", {std::to_string(count)}},
                     {"--timeout", {std::to_string(timeout)}},
                     {"--as", {"cpp"}}};
  watch(command, false);
  return batch("cpp");
}

RecordResult Controller::record(const std::string &path,
                                const RecordOptions &options) {
  if (path.empty() || path == ":memory:" || path.find('\0') != std::string::npos)
    throw std::invalid_argument("SQLite recording requires a persistent file path");
  if (options.session && options.all_sessions)
    throw std::invalid_argument("recording session conflicts with all_sessions");
  if (!options.phase.empty() && options.phase != "PRE" && options.phase != "POST")
    throw std::invalid_argument("recording phase must be PRE or POST");
  if (options.timeout_seconds > 86400)
    throw std::invalid_argument("recording timeout must be at most 86400 seconds");

  Command command;
  command.options["--sqlite"] = {"true"};
  if (options.session) {
    require_session(*options.session);
    command.options["--session"] = {options.session->id};
  }
  if (options.all_sessions)
    command.options["--all"] = {"true"};
  if (!options.names.empty())
    command.options["--name"] = options.names;
  if (!options.phase.empty())
    command.options["--phase"] = {options.phase};
  if (options.probe_id)
    command.options["--probe"] = {std::to_string(options.probe_id)};
  if (options.after_sequence)
    command.options["--after"] = {std::to_string(options.after_sequence)};
  if (options.count)
    command.options["--count"] = {std::to_string(options.count)};
  if (options.timeout_seconds)
    command.options["--timeout"] = {std::to_string(options.timeout_seconds)};
  return watch(command, false, path);
}

std::pair<std::size_t, std::size_t> Controller::background_storage() const {
  std::pair<std::size_t, std::size_t> used{};
  for (const auto& weak : background_) {
    if (auto state = weak.lock()) {
      used.first += state->max_events;
      used.second += state->max_bytes;
    }
  }
  return used;
}

EventBuffer Controller::read_recording(const std::string &path,
                                       const EventQuery &query) const {
  auto [events, bytes] = background_storage();
  for (const auto &entry : batches_) {
    bytes += entry.second.bytes();
    events += entry.second.events().size();
  }
  if (bytes >= settings_.max_bytes || events >= settings_.max_events) {
    throw std::runtime_error(
        "clear collections or release event ranges before loading a recording");
  }
  auto recording = open_recording(path, settings_.max_events - events,
                                 settings_.max_bytes - bytes, query);
  for (auto &types : recording.replay_types)
    pending_replay_types_.push_back(std::move(types));
  recording.replay_types.clear();
  return recording;
}

Collection Controller::replay(const std::string &path,
                              const EventQuery &query) const {
  return read_recording(path, query).snapshot();
}

void Controller::stop_background(const std::string& session_id) {
  for (auto it = background_.begin(); it != background_.end();) {
    if (auto state = it->lock()) {
      if (session_id.empty() || state->session.id == session_id) {
        state->stop();
      }
      ++it;
    } else {
      it = background_.erase(it);
    }
  }
}

EventRange Controller::events(const SessionHandle& session, std::uint64_t count, unsigned timeout) {
  if (!count || !timeout || timeout > 86400) {
    throw std::invalid_argument(
        "collection count and timeout must be positive; timeout at most 86400 seconds");
  }
  auto range = events_async(session, count, timeout);
  while (range.running() && !interrupted) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  range.stop();
  return range;
}

EventRange Controller::events_async(const SessionHandle& session, std::uint64_t count,
                                    unsigned timeout) {
  require_session(session);
  if (timeout > 86400) {
    throw std::invalid_argument("event timeout must be at most 86400 seconds");
  }
  auto used = background_storage();
  for (const auto& entry : batches_) {
    used.first += entry.second.events().size();
    used.second += entry.second.bytes();
  }
  if (used.first >= settings_.max_events || used.second >= settings_.max_bytes) {
    throw std::runtime_error(
        "collection storage is full; clear collections or release event ranges");
  }
  auto state = std::make_shared<BackgroundEvents>(endpoint_, session, count, timeout,
                                                  settings_.max_events - used.first,
                                                  settings_.max_bytes - used.second);
  background_.erase(std::remove_if(background_.begin(), background_.end(),
                                   [](const auto& weak) { return weak.expired(); }),
                    background_.end());
  background_.push_back(state);
  return EventRange(std::move(state));
}

std::vector<Controller::ProcessChoice>
Controller::processes(const std::string &filter) const {
#ifdef __linux__
  if (server_.pid() < 0) {
    throw std::runtime_error("process listing is local-owned-server only; "
                             "remote attach requires an explicit PID");
  }
  std::vector<ProcessChoice> result;
  for (const auto &entry : std::filesystem::directory_iterator("/proc")) {
    const auto name = entry.path().filename().string();
    if (name.empty() ||
        name.find_first_not_of("0123456789") != std::string::npos) {
      continue;
    }
    const auto pid =
        static_cast<unsigned>(unsigned_number(name, "PID", INT32_MAX));
    if (pid == static_cast<unsigned>(::getpid()) ||
        pid == static_cast<unsigned>(server_.pid())) {
      continue;
    }
    try {
      const auto start = tracer::proc::start_time(entry.path() / "stat");
      std::error_code error;
      auto executable =
          std::filesystem::read_symlink(entry.path() / "exe", error).string();
      if (error || executable.find(filter) == std::string::npos)
        continue;
      std::ifstream input(entry.path() / "cmdline", std::ios::binary);
      std::string command(4096, '\0');
      input.read(command.data(), command.size());
      command.resize(input.gcount());
      std::replace(command.begin(), command.end(), '\0', ' ');
      if (tracer::proc::start_time(entry.path() / "stat") == start)
        result.push_back({pid, start, std::move(executable), std::move(command)});
    } catch (const std::runtime_error &) {
      // A process may exit or become inaccessible while reading /proc.
    }
  }
  std::sort(result.begin(), result.end(),
            [](const auto &left, const auto &right) { return left.pid < right.pid; });
  return result;
#else
  (void)filter;
  throw std::runtime_error("process listing needs a local Linux server; use "
                           "%attach PID for remote tracing");
#endif
}

void Controller::select_process(
    const std::string &filter,
    const std::function<std::string(const std::string &)> &read) {
  ensure_connected();
  const auto choices = processes(filter);
  if (choices.empty()) {
    throw std::runtime_error("no matching processes");
  }
  auto selected = choices.begin();
  if (choices.size() > 1) {
    for (const auto &choice : choices)
      out_ << choice.pid << " start=" << choice.start_time << " exe="
           << std::quoted(printable_proc_text(choice.executable)) << " "
           << printable_proc_text(choice.command) << '\n';
    const auto choice = read("Attach PID (empty cancels)> ");
    if (choice.empty())
      return;
    const auto pid = unsigned_number(choice, "PID", INT32_MAX);
    selected = std::find_if(choices.begin(), choices.end(),
                            [&](const auto &p) { return p.pid == pid; });
    if (selected == choices.end())
      throw std::invalid_argument("PID was not in the displayed process list");
  }
  const auto proc = tracer::proc::directory(selected->pid);
  if (tracer::proc::start_time(proc / "stat") != selected->start_time ||
      std::filesystem::read_symlink(proc / "exe") != selected->executable)
    throw std::runtime_error("target identity changed; refresh process selection");
  attach(selected->pid, selected->start_time, selected->executable);
}

void Controller::preview_bulk_trace(const Command &command) {
  require_session(selected_);
  api::PreviewBulkTraceRequest request;
  request.set_session_id(selected_.id);
  auto &rules = *request.mutable_rules();
  if (command.has("--manifest")) {
    rules = read_bulk_manifest(command.option("--manifest"));
  } else {
    rules.set_version(1);
    const auto copy = [&](const char *option, auto *field) {
      const auto values = command.options.find(option);
      if (values != command.options.end())
        for (const auto &value : values->second)
          *field->Add() = value;
    };
    copy("--module", rules.mutable_module_globs());
    copy("--include", rules.mutable_include_globs());
    copy("--exclude", rules.mutable_exclude_globs());
    copy("--exact", rules.mutable_exact_selectors());
  }
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(settings_.timeout_seconds));
  Cancellation cancellation(context);
  api::PreviewBulkTraceResponse response;
  const auto status = stub_->PreviewBulkTrace(&context, request, &response);
  if (!status.ok())
    throw std::runtime_error("bulk trace preview: " + status.error_message());

  unsigned supported = 0;
  for (const auto &candidate : response.candidates()) {
    const auto &function = candidate.function();
    out_ << api::BulkTraceCandidate::AbiStatus_Name(candidate.abi_status()) << " ";
    if (function.runtime_address())
      out_ << "address=0x" << std::hex << function.runtime_address() << std::dec;
    else
      out_ << "address=unavailable";
    out_ << " " << std::quoted(printable_proc_text(function.selector()))
         << " module=" << std::quoted(printable_proc_text(function.module_path()));
    if (!candidate.diagnostic().empty())
      out_ << " reason=" << std::quoted(printable_proc_text(candidate.diagnostic()));
    out_ << '\n';
    if (candidate.abi_status() == api::BulkTraceCandidate::DWARF_SUPPORTED)
      ++supported;
  }
  for (const auto &selector : response.unmatched_exact_selectors())
    out_ << "UNMATCHED " << std::quoted(printable_proc_text(selector))
         << " reason=\"no matching concrete entry after module and exclusion rules\"\n";
  out_ << "dry-run matched=" << response.candidates_size()
       << " dwarf_supported=" << supported
       << " limit=" << response.retained_limit()
       << " session_retained=" << response.session_retained();
  if (response.has_available_slots())
    out_ << " agent_retained=" << response.agent_retained()
         << " available=" << response.available_slots()
         << " fits=" << (supported <= response.available_slots() ? "yes" : "no");
  else
    out_ << " available=unknown fits=unknown";
  out_ << '\n';
  if (command.has("--save")) {
    const auto path = command.option("--save");
    save_bulk_manifest(path, rules);
    out_ << "manifest=" << std::quoted(printable_proc_text(path)) << '\n';
  }
}

void Controller::apply_bulk_trace(const Command &command) {
  require_idle_target(selected_);
  require_session(selected_);
  api::ApplyBulkTraceRequest request;
  request.set_session_id(selected_.id);
  *request.mutable_rules() = read_bulk_manifest(command.option("--manifest"));
  if (command.has("--callback"))
    request.set_callback_source(read_source(command.option("--callback")));
  request.set_batch_id(new_bulk_batch_id());
  out_ << "bulk apply starting batch=" << request.batch_id() << std::endl;

  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(settings_.timeout_seconds));
  Cancellation cancellation(context);
  api::BulkTraceReport report;
  const auto status = stub_->ApplyBulkTrace(&context, request, &report);
  if (!status.ok()) {
    auto message = "bulk trace apply: " + status.error_message();
    if (status.error_code() == grpc::StatusCode::CANCELLED ||
        status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ||
        status.error_code() == grpc::StatusCode::UNAVAILABLE ||
        status.error_code() == grpc::StatusCode::UNKNOWN ||
        status.error_code() == grpc::StatusCode::INTERNAL)
      message += " | check or restore with %trace remove " + request.batch_id();
    throw std::runtime_error(message);
  }
  print_bulk_report(out_, "apply", report);
  if (!report.complete()) {
    auto message = report.diagnostic();
    if (std::any_of(report.entries().begin(), report.entries().end(), [](const auto &row) {
          return row.status() == api::BulkTraceEntry::RESTORE_PENDING;
        }))
      message += " | retry with %trace remove " + request.batch_id();
    throw std::runtime_error(message);
  }
}

void Controller::remove_bulk_trace(const std::string &batch_id) {
  require_idle_target(selected_);
  require_session(selected_);
  api::RemoveBulkTraceRequest request;
  request.set_session_id(selected_.id);
  request.set_batch_id(batch_id);
  const auto report = unary<api::BulkTraceReport>(
      [&](auto &context, auto &response) { return stub_->RemoveBulkTrace(&context, request, &response); },
      settings_.timeout_seconds);
  print_bulk_report(out_, "remove", report);
  if (!report.complete())
    throw std::runtime_error(report.diagnostic() + " | retry with %trace remove " + batch_id);
}

bool Controller::execute(const Command &command) {
  const auto &args = command.arguments;
  using K = CommandKind;
  switch (command.kind) {
  case K::quit:
    return false;
  case K::server_start:
    start_server(command);
    return true;
  case K::server_connect:
    require_no_owned_targets();
    if (server_.running() || !owned_sessions_.empty() ||
        !pending_attaches_.empty()) {
      throw std::runtime_error(
          "detach sessions and stop the owned server before reconnecting");
    }
    connect(args.at(0));
    settings_.endpoint = args.at(0);
    out_ << "connected endpoint=" << endpoint_ << " ownership=external\n";
    return true;
  case K::server_stop:
    require_no_owned_targets();
    if (server_.pid() < 0) {
      throw std::runtime_error("this console does not own a server");
    }
    server_.stop(settings_.timeout_seconds);
    owned_sessions_.clear();
    pending_attaches_.clear();
    disconnect();
    out_ << "owned server stopped after restoration\n";
    return true;
  case K::server_status:
  case K::status: {
    prune_owned_targets();
    const bool owned = server_.running();
    out_ << "endpoint=" << (endpoint_.empty() ? settings_.endpoint : endpoint_)
         << " ownership="
         << (owned ? "this-console" : "external-or-unconnected")
         << " child_pid=" << server_.pid() << " session=" << selected_.id
         << '\n';
    if (stub_) {
      api::ListSessionsRequest request;
      unary<api::ListSessionsResponse>(
          [&](auto &c, auto &r) {
            return stub_->ListSessions(&c, request, &r);
          },
          1);
      out_ << "ready=true\n";
    } else {
      out_ << "ready=unconnected\n";
    }
    if (!pending_attaches_.empty())
      out_ << reconcile_attaches() << '\n';
    for (const auto &target : owned_targets_)
      out_ << "owned_target pid=" << target.first
           << " paused=" << (target.second.process->paused() ? "true" : "false")
           << " calls=" << (target.second.process->calls_enabled() ? "enabled" : "unavailable")
           << " call_pending=" << (target.second.process->call_pending() ? "true" : "false") << '\n';
    for (const auto &b : batches_) {
      out_ << "collection=" << b.first << " events=" << b.second.events().size()
           << " bytes=" << b.second.bytes() << '\n';
    }
    return true;
  }
  case K::attach:
    attach(static_cast<unsigned>(unsigned_number(args.at(0), "PID", INT32_MAX)),
           command.number("--start-time", 0));
    return true;
  case K::launch:
    launch(command);
    return true;
  case K::resume:
  case K::terminate:
    control_target(command);
    return true;
  case K::target_call:
  case K::target_call_result:
    call_target(command);
    return true;
  case K::syscalls:
  case K::watchpoint:
  case K::sample_cpu: {
    if (!server_.running() || endpoint_ != server_.endpoint())
      throw std::runtime_error("local perf capture requires the REPL's owned local server");
    const auto start = require_session(selected_);
    auto mode = command.kind == K::syscalls ? LocalPerfMode::syscalls : LocalPerfMode::cpu;
    Watchpoint watch{};
    if (command.kind == K::watchpoint) {
      mode = LocalPerfMode::watchpoint;
      watch = {static_cast<std::uint32_t>(command.number("--tid", 0)),
               address_number(command.option("--address")),
               static_cast<unsigned>(command.number("--size", 0)),
               command.option("--access") == "read-write"};
      api::ReadMemoryRequest request;
      request.set_session_id(selected_.id);
      request.set_mapping_token(command.option("--mapping"));
      request.set_address(watch.address);
      request.set_size(watch.size);
      const auto result = unary<api::ReadMemoryResponse>(
          [&](auto &c, auto &r) { return stub_->ReadMemory(&c, request, &r); },
          settings_.timeout_seconds);
      if (result.address() != watch.address || result.data().size() != watch.size)
        throw std::runtime_error("watchpoint range could not be verified");
    }
    capture_local_perf(mode, selected_.pid, start, command.number("--timeout", 2),
                       command.number("--count", 256), interrupted, out_,
                       mode == LocalPerfMode::watchpoint ? &watch : nullptr);
    return true;
  }
  case K::bulk_preview:
    preview_bulk_trace(command);
    return true;
  case K::bulk_apply:
    apply_bulk_trace(command);
    return true;
  case K::bulk_remove:
    remove_bulk_trace(args.at(0));
    return true;
  case K::sessions:
  case K::use:
  case K::probes: {
    ensure_connected();
    api::ListSessionsRequest request;
    const auto response = unary<api::ListSessionsResponse>(
        [&](auto &c, auto &r) { return stub_->ListSessions(&c, request, &r); });
    for (const auto &s : response.sessions()) {
      if (command.kind == K::use && s.id() == args.at(0)) {
        if (s.state() != api::Session::ATTACHED) {
          throw std::runtime_error("session is not attached");
        }
        selected_ = {s.id(), s.pid(), connection_};
        out_ << "selected session=" << s.id() << '\n';
        return true;
      }
      if (command.kind == K::sessions) {
        out_ << s.ShortDebugString() << '\n';
      }
      if (command.kind == K::probes && s.id() == selected_.id) {
        for (const auto &probe : s.probes()) {
          out_ << probe.ShortDebugString() << '\n';
        }
      }
    }
    if (command.kind == K::use) {
      throw std::runtime_error("unknown session: " + args.at(0));
    }
    return true;
  }
  case K::detach: {
    auto session = selected_;
    if (!args.empty()) {
      ensure_connected();
      session.id = args[0];
      session.connection = connection_;
    }
    detach(session);
    return true;
  }
  case K::modules: {
    require_session(selected_);
    api::ListModulesRequest request;
    request.set_session_id(selected_.id);
    request.set_include_mappings(command.has("--details") || command.has("--watch"));
    request.set_elf_path(command.option("--elf"));
    const auto modules = [&] { return unary<api::ListModulesResponse>(
        [&](auto &c, auto &r) { return stub_->ListModules(&c, request, &r); }); };
    const auto response = modules();
    if (command.has("--watch")) {
      const auto indexed = [](const api::ListModulesResponse &snapshot) {
        std::map<std::string, api::ModuleMapping> entries;
        for (const auto &mapping : snapshot.mappings())
          entries.emplace(mapping.SerializeAsString(), mapping);
        return entries;
      };
      if (response.mapping_token().empty())
        throw std::runtime_error("server does not support mapped module snapshots");
      auto before = indexed(response);
      const auto deadline = std::chrono::steady_clock::now() +
          std::chrono::seconds(command.number("--timeout", 5));
      const auto limit = command.number("--count", 16);
      std::uint64_t count = 0;
      out_ << "module watch ready\n" << std::flush;
      const auto report = [&](const auto &from, const auto &to, const char *kind) {
        for (const auto &[key, mapping] : from) {
          if (count == limit)
            break;
          if (to.count(key))
            continue;
          out_ << "module " << kind << " address=0x" << std::hex << mapping.begin_address()
               << std::dec << " " << std::quoted(printable_proc_text(mapping.path())) << '\n'
               << std::flush;
          ++count;
        }
      };
      while (!interrupted.load() && count < limit && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (interrupted.load() || std::chrono::steady_clock::now() >= deadline)
          break;
        auto after = indexed(modules());
        report(before, after, "unloaded");
        report(after, before, "loaded");
        before = std::move(after);
      }
      out_ << "module watch complete changes=" << count << '\n';
    } else if (command.has("--elf")) {
      if (response.elf_entries().empty())
        throw std::runtime_error("server did not return an ELF view");
      for (const auto &entry : response.elf_entries())
        out_ << "elf " << api::ModuleElfEntry::Kind_Name(entry.kind())
             << " address=0x" << std::hex << entry.elf_address() << std::dec
             << " size=" << entry.size() << ' '
             << std::quoted(printable_proc_text(entry.name())) << '\n';
    } else if (!command.has("--details")) {
      for (const auto &module : response.modules())
        out_ << module << '\n';
    } else {
      if (response.mapping_token().empty())
        throw std::runtime_error("server does not support mapped module details");
      out_ << "mapping=" << response.mapping_token() << '\n';
      for (const auto &entry : response.mappings()) {
        out_ << "0x" << std::hex << entry.begin_address() << "-0x"
             << entry.end_address() << " offset=0x" << entry.file_offset()
             << std::dec << " device=" << entry.device() << " inode="
             << entry.inode() << " build_id="
             << (entry.build_id().empty() ? "unavailable" : entry.build_id())
             << " " << std::quoted(printable_proc_text(entry.path())) << '\n';
      }
    }
    return true;
  }
  case K::memory_maps: {
    require_session(selected_);
    api::ListMemoryMappingsRequest request;
    request.set_session_id(selected_.id);
    const auto response = unary<api::ListMemoryMappingsResponse>(
        [&](auto &c, auto &r) { return stub_->ListMemoryMappings(&c, request, &r); },
        settings_.timeout_seconds);
    out_ << "memory mapping=" << response.mapping_token() << '\n';
    for (const auto &entry : response.mappings()) {
      out_ << "0x" << std::hex << entry.begin_address() << "-0x"
           << entry.end_address() << ' ' << entry.permissions()
           << " offset=0x" << entry.file_offset() << std::dec
           << " device=" << entry.device() << " inode=" << entry.inode()
           << ' ' << std::quoted(printable_proc_text(entry.path())) << '\n';
    }
    out_ << "memory mappings=" << response.mappings_size() << '\n';
    return true;
  }
  case K::instructions_start: {
    require_session(selected_);
    if (!server_.running())
      throw std::runtime_error("instruction tracing requires the REPL's owned local server");
    api::StartInstructionTraceRequest request;
    request.set_session_id(selected_.id);
    request.set_mapping_token(command.option("--mapping"));
    request.set_tid(command.number("--tid", 0));
    request.set_address(address_number(command.option("--address")));
    request.set_size(command.number("--size", 0));
    request.set_max_records(command.number("--count", 256));
    request.set_timeout_seconds(command.number("--timeout", 5));
    request.set_registers(command.has("--registers"));
    request.set_callback_library(command.option("--callback-library"));
    if (command.has("--memory-address")) {
      request.set_memory_address(address_number(command.option("--memory-address")));
      request.set_memory_size(command.number("--memory-size", 0));
    }
    const auto result = unary<api::InstructionTrace>(
        [&](auto &c, auto &r) { return stub_->StartInstructionTrace(&c, request, &r); },
        settings_.timeout_seconds);
    out_ << "instructions started tid=" << result.tid() << " address=0x"
         << std::hex << result.address() << std::dec << " size=" << result.size()
         << " count-limit=" << request.max_records()
         << " timeout=" << request.timeout_seconds() << '\n';
    return true;
  }
  case K::page_monitor_start:
  case K::page_monitor_stop: {
    if (!server_.running() || endpoint_ != server_.endpoint())
      throw std::runtime_error("page monitoring requires the REPL's owned local server");
    require_idle_target(selected_);
    require_session(selected_);
    if (command.kind == K::page_monitor_start) {
      api::StartPageMonitorRequest request;
      request.set_session_id(selected_.id);
      request.set_mapping_token(command.option("--mapping"));
      request.set_tid(unsigned_number(command.option("--tid"), "monitor TID", INT32_MAX));
      request.set_address(address_number(command.option("--address")));
      request.set_size(unsigned_number(command.option("--size"), "monitor size", UINT32_MAX));
      request.set_timeout_seconds(command.number("--timeout", 5));
      const auto result = unary<api::PageMonitorResult>(
          [&](auto &c, auto &r) { return stub_->StartPageMonitor(&c, request, &r); },
          settings_.timeout_seconds);
      out_ << "page monitor started tid=" << result.tid() << '\n' << std::flush;
    } else {
      api::StopPageMonitorRequest request;
      request.set_session_id(selected_.id);
      const auto result = unary<api::PageMonitorResult>(
          [&](auto &c, auto &r) { return stub_->StopPageMonitor(&c, request, &r); },
          settings_.timeout_seconds);
      for (const auto &access : result.accesses())
        out_ << "page " << (access.write() ? "write" : "read") << " address=0x"
             << std::hex << access.address() << " pc=0x" << access.pc() << std::dec << '\n';
      out_ << "page monitor stopped count=" << result.accesses_size()
           << " expired=" << (result.expired() ? "true" : "false") << '\n';
    }
    return true;
  }
  case K::instructions_stop: {
    require_session(selected_);
    if (!server_.running())
      throw std::runtime_error("instruction tracing requires the REPL's owned local server");
    api::StopInstructionTraceRequest request;
    request.set_session_id(selected_.id);
    const auto result = unary<api::InstructionTrace>(
        [&](auto &c, auto &r) { return stub_->StopInstructionTrace(&c, request, &r); },
        settings_.timeout_seconds);
    if (result.reason() == api::InstructionTrace::RUNNING ||
        result.pcs_size() > TRACER_INSTRUCTION_MAX_RECORDS ||
        (result.details_size() && result.details_size() != result.pcs_size()))
      throw std::runtime_error("invalid instruction capture response");
    constexpr const char *registers[] = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
    static_assert(std::size(registers) == TRACER_INSTRUCTION_REGISTER_COUNT);
    for (int i = 0; i < result.pcs_size(); ++i) {
      out_ << "instruction pc=0x" << std::hex << result.pcs(i);
      if (result.details_size()) {
        const auto &detail = result.details(i);
        if (detail.registers_size() && detail.registers_size() != TRACER_INSTRUCTION_REGISTER_COUNT)
          throw std::runtime_error("invalid instruction register snapshot");
        for (int j = 0; j < detail.registers_size(); ++j)
          out_ << ' ' << registers[j] << "=0x" << detail.registers(j);
        if (detail.has_callback_value())
          out_ << " callback=0x" << detail.callback_value();
        if (detail.memory_size()) {
          out_ << " memory=0x" << detail.memory_address() << std::dec
               << " size=" << detail.memory_size() << " access="
               << (detail.memory_access() == TRACER_INSTRUCTION_MEMORY_READ ? "read" :
                   detail.memory_access() == TRACER_INSTRUCTION_MEMORY_WRITE ? "write" : "read-write");
        }
      }
      out_ << std::dec << '\n';
    }
    out_ << "instructions stopped tid=" << result.tid() << " count=" << result.pcs_size()
         << " reason=" << api::InstructionTrace::Reason_Name(result.reason())
         << " truncated=" << (result.reason() == api::InstructionTrace::LIMIT ? "true" : "false")
         << '\n';
    return true;
  }
  case K::memory_read: {
    require_session(selected_);
    api::ReadMemoryRequest request;
    request.set_session_id(selected_.id);
    request.set_mapping_token(command.option("--mapping"));
    request.set_address(address_number(args.at(0)));
    const auto type = command.option("--type");
    const auto size = command.has("--type")
        ? memory_scalar_size(type)
        : unsigned_number(command.option("--count"), "memory read size", 65536);
    request.set_size(static_cast<std::uint32_t>(size));
    const auto response = unary<api::ReadMemoryResponse>(
        [&](auto &c, auto &r) { return stub_->ReadMemory(&c, request, &r); },
        settings_.timeout_seconds);
    if (response.address() != request.address() || response.data().size() != size)
      throw std::runtime_error("invalid memory read response");
    if (command.has("--type"))
      print_memory_scalar(out_, request.address(), response.data(), type);
    else
      print_memory_dump(out_, request.address(), response.data());
    return true;
  }
  case K::memory_patch:
  case K::memory_import_hook:
  case K::memory_restore: {
    if (!server_.running() || endpoint_ != server_.endpoint())
      throw std::runtime_error("memory mutation requires the REPL's owned local server");
    require_idle_target(selected_);
    require_session(selected_);
    api::MemoryPatch response;
    if (command.kind == K::memory_restore) {
      api::RestoreMemoryPatchRequest request;
      request.set_session_id(selected_.id);
      request.set_patch_id(unsigned_number(args.at(0), "patch ID"));
      response = unary<api::MemoryPatch>(
          [&](auto &c, auto &r) { return stub_->RestoreMemoryPatch(&c, request, &r); },
          settings_.timeout_seconds);
    } else {
      api::ApplyMemoryPatchRequest request;
      request.set_session_id(selected_.id);
      request.set_mapping_token(command.option("--mapping"));
      request.set_module(command.option("--module"));
      if (command.kind == K::memory_import_hook) {
        auto *hook = request.mutable_import_slot();
        hook->set_symbol(command.option("--symbol"));
        hook->set_abi(command.option("--abi"));
        hook->set_expected_target(address_number(command.option("--original")));
        hook->set_replacement_target(address_number(command.option("--address")));
      } else {
        const auto bytes = [&](const char *option) {
          auto [value, mask] = parse_memory_pattern(command.option(option));
          if (mask.find_first_not_of(static_cast<char>(0xff)) != std::string::npos)
            throw std::invalid_argument("memory patch bytes cannot use wildcards or masks");
          return value;
        };
        auto *patch = request.mutable_executable();
        patch->set_elf_address(address_number(command.option("--elf-address")));
        patch->set_expected(bytes("--expect"));
        patch->set_replacement(bytes("--replace"));
      }
      response = unary<api::MemoryPatch>(
          [&](auto &c, auto &r) { return stub_->ApplyMemoryPatch(&c, request, &r); },
          settings_.timeout_seconds);
    }
    out_ << "memory patch=" << response.id() << " address=0x" << std::hex << response.address()
         << std::dec << " size=" << response.size()
         << " restored=" << (response.restored() ? "true" : "false") << '\n';
    return true;
  }
  case K::memory_scan: {
    require_session(selected_);
    api::ScanMemoryRequest request;
    request.set_session_id(selected_.id);
    request.set_mapping_token(command.option("--mapping"));
    request.set_address(address_number(args.at(0)));
    request.set_size(static_cast<std::uint32_t>(
        unsigned_number(command.option("--count"), "memory scan size", 1048576)));
    auto [pattern, mask] = parse_memory_pattern(command.option("--pattern"));
    request.set_pattern(std::move(pattern));
    request.set_mask(std::move(mask));
    if (command.has("--limit")) {
      const auto limit = unsigned_number(command.option("--limit"), "memory scan match limit", 1024);
      if (!limit)
        throw std::invalid_argument("memory scan match limit must be positive");
      request.set_max_matches(static_cast<std::uint32_t>(limit));
    }
    const auto response = unary<api::ScanMemoryResponse>(
        [&](auto &c, auto &r) {
          Cancellation cancellation(c);
          return stub_->ScanMemory(&c, request, &r);
        },
        settings_.timeout_seconds);
    for (const auto address : response.addresses())
      out_ << "match=0x" << std::hex << address << std::dec << '\n';
    out_ << "memory scan matches=" << response.addresses_size()
         << " truncated=" << (response.truncated() ? "true" : "false") << '\n';
    return true;
  }
  case K::probe_entry: {
    require_session(selected_);
    api::PreviewFunctionEntryRequest request;
    request.set_session_id(selected_.id);
    request.set_mapping_token(command.option("--mapping"));
    request.set_module(command.option("--module"));
    if (command.has("--elf-address")) {
      request.set_elf_address(address_number(command.option("--elf-address")));
      request.set_abi(command.option("--abi"));
    } else {
      request.set_selector(args.at(0));
    }
    const auto response = unary<api::PreviewFunctionEntryResponse>(
        [&](auto &c, auto &r) {
          Cancellation cancellation(c);
          return stub_->PreviewFunctionEntry(&c, request, &r);
        },
        settings_.timeout_seconds);
    out_ << "entry=0x" << std::hex << response.runtime_address()
         << " elf=0x" << response.elf_address() << std::dec
         << " module=" << std::quoted(printable_proc_text(response.module()))
         << " boundary=" << api::PreviewFunctionEntryResponse::Boundary_Name(response.boundary())
         << " abi=" << response.abi() << " function_size=" << response.function_size()
         << " verified_bytes=" << response.entry_bytes().size();
    if (!response.selector().empty())
      out_ << " selector=" << std::quoted(printable_proc_text(response.selector()));
    out_ << '\n';
    for (const auto &instruction : response.instructions()) {
      std::ostringstream line;
      line << "0x" << std::hex << instruction.address() << "  ";
      for (const unsigned char byte : instruction.encoding())
        line << std::setfill('0') << std::setw(2) << static_cast<unsigned>(byte) << ' ';
      line << std::dec << ' ' << instruction.assembly();
      if (instruction.flow() != api::EntryInstruction::ORDINARY) {
        line << " flow=" << api::EntryInstruction::Flow_Name(instruction.flow());
        if (instruction.has_target_address())
          line << " target=0x" << std::hex << instruction.target_address() << std::dec;
        else
          line << " target=unresolved";
      }
      if (instruction.falls_through())
        line << " next=0x" << std::hex << instruction.address() + instruction.encoding().size();
      out_ << line.str() << '\n';
    }
    if (response.decode_failed())
      out_ << "disassembly incomplete within verified window\n";
    if (response.patch_size())
      out_ << "patch_size=" << response.patch_size() << " relocation=copy\n";
    else
      out_ << "patch_size=0 reason="
           << std::quoted(printable_proc_text(response.patch_diagnostic())) << '\n';
    return true;
  }
  case K::threads: {
    require_session(selected_);
    api::ListThreadsRequest request;
    request.set_session_id(selected_.id);
    const auto response = unary<api::ListThreadsResponse>(
        [&](auto &c, auto &r) { return stub_->ListThreads(&c, request, &r); });
    for (const auto &thread : response.threads())
      out_ << "tid=" << thread.tid() << " start=" << thread.start_time()
           << " name=" << std::quoted(printable_proc_text(thread.name())) << '\n';
    return true;
  }
  case K::symbols: {
    require_session(selected_);
    api::ListSymbolsRequest request;
    request.set_session_id(selected_.id);
    request.set_query(command.option("--filter"));
    request.set_module(command.option("--module"));
    request.set_page_size(static_cast<unsigned>(
        unsigned_number(command.option("--limit", "0"), "limit", UINT32_MAX)));
    request.set_page_token(command.option("--page"));
    request.set_mapping_token(command.option("--mapping"));
    request.set_include_addresses(command.has("--addresses"));
    if (command.has("--address"))
      request.set_address(address_number(command.option("--address")));
    const auto response = unary<api::ListSymbolsResponse>(
        [&](auto &c, auto &r) { return stub_->ListSymbols(&c, request, &r); });
    const bool addresses = request.include_addresses() || request.address();
    if (addresses) {
      if (response.mapping_token().empty())
        throw std::runtime_error("server does not support symbol address lookup");
      out_ << "mapping=" << response.mapping_token() << '\n';
    }
    for (const auto &symbol : response.symbols()) {
      out_ << symbol.selector() << " [" << symbol.module_path() << "]";
      if (symbol.runtime_address())
        out_ << " address=0x" << std::hex << symbol.runtime_address()
             << " elf=0x" << symbol.elf_address() << std::dec;
      else if (addresses)
        out_ << " address=unavailable";
      out_ << '\n';
    }
    if (!response.next_page_token().empty()) {
      out_ << "next_page=" << response.next_page_token() << '\n';
    }
    return true;
  }
  case K::prepare:
    prepare(args.at(0), command);
    return true;
  case K::resolve:
    out_ << resolve(args.at(0), command).DebugString();
    return true;
  case K::inject:
    inject(selected_, args.at(0), read_source(command.option("--callback")),
           command);
    return true;
  case K::validate:
  case K::draft_validate: {
    require_idle_target(selected_);
    const bool override_arguments = command.has("--allow-arg-overrides");
    const bool override_result = command.has("--allow-result-override");
    const bool replacement = command.has("--allow-replacement");
    require_session(selected_, override_arguments || override_result || replacement);
    const bool named = command.kind == K::draft_validate;
    const auto selector = named ? command.option("--target") : args.at(0);
    api::ResolveProbeRequest resolution;
    resolution_options(resolution, command);
    api::ValidateProbeRequest request;
    request.set_session_id(selected_.id);
    request.set_selector(selector);
    request.set_allow_argument_overrides(override_arguments);
    request.set_allow_result_override(override_result);
    request.set_allow_replacement(replacement);
    request.set_reclaimable(command.has("--reclaimable"));
    request.set_callback_source(named ? drafts_.at(args.at(0))
                                      : read_source(command.option("--callback")));
    *request.mutable_include_headers() = resolution.include_headers();
    *request.mutable_compile_options() = resolution.compile_options();
    request.set_module(resolution.module());
    request.set_compilation_database(resolution.compilation_database());
    const auto prepared = prepared_options(request, command, prepared_packages_, out_);
    const auto validate = [&] { return unary<api::ValidationReport>(
        [&](auto &c, auto &r) { return stub_->ValidateProbe(&c, request, &r); },
        settings_.timeout_seconds); };
    api::ValidationReport response;
    try {
      response = validate();
      if (prepared != PreparedMode::off && !response.prepared_used())
        throw std::runtime_error("server does not support prepared validation");
    } catch (const std::exception &error) {
      if (prepared != PreparedMode::prefer) throw;
      out_ << "prepared validation rejected; falling back to DWARF: " << error.what() << '\n';
      request.clear_prepared();
      request.set_selector(selector);
      response = validate();
    }
    out_ << response.DebugString();
    if (!response.success()) {
      throw std::runtime_error("interceptor validation failed");
    }
    return true;
  }
  case K::probe_reload: {
    require_idle_target(selected_);
    require_session(selected_);
    api::ReloadProbeRequest request;
    request.set_session_id(selected_.id);
    request.set_probe_id(unsigned_number(args.at(0), "probe ID"));
    const auto reload = [&](const std::string &source) {
      request.set_callback_source(source);
      const auto response = unary<api::ReloadProbeResponse>(
        [&](auto &c, auto &r) {
          Cancellation cancellation(c);
          return stub_->ReloadProbe(&c, request, &r);
        },
        settings_.timeout_seconds);
      out_ << "reloaded probe=" << response.probe().id()
           << " replaces=" << response.probe().replaces_probe_id()
           << " retained_limit=" << response.retained_limit();
      if (response.has_agent_retained())
        out_ << " agent_retained=" << response.agent_retained();
      else
        out_ << " agent_retained=unknown";
      out_ << '\n' << std::flush;
      request.set_probe_id(response.probe().id());
    };
    const auto path = command.option("--callback");
    if (!command.has("--watch")) {
      reload(read_source(path));
      return true;
    }
    auto previous = read_source(path);
    auto candidate = previous;
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(command.number("--timeout", 15));
    const auto limit = command.number("--count", 16);
    std::uint64_t count = 0;
    out_ << "probe watch ready probe=" << request.probe_id() << '\n' << std::flush;
    while (!interrupted.load() && count < limit && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (interrupted.load() || std::chrono::steady_clock::now() >= deadline)
        break;
      try {
        auto source = read_source(path);
        if (source != candidate) {
          candidate = std::move(source);
          continue;
        }
      } catch (const std::exception &) {
        candidate = previous;
        continue; // An editor may briefly remove the file during a save.
      }
      if (candidate == previous)
        continue;
      previous = candidate;
      try {
        reload(candidate);
      } catch (const std::exception &error) {
        out_ << "probe watch rejected: " << error.what() << '\n' << std::flush;
      }
      ++count;
    }
    out_ << "probe watch complete changes=" << count << " probe=" << request.probe_id() << '\n';
    return true;
  }
  case K::remove: {
    require_idle_target(selected_);
    require_session(selected_);
    api::RemoveProbeRequest request;
    request.set_session_id(selected_.id);
    request.set_probe_id(unsigned_number(args.at(0), "probe ID"));
    const auto response = unary<api::Probe>(
        [&](auto &c, auto &r) { return stub_->RemoveProbe(&c, request, &r); },
        settings_.timeout_seconds);
    out_ << "removed probe=" << response.id() << " restored="
         << (response.original_bytes_restored() ? "true" : "false")
         << (response.reclaimed() ? " reclaimed=true" : "") << '\n';
    if (!response.original_bytes_restored() && !response.superseded_by_probe_id()) {
      throw std::runtime_error("probe restoration incomplete");
    }
    return true;
  }
  case K::draft_load:
    draft(command.option("--as"), read_source(args.at(0)));
    out_ << "draft=" << command.option("--as") << '\n';
    return true;
  case K::draft_show:
    out_ << drafts_.at(args.at(0)) << '\n';
    return true;
  case K::draft_upload:
    inject(selected_, command.option("--target"), drafts_.at(args.at(0)),
           command);
    return true;
  case K::events:
    watch(command, true);
    return true;
  case K::collect:
    watch(command, false);
    return true;
  case K::record:
    watch(command, false, args.at(0));
    return true;
  case K::event_list: {
    const auto &b = buffer(args.at(0));
    std::size_t n = 0;
    const auto limit = command.number("--limit", 20);
    for (const auto &event : b.events()) {
      if (limit && n >= limit) {
        break;
      }
      out_ << n++ << " " << event_summary(event->message) << '\n';
    }
    return true;
  }
  case K::event_show: {
    auto b = batch(args.at(0));
    print(b.at(command.number("--index", 0)));
    return true;
  }
  case K::event_calls:
  case K::event_stats:
    event_statistics(buffer(args.at(0)), out_, command.kind == K::event_calls);
    return true;
  case K::event_clear:
    if (!batches_.erase(args.at(0))) {
      throw std::runtime_error("unknown collection");
    }
    return true;
  case K::event_open: {
    const auto name = command.option("--as");
    if (batches_.count(name)) {
      throw std::runtime_error("collection already exists: " + name +
                               "; choose a new name or use %events clear NAME");
    }
    auto loaded = read_recording(args.at(0), {});
    out_ << "opened events=" << loaded.received
         << " retained=" << loaded.events().size()
         << " evicted=" << loaded.evicted << '\n';
    batches_.emplace(name, std::move(loaded));
    return true;
  }
  case K::event_export:
    export_json(buffer(args.at(0)), command.option("--output"));
    return true;
  case K::processes:
    ensure_connected();
    for (const auto &p : processes(command.option("--filter"))) {
      out_ << p.pid << " start=" << p.start_time << " exe="
           << std::quoted(printable_proc_text(p.executable)) << " "
           << printable_proc_text(p.command) << '\n';
    }
    return true;
  case K::help:
  case K::cpp_load:
  case K::cpp_reset:
  case K::cpp_import:
  case K::cpp_options:
  case K::cpp_generate:
  case K::shell:
  case K::complete:
  case K::type:
  case K::draft_edit:
    throw std::logic_error("command requires the interactive driver");
  }
  throw std::logic_error("unhandled command");
}

void Controller::shutdown() {
  stop_background();
  std::string failures;
  for (auto it = owned_targets_.begin(); it != owned_targets_.end();) {
    try {
      it->second.process->terminate();
      pending_attaches_.erase(it->first);
      it = owned_targets_.erase(it);
    } catch (const std::exception &e) {
      failures += "pid=" + std::to_string(it->first) + ": " + e.what() + "\n";
      ++it;
    }
  }
  for (const auto &id : std::vector<std::string>(owned_sessions_.begin(),
                                                 owned_sessions_.end())) {
    try {
      detach({id, 0, connection_});
    } catch (const std::exception &e) {
      failures += "session=" + id + ": " + e.what() + "\n";
    }
  }
  if (server_.pid() >= 0) {
    try {
      server_.stop(settings_.timeout_seconds);
      owned_sessions_.clear();
      pending_attaches_.clear();
      disconnect();
    } catch (const std::exception &e) {
      failures += e.what();
    }
  }
  if (!pending_attaches_.empty())
    failures += "uncertain attach: " + reconcile_attaches() + "\n";
  if (!failures.empty()) {
    throw std::runtime_error("cleanup requires recovery at " + endpoint_ +
                             "\n" + failures);
  }
}

Facade::Facade(Controller &controller) : controller_(&controller) {}

const SessionHandle &Facade::selected_session() const {
  return controller_->selected_session();
}

SessionHandle Facade::attach(std::uint32_t pid) {
  return controller_->attach(pid);
}

void Facade::prepare(const std::string &selector, const std::string &header,
                     const std::string &output, const std::string &compdb,
                     const std::string &translation_unit) {
  prepare(selector, header, compdb, translation_unit, {}, output);
}

void Facade::prepare(const std::string &selector, const std::string &header,
                     const std::string &compdb, const std::string &translation_unit,
                     const std::string &module, const std::string &output) {
  prepare(selector, std::vector<std::string>{header}, compdb, translation_unit,
          module, output);
}

void Facade::prepare(const std::string &selector,
                     const std::vector<std::string> &headers,
                     const std::string &output, const std::string &compdb,
                     const std::string &translation_unit) {
  prepare(selector, headers, compdb, translation_unit, {}, output);
}

void Facade::prepare(const std::string &selector,
                     const std::vector<std::string> &headers,
                     const std::string &compdb, const std::string &translation_unit,
                     const std::string &module, const std::string &output) {
  if (headers.empty() ||
      std::any_of(headers.begin(), headers.end(), [](const auto &header) {
        return header.empty() || header.find('\0') != std::string::npos;
      }))
    throw std::invalid_argument("preparation requires nonempty header paths");
  Command options;
  options.options = {{"--include", headers}, {"--output", {output}}};
  if (!compdb.empty()) options.options["--compdb"] = {compdb};
  if (!translation_unit.empty()) options.options["--translation-unit"] = {translation_unit};
  if (!module.empty()) options.options["--module"] = {module};
  controller_->prepare(selector, options);
}

void Facade::prepare(const std::string &selector,
                     std::initializer_list<std::string> headers,
                     const std::string &output, const std::string &compdb,
                     const std::string &translation_unit) {
  prepare(selector, std::vector<std::string>(headers), output, compdb,
          translation_unit);
}

void Facade::prepare(const std::string &selector,
                     std::initializer_list<std::string> headers,
                     const std::string &compdb, const std::string &translation_unit,
                     const std::string &module, const std::string &output) {
  prepare(selector, std::vector<std::string>(headers), compdb, translation_unit,
          module, output);
}

void Facade::detach(const SessionHandle &session) {
  controller_->detach(session);
}

std::uint64_t Facade::inject(const SessionHandle &session,
                             const std::string &selector,
                             const std::string &file) {
  return controller_->inject(session, selector, read_source(file), Command{});
}

Collection Facade::collect(const SessionHandle &session, std::uint64_t count,
                           unsigned timeout) {
  return controller_->collect(session, count, timeout);
}

EventRange Facade::events(const SessionHandle& session, std::uint64_t count, unsigned timeout) {
  return controller_->events(session, count, timeout);
}

EventRange Facade::events_async(const SessionHandle& session, std::uint64_t count,
                                unsigned timeout) {
  return controller_->events_async(session, count, timeout);
}

RecordResult Facade::record(const std::string &path, const RecordOptions &options) {
  return controller_->record(path, options);
}

Collection Facade::replay(const std::string &path, const EventQuery &query) const {
  return controller_->replay(path, query);
}

Collection Facade::batch(const std::string &name) const {
  return controller_->batch(name);
}

std::uint64_t Facade::inject(const SessionHandle &session,
                            const std::string &selector, const std::string &file,
                            const std::string &prepared, const std::string &mode) {
  Command options;
  options.options = {{"--prepared", {prepared}}, {"--prepared-mode", {mode}}};
  return controller_->inject(session, selector, read_source(file), options);
}

void Facade::print(const Event &event) const { controller_->print(event); }
void Facade::print(const Collection &collection) const {
  controller_->print(collection);
}
} // namespace tracer_repl

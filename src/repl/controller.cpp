#include "controller.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>

#include <grpcpp/grpcpp.h>
#include <unistd.h>

#include "background_events.hpp"

extern char **environ;

namespace tracer_repl {
namespace api = clangtracer::v1;
static_assert(std::atomic<bool>::is_always_lock_free,
              "signal cancellation requires lock-free bool");
std::atomic<bool> interrupted{false};

namespace {
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

template <class Request>
PreparedMode prepared_options(Request &request, const Command &options,
                              const std::map<std::string, std::string> &packages,
                              std::ostream &out) {
  const auto selector = compact_selector(request.selector());
  auto directory = options.option("--prepared");
  if (!options.has("--prepared")) {
    const auto package = packages.find(selector);
    if (package != packages.end()) directory = package->second;
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
    const bool pinned = prepared.format_version() == 1 &&
                        prepared.module_sha256().size() == 32 && !prepared.module_name().empty();
    const bool source_only = prepared.format_version() == 2 &&
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

void Controller::require_session(const SessionHandle &session) {
  if (!stub_ || session.id.empty() || session.connection != connection_) {
    throw std::runtime_error(
        "no valid selected session; use %attach PID or %use SESSION");
  }
  api::ListSessionsRequest request;
  auto response = unary<api::ListSessionsResponse>(
      [&](auto &c, auto &r) { return stub_->ListSessions(&c, request, &r); });
  for (const auto &entry : response.sessions()) {
    if (entry.id() == session.id && entry.state() == api::Session::ATTACHED) {
      return;
    }
  }
  throw std::runtime_error("session is no longer attached: " + session.id);
}

SessionHandle Controller::attach(std::uint32_t pid) {
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
      selected_ = {s.id(), s.pid(), connection_};
      out_ << "selected existing session=" << s.id() << " pid=" << pid << '\n';
      return selected_;
    }
  }
  api::AttachRequest request;
  request.set_pid(pid);
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
  selected_ = {response.id(), response.pid(), connection_};
  out_ << "attached session=" << selected_.id << " pid=" << selected_.pid
       << '\n';
  return selected_;
}

void Controller::detach(const SessionHandle &session) {
  if (!stub_ || session.id.empty() || session.connection != connection_) {
    throw std::runtime_error("invalid session handle");
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
  prepared_packages_.insert_or_assign(compact_selector(selector), directory.string());
  out_ << "prepared directory=" << directory << '\n';
}

std::uint64_t Controller::inject(const SessionHandle &session,
                                 const std::string &selector,
                                 const std::string &source,
                                 const Command &options) {
  require_session(session);
  auto spec = resolve_for(session, selector, options);
  spec.set_callback_source(source);
  spec.set_stack_depth(
      static_cast<unsigned>(options.number("--stack-depth", 0)));
  spec.set_print_arguments(options.has("--print-arguments"));
  spec.set_print_result(options.has("--print-result"));
  api::AddProbeRequest request;
  request.set_session_id(session.id);
  *request.mutable_spec() = std::move(spec);
  const auto result = unary<api::Probe>(
      [&](auto &c, auto &r) { return stub_->AddProbe(&c, request, &r); },
      settings_.timeout_seconds);
  if (!result.active()) {
    throw std::runtime_error("probe installation did not become active");
  }
  out_ << "installed probe=" << result.id()
       << " session=" << result.session_id() << '\n';
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

void Controller::watch(const Command &command, bool display,
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

  if (!record_path.empty()) {
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
      continue;
    }
    if (output.fd >= 0) {
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
  next.complete = complete;
  next.cancelled = interrupted != 0;
  next.timed_out = status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED;
  out_ << "received=" << accepted << " cursor=" << cursor
       << " complete=" << (complete ? "true" : "false")
       << " cancelled=" << (next.cancelled ? "true" : "false")
       << " timed_out=" << (next.timed_out ? "true" : "false");
  if (record_path.empty()) {
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

std::vector<std::pair<unsigned, std::string>>
Controller::processes(const std::string &filter) const {
#ifdef __linux__
  if (server_.pid() < 0) {
    throw std::runtime_error("process listing is local-owned-server only; "
                             "remote attach requires an explicit PID");
  }
  std::vector<std::pair<unsigned, std::string>> result;
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
    std::error_code error;
    const auto executable =
        std::filesystem::read_symlink(entry.path() / "exe", error).string();
    if (error ||
        (!filter.empty() && executable.find(filter) == std::string::npos)) {
      continue;
    }
    std::ifstream input(entry.path() / "cmdline", std::ios::binary);
    std::string command;
    std::getline(input, command);
    std::replace(command.begin(), command.end(), '\0', ' ');
    if (!command.empty()) {
      result.emplace_back(pid, std::move(command));
    }
  }
  std::sort(result.begin(), result.end());
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
  if (choices.size() == 1) {
    attach(choices[0].first);
    return;
  }
  for (const auto &choice : choices) {
    out_ << choice.first << "  " << choice.second << '\n';
  }
  const auto choice = read("Attach PID (empty cancels)> ");
  if (choice.empty()) {
    return;
  }
  const auto pid = unsigned_number(choice, "PID", INT32_MAX);
  if (std::none_of(choices.begin(), choices.end(),
                   [&](const auto &p) { return p.first == pid; })) {
    throw std::invalid_argument("PID was not in the displayed process list");
  }
  attach(static_cast<unsigned>(pid));
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
    for (const auto &b : batches_) {
      out_ << "collection=" << b.first << " events=" << b.second.events().size()
           << " bytes=" << b.second.bytes() << '\n';
    }
    return true;
  }
  case K::attach:
    attach(
        static_cast<unsigned>(unsigned_number(args.at(0), "PID", INT32_MAX)));
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
    const auto response = unary<api::ListModulesResponse>(
        [&](auto &c, auto &r) { return stub_->ListModules(&c, request, &r); });
    for (const auto &module : response.modules()) {
      out_ << module << '\n';
    }
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
    const auto response = unary<api::ListSymbolsResponse>(
        [&](auto &c, auto &r) { return stub_->ListSymbols(&c, request, &r); });
    for (const auto &symbol : response.symbols()) {
      out_ << symbol.selector() << " [" << symbol.module_path() << "]\n";
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
  case K::validate: {
    require_session(selected_);
    api::ResolveProbeRequest resolution;
    resolution_options(resolution, command);
    api::ValidateProbeRequest request;
    request.set_session_id(selected_.id);
    request.set_selector(args.at(0));
    request.set_callback_source(read_source(command.option("--callback")));
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
      request.set_selector(args.at(0));
      response = validate();
    }
    out_ << response.DebugString();
    if (!response.success()) {
      throw std::runtime_error("interceptor validation failed");
    }
    return true;
  }
  case K::remove: {
    require_session(selected_);
    api::RemoveProbeRequest request;
    request.set_session_id(selected_.id);
    request.set_probe_id(unsigned_number(args.at(0), "probe ID"));
    const auto response = unary<api::Probe>(
        [&](auto &c, auto &r) { return stub_->RemoveProbe(&c, request, &r); },
        settings_.timeout_seconds);
    out_ << "removed probe=" << response.id() << " restored="
         << (response.original_bytes_restored() ? "true" : "false") << '\n';
    if (!response.original_bytes_restored()) {
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
    const auto reserved = background_storage();
    std::size_t events = reserved.first, bytes = reserved.second;
    for (const auto &b : batches_) {
      if (b.first != command.option("--as")) {
        bytes += b.second.bytes();
        events += b.second.events().size();
      }
    }
    if (bytes >= settings_.max_bytes || events >= settings_.max_events) {
      throw std::runtime_error(
          "clear collections before loading another recording");
    }
    auto loaded = open_recording(args.at(0), settings_.max_events - events,
                                 settings_.max_bytes - bytes);
    out_ << "opened events=" << loaded.received
         << " retained=" << loaded.events().size()
         << " evicted=" << loaded.evicted << '\n';
    batches_.insert_or_assign(command.option("--as"), std::move(loaded));
    return true;
  }
  case K::event_export:
    export_json(buffer(args.at(0)), command.option("--output"));
    return true;
  case K::processes:
    ensure_connected();
    for (const auto &p : processes(command.option("--filter"))) {
      out_ << p.first << " " << p.second << '\n';
    }
    return true;
  case K::help:
    out_ << "Use %commands for tracing, ordinary C++ for local analysis.\n"
         << "%server start [--executable PATH] [--agent PATH]; %server connect "
            "ENDPOINT; %server "
            "status; %server stop\n"
         << "%attach [PID]; %processes [--filter TEXT]; %sessions; %use "
            "SESSION; %status; %detach "
            "[SESSION]\n"
         << "%modules; %symbols [--filter TEXT] [--module TEXT] [--limit N] "
            "[--page TOKEN]\n"
         << "%probe resolve SELECTOR; %probe inject SELECTOR --callback FILE; "
            "%probe validate "
            "SELECTOR --callback FILE\n"
         << "%probe prepare SELECTOR --include HEADER --output NEW_DIRECTORY\n"
         << "  overrides: --compdb FILE --translation-unit FILE --module ELF (pin to a build)\n"
         << "  preparation defaults to compile_commands.json in the current directory\n"
         << "  uses the first compile command for a source beside the first header\n"
         << "  uses the prepared package for this selector when available, otherwise DWARF\n"
         << "  overrides: --prepared DIRECTORY --prepared-mode off|prefer|require\n"
         << "  options: --include SERVER_HEADER --compdb SERVER_PATH "
            "--compile-option FLAG "
            "--module TEXT\n"
         << "  deployment: --stack-depth 0..64 --print-arguments "
            "--print-result\n"
         << "%interceptor load FILE --as NAME; %interceptor edit NAME (.end "
            "finishes); "
            "%interceptor show NAME\n"
         << "%interceptor upload NAME --target SELECTOR; %probes; %probe "
            "remove ID\n"
         << "%events; %collect [--as NAME]; %record NEW_FILE.pb\n"
         << "  stream options: --count N --timeout SECONDS --after CURSOR "
            "--probe ID --phase "
            "PRE|POST\n"
         << "  --name SELECTOR --session ID --all; no count streams until "
            "Ctrl+C\n"
         << "%events list NAME [--limit N]; %events show NAME --index N; "
            "%events calls NAME; "
            "%events stats NAME\n"
         << "%events open FILE.pb --as NAME; %events export NAME --format json "
            "--output "
            "NEW_FILE.json; %events clear NAME\n"
         << "%cpp import HEADER [--type TYPE] [--output FILE]; %cpp load FILE; %cpp reset\n"
         << "%cpp generate HEADER [--output FILE] generates the whole include from its AST\n"
         << "%cpp options [add ARG... | clear | compdb DATABASE --file FILE]\n"
         << "%shell \"COMMAND\"; %cpp options add `COMMAND`; %help cpp; %quit\n"
         << "%help api describes the C++ API; %help Event / Collection / "
            "tracer show public declarations.\n"
         << "Tab completes C++ members; %complete \"tracer.\" lists candidates "
            "without executing code.\n";
    return true;
  case K::cpp_load:
  case K::cpp_reset:
  case K::cpp_import:
  case K::cpp_options:
  case K::cpp_generate:
  case K::shell:
  case K::complete:
  case K::draft_edit:
    throw std::logic_error("command requires the interactive driver");
  }
  throw std::logic_error("unhandled command");
}

void Controller::shutdown() {
  stop_background();
  std::string failures;
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
  Command options;
  options.options = {{"--include", {header}}, {"--output", {output}}};
  if (!compdb.empty()) options.options["--compdb"] = {compdb};
  if (!translation_unit.empty()) options.options["--translation-unit"] = {translation_unit};
  if (!module.empty()) options.options["--module"] = {module};
  controller_->prepare(selector, options);
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

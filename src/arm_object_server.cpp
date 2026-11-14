#include "tracer/compat.hpp"
#include "arm_object_watch.hpp"
#include "arm_method_watch.hpp"
#include "debug_resolver.hpp"
#include "proc_identity.hpp"
#include "tracer.grpc.pb.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
namespace api = clangtracer::v1;
namespace arm = tracer::arm_object_watch;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

struct Directory {
  std::string path = (fs::temp_directory_path() / "ct-arm-server-XXXXXX").string();
  Directory() {
    if (!::mkdtemp(path.data()))
      throw std::runtime_error("cannot create private AArch64 server directory");
  }
  ~Directory() {
    std::error_code ignored;
    fs::remove_all(path, ignored);
  }
};

struct Session {
  std::mutex mutex;
  api::Session info;
  std::unique_ptr<tracer::object_watch::Watch> diagnostic;
  std::unique_ptr<arm::NativeWatch> native;
  std::unique_ptr<arm::MethodWatch> methods;

  bool alive() const {
    return arm::same_process(info.pid(), info.process_start_time(), info.executable());
  }

  api::ObjectWatchStatus status() const {
    if (methods)
      return methods->status();
    if (native)
      return native->status();
    return diagnostic ? diagnostic->status() : api::ObjectWatchStatus{};
  }

  api::ObjectWatchStatus stop(bool detach) {
    if (methods)
      return methods->stop();
    if (native)
      return native->stop(detach ? api::ObjectObservation::WATCH_DETACHED :
                                   api::ObjectObservation::WATCH_STOPPED);
    return diagnostic ? diagnostic->stop(detach) : api::ObjectWatchStatus{};
  }

  void refresh() {
    if (info.state() == api::Session::ATTACHED && !alive()) {
      stop(false);
      info.set_state(api::Session::EXITED);
    }
  }
};

grpc::Status status_for(const std::exception &error) {
  return {dynamic_cast<const std::invalid_argument *>(&error) ?
              grpc::StatusCode::INVALID_ARGUMENT : grpc::StatusCode::FAILED_PRECONDITION,
          error.what()};
}

bool matches(const api::WatchEventsRequest &filter, const api::TraceEvent &event) {
  return (!filter.pid() || filter.pid() == event.pid()) &&
      (filter.session_ids().empty() || std::find(std::begin(filter.session_ids()), std::end(filter.session_ids()), event.session_id()) !=
                                         filter.session_ids().end()) &&
      (filter.names().empty() || std::find(std::begin(filter.names()), std::end(filter.names()), event.function_name()) !=
                                   filter.names().end());
}

class Service final : public api::Tracer::Service {
  std::string agent_, event_path_, instance_;
  std::chrono::seconds timeout_;
  std::size_t history_limit_;
  std::mutex sessions_mutex_, events_mutex_;
  std::map<std::string, std::shared_ptr<Session>> sessions_;
  std::atomic<std::uint64_t> next_watch_{1};
  std::deque<api::TraceEvent> events_;
  std::condition_variable events_changed_;
  std::uint64_t next_sequence_ = 1, evicted_ = 0, dropped_ = 0;

  std::shared_ptr<Session> find(const std::string &id) {
    const std::lock_guard lock(sessions_mutex_);
    const auto found = sessions_.find(id);
    return found == sessions_.end() ? nullptr : found->second;
  }

  std::vector<std::shared_ptr<Session>> sessions() {
    const std::lock_guard lock(sessions_mutex_);
    std::vector<std::shared_ptr<Session>> result;
    for (const auto &[id, session] : sessions_)
      result.push_back(session);
    return result;
  }

  void publish(api::TraceEvent event) {
    const std::lock_guard lock(events_mutex_);
    event.set_sequence(next_sequence_++);
    if (events_.size() == history_limit_) {
      evicted_ = events_.front().sequence();
      events_.pop_front();
      ++dropped_;
    }
    event.set_server_instance_id(instance_);
    event.set_server_dropped_events(dropped_);
    events_.push_back(std::move(event));
    events_changed_.notify_all();
  }

public:
  Service(std::string agent, const Directory &directory, std::chrono::seconds timeout,
          std::size_t history_limit)
      : agent_(std::move(agent)), event_path_(directory.path + "/events"),
        instance_(fs::path(directory.path).filename()), timeout_(timeout),
        history_limit_(history_limit) {}

  ~Service() override { sessions_.clear(); }

  grpc::Status Attach(grpc::ServerContext *, const api::AttachRequest *request,
                      api::Session *response) override {
    const auto pid = request->pid();
    if (!pid || pid > INT32_MAX || pid == static_cast<std::uint32_t>(::getpid()))
      return {grpc::StatusCode::INVALID_ARGUMENT, "PID must identify another process"};
    try {
      const auto path = tracer::proc::directory(pid);
      const auto start = tracer::proc::start_time(path / "stat");
      const auto executable = fs::read_symlink(path / "exe").string();
      if (!arm::same_process(pid, start, executable) ||
          (request->expected_process_start_time() && request->expected_process_start_time() != start) ||
          (!request->expected_executable().empty() && request->expected_executable() != executable))
        return {grpc::StatusCode::FAILED_PRECONDITION, "target identity changed"};
      const std::lock_guard lock(sessions_mutex_);
      for (auto &[id, session] : sessions_) {
        const std::lock_guard operation(session->mutex);
        session->refresh();
        if (session->info.pid() == pid && session->info.state() == api::Session::ATTACHED)
          return {grpc::StatusCode::ALREADY_EXISTS, "a session already owns this PID"};
      }
      auto session = std::make_shared<Session>();
      const auto id = instance_ + "-" + std::to_string(sessions_.size() + 1);
      session->info.set_id(id);
      session->info.set_pid(pid);
      session->info.set_process_start_time(start);
      session->info.set_executable(executable);
      session->info.set_state(api::Session::ATTACHED);
      sessions_.emplace(id, session);
      *response = session->info;
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ListSessions(grpc::ServerContext *, const api::ListSessionsRequest *,
                            api::ListSessionsResponse *response) override {
    try {
      for (const auto &session : sessions()) {
        const std::lock_guard operation(session->mutex);
        session->refresh();
        *response->add_sessions() = session->info;
      }
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status Detach(grpc::ServerContext *, const api::DetachRequest *request,
                      api::Session *response) override {
    const auto session = find(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    const std::lock_guard operation(session->mutex);
    try {
      session->refresh();
      if (session->info.state() == api::Session::ATTACHED) {
        const auto status = session->stop(true);
        if (status.restoration_pending())
          throw std::runtime_error(status.diagnostic());
        session->info.set_state(api::Session::DETACHED);
      }
      *response = session->info;
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ListModules(grpc::ServerContext *, const api::ListModulesRequest *request,
                           api::ListModulesResponse *response) override {
    const auto session = find(request->session_id());
    if (!session)
      return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
    const std::lock_guard operation(session->mutex);
    try {
      session->refresh();
      if (session->info.state() != api::Session::ATTACHED)
        return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
      *response = tracer::client::list_debug_module_mappings(session->info.pid());
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status ObjectWatch(grpc::ServerContext *, const api::ObjectWatchRequest *input,
                           api::ObjectWatchStatus *response) override {
    using Request = api::ObjectWatchRequest;
    auto request = *input;
    try {
      if (request.action() == Request::PREPARE) {
        if (!fs::path(request.executable_file()).is_absolute() || request.has_prepared_bindings())
          throw std::invalid_argument("offline preparation requires an absolute executable path");
        if (request.mode() == Request::SNAPSHOTS) {
          *response = arm::preview_snapshots(request, 0, fs::canonical(request.executable_file()));
          return grpc::Status::OK;
        }
        arm::validate(request);
        *response = tracer::object_watch::preview(request, 0, fs::canonical(request.executable_file()));
        return grpc::Status::OK;
      }
      const auto session = find(request.session_id());
      if (!session)
        return {grpc::StatusCode::NOT_FOUND, "session does not exist"};
      const std::lock_guard operation(session->mutex);
      session->refresh();
      const auto current = session->status();
      if (request.action() == Request::STATUS || request.action() == Request::STOP) {
        if (!current.watch_id() || (request.watch_id() && request.watch_id() != current.watch_id()))
          return {grpc::StatusCode::NOT_FOUND, "no matching object watch in this session"};
        *response = request.action() == Request::STATUS ? current : session->stop(false);
        return grpc::Status::OK;
      }
      if (request.action() != Request::PREVIEW && request.action() != Request::START)
        throw std::invalid_argument("unknown object watch action");
      if (session->info.state() != api::Session::ATTACHED)
        return {grpc::StatusCode::FAILED_PRECONDITION, "session is not attached"};
      const auto pid = session->info.pid();
      const auto &executable = session->info.executable();
      if (request.mode() == Request::SNAPSHOTS) {
        if (request.action() == Request::START && (current.active() || current.restoration_pending())) {
          if (session->methods)
            return {grpc::StatusCode::ALREADY_EXISTS, "a snapshot watch is already active"};
          return {grpc::StatusCode::FAILED_PRECONDITION, "stop the active object watch first"};
        }
        auto preview = arm::preview_snapshots(request, pid, executable);
        if (request.action() == Request::PREVIEW) {
          *response = std::move(preview);
          return grpc::Status::OK;
        }
        if (session->native)
          session->native->stop(api::ObjectObservation::WATCH_STOPPED);
        session->native.reset();
        session->diagnostic.reset();
        session->methods = std::make_unique<arm::MethodWatch>(request, next_watch_.fetch_add(1),
            session->info, preview, fs::path(event_path_).parent_path().string(), agent_, timeout_,
            [this](auto event) { publish(std::move(event)); });
        session->methods->start();
        *response = session->methods->status();
        return grpc::Status::OK;
      }
      arm::validate(request);
      if (request.writer_tid() &&
          !fs::exists(tracer::proc::directory(pid) / "task" / std::to_string(request.writer_tid())))
        throw std::invalid_argument("selected writer thread is not live in the target");
      if (request.action() == Request::PREVIEW) {
        *response = tracer::object_watch::preview(request, pid, executable);
        return grpc::Status::OK;
      }
      if (current.active() || current.restoration_pending())
        return {grpc::StatusCode::FAILED_PRECONDITION, "stop the active object watch first"};
      if (session->native)
        session->native->stop(api::ObjectObservation::WATCH_STOPPED);
      session->methods.reset();
      const auto binding = request.has_prepared_bindings() ?
          tracer::object_watch::preview(request, pid, executable).binding_status() : "live";
      const auto id = next_watch_.fetch_add(1);
      const auto start = session->info.process_start_time();
      const auto sink = [this](auto event) { publish(std::move(event)); };
      if (request.backend() == Request::CODE_CACHE) {
        auto next = std::make_unique<arm::NativeWatch>(
            request, id, pid, start, executable, event_path_, timeout_, sink);
        session->diagnostic.reset();
        session->native = std::move(next);
        session->native->start(agent_, request.application_rendezvous());
      } else {
        auto next = std::make_unique<tracer::object_watch::Watch>(
            request, id, pid, start, executable, timeout_, sink);
        session->native.reset();
        session->diagnostic = std::move(next);
      }
      *response = session->status();
      response->set_binding_status(binding);
      return grpc::Status::OK;
    } catch (const std::exception &error) {
      return status_for(error);
    }
  }

  grpc::Status WatchEvents(grpc::ServerContext *context, const api::WatchEventsRequest *filter,
                            grpc::ServerWriter<api::TraceEvent> *writer) override {
    if (filter->names_size() > 64 || filter->session_ids_size() > 64)
      return {grpc::StatusCode::INVALID_ARGUMENT, "at most 64 filters of each type are allowed"};
    auto cursor = filter->after_sequence();
    while (!context->IsCancelled()) {
      std::vector<api::TraceEvent> batch;
      {
        std::unique_lock lock(events_mutex_);
        if (cursor && cursor < evicted_)
          return {grpc::StatusCode::OUT_OF_RANGE, "event cursor is older than retained history"};
        for (const auto &event : events_) {
          if (event.sequence() <= cursor)
            continue;
          cursor = event.sequence();
          if (matches(*filter, event))
            batch.push_back(event);
          if (batch.size() == 64)
            break;
        }
        if (batch.empty())
          events_changed_.wait_for(lock, 100ms);
      }
      for (const auto &event : batch)
        if (!writer->Write(event))
          return grpc::Status::OK;
    }
    return grpc::Status::OK;
  }

  grpc::Status ReadEvents(grpc::ServerContext *, const api::ReadEventsRequest *request,
                           api::ReadEventsResponse *response) override {
    if (request->limit() > 256 || request->filter().names_size() > 64 ||
        request->filter().session_ids_size() > 64)
      return {grpc::StatusCode::INVALID_ARGUMENT, "event limit is 256; filter limit is 64"};
    const std::lock_guard lock(events_mutex_);
    if ((request->through_sequence() || !request->server_instance_id().empty()) &&
        request->server_instance_id() != instance_)
      return {grpc::StatusCode::FAILED_PRECONDITION, "event cursor belongs to another server"};
    const auto after = request->filter().after_sequence();
    if (after && after < evicted_)
      return {grpc::StatusCode::OUT_OF_RANGE, "event cursor is older than retained history"};
    const auto through = request->through_sequence() ? request->through_sequence() : next_sequence_ - 1;
    response->set_server_instance_id(instance_);
    response->set_through_sequence(through);
    response->set_server_dropped_events(dropped_);
    for (const auto &event : events_) {
      if (event.sequence() > through)
        break;
      if (event.sequence() > after && matches(request->filter(), event))
        *response->add_events() = event;
      if (response->events_size() == static_cast<int>(request->limit() ? request->limit() : 256))
        break;
    }
    return grpc::Status::OK;
  }

  grpc::Status AcknowledgeEvents(grpc::ServerContext *, const api::AcknowledgeEventsRequest *request,
                                  api::AcknowledgeEventsResponse *response) override {
    if (request->server_instance_id() != instance_ || request->sequences_size() > 256)
      return {grpc::StatusCode::INVALID_ARGUMENT, "matching server instance and at most 256 sequences required"};
    const std::lock_guard lock(events_mutex_);
    const auto end = std::remove_if(events_.begin(), events_.end(), [&](const auto &event) {
      return std::find(std::begin(request->sequences()), std::end(request->sequences()), event.sequence()) != request->sequences().end();
    });
    response->set_removed(std::distance(end, events_.end()));
    events_.erase(end, events_.end());
    return grpc::Status::OK;
  }

  bool shutdown() {
    bool clean = true;
    for (const auto &session : sessions()) {
      const std::lock_guard operation(session->mutex);
      try {
        const auto status = session->stop(true);
        if (status.restoration_pending())
          throw std::runtime_error(status.diagnostic());
      } catch (const std::exception &error) {
        std::cerr << session->info.id() << ' ' << error.what() << '\n';
        clean = false;
      }
    }
    return clean;
  }
};

unsigned positive(std::string_view value) {
  unsigned number = 0;
  const auto end = value.data() + value.size();
  const auto result = std::from_chars(value.data(), end, number);
  if (result.ec != std::errc{} || result.ptr != end || !number)
    throw std::invalid_argument("expected a positive number");
  return number;
}

std::string default_agent() {
  if (const char *path = std::getenv("CLANG_TRACER_AGENT"); path && *path)
    return path;
  const auto directory = fs::canonical("/proc/self/exe").parent_path();
  const auto local = directory / "libclang_tracer_object_cache_arm.so";
  return fs::exists(local) ? local : directory / TRACER_ARM_AGENT_RELATIVE_PATH;
}
}

int main(int argc, char **argv) {
  try {
    std::string endpoint, agent;
    std::chrono::seconds timeout = 10s;
    unsigned history = 4096;
    for (int index = 1; index < argc; ++index) {
      const std::string_view option = argv[index];
      if (option == "--help") {
        std::cout << "Usage: clang-tracer-server [--agent FILE] [--listen unix:///PATH] "
                     "[--timeout SECONDS] [--event-history-limit N]\n";
        return 0;
      }
      if (++index == argc)
        throw std::invalid_argument("missing server option value");
      const std::string_view value = argv[index];
      if (option == "--agent")
        agent = value;
      else if (option == "--listen")
        endpoint = value;
      else if (option == "--timeout")
        timeout = std::chrono::seconds(positive(value));
      else if (option == "--event-history-limit")
        history = positive(value);
      else
        throw std::invalid_argument("unknown AArch64 server option " + std::string(option));
    }
    if (timeout > 300s)
      throw std::invalid_argument("server timeout must be at most 300 seconds");
    Directory directory;
    if (endpoint.empty())
      endpoint = "unix://" + directory.path + "/control.sock";
    if (!::tracer::compat::starts_with(endpoint, "unix:///"))
      throw std::invalid_argument("AArch64 server requires an absolute Unix socket endpoint");
    const fs::path socket = endpoint.substr(7);
    struct stat parent{};
    if (::stat(socket.parent_path().c_str(), &parent) || parent.st_uid != ::geteuid() ||
        (parent.st_mode & 077) || fs::symlink_status(socket).type() != fs::file_type::not_found)
      throw std::invalid_argument("Unix socket must be new and in a private owned directory");
    ::umask(0077);
    sigset_t signals;
    ::sigemptyset(&signals);
    ::sigaddset(&signals, SIGINT);
    ::sigaddset(&signals, SIGTERM);
    if (::pthread_sigmask(SIG_BLOCK, &signals, nullptr))
      throw std::runtime_error("cannot block server stop signals");
    Service service(agent.empty() ? default_agent() : agent, directory, timeout, history);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    if (!server)
      throw std::runtime_error("cannot bind AArch64 server socket");
    std::cout << "clang-tracer-server listening at " << endpoint << std::endl;
    int signal = 0;
    ::sigwait(&signals, &signal);
    server->Shutdown(std::chrono::system_clock::now() + 1s);
    server->Wait();
    return service.shutdown() ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer-server: " << error.what() << '\n';
    return 1;
  }
}

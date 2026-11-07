#pragma once
#include <atomic>
#include <functional>
#include <iosfwd>
#include <map>
#include <set>

#include <signal.h>

#include "api.hpp"
#include "commands.hpp"
#include "events.hpp"
#include "owned_target.hpp"
#include "server_process.hpp"
#include "tracer.grpc.pb.h"

namespace tracer_repl {
extern std::atomic<bool> interrupted;

struct Settings {
  std::string endpoint, server, agent;
  unsigned timeout_seconds = 30;
  std::size_t max_events = 4096, max_bytes = 64 * 1024 * 1024;
};

class Controller {
  Settings settings_;
  ServerProcess server_;
  std::unique_ptr<clangtracer::v1::Tracer::Stub> stub_;
  std::string endpoint_;
  std::uint64_t connection_ = 1;
  SessionHandle selected_;
  std::set<std::string> owned_sessions_;
  struct PendingAttach {
    std::set<std::string> previous_ids;
    std::set<std::string> candidate_ids;
  };
  std::map<std::uint32_t, PendingAttach> pending_attaches_;
  struct OwnedLaunch {
    std::unique_ptr<OwnedTarget> process;
    SessionHandle session;
  };
  std::map<std::uint32_t, OwnedLaunch> owned_targets_;
  std::map<std::string, std::string> drafts_;
  struct PreparedPackage {
    std::string directory;
    clangtracer::v1::PreparedInjector artifact;
  };
  std::map<std::string, PreparedPackage> prepared_packages_;
  std::vector<std::string> pending_imports_;
  mutable std::vector<clangtracer::v1::PreparedInjector> pending_replay_types_;
  std::map<std::string, EventBuffer> batches_;
  std::vector<std::weak_ptr<BackgroundEvents>> background_;
  void stop_background(const std::string& session_id = {});
  std::pair<std::size_t, std::size_t> background_storage() const;
  std::ostream &out_;
  void ensure_connected();
  void start_server(const Command &command);
  void connect(const std::string &endpoint);
  void disconnect();
  void prune_owned_targets();
  void require_no_owned_targets();
  void launch(const Command &command);
  void control_target(const Command &command);
  void call_target(const Command &command);
  void require_idle_target(const SessionHandle &session) const;
  std::string reconcile_attaches(bool *snapshot_available = nullptr);
  clangtracer::v1::ProbeSpec resolve(const std::string &selector,
                                     const Command &command);
  clangtracer::v1::ProbeSpec resolve_for(const SessionHandle &session,
                                        const std::string &selector,
                                        const Command &command);
  RecordResult watch(const Command &command, bool display,
                     const std::string &record_path = {});
  EventBuffer read_recording(const std::string &path,
                             const EventQuery &query) const;
  const EventBuffer &buffer(const std::string &name) const;
  void preview_bulk_trace(const Command &command);
  void apply_bulk_trace(const Command &command);
  void remove_bulk_trace(const std::string &batch_id);
  struct ProcessChoice {
    std::uint32_t pid;
    std::uint64_t start_time;
    std::string executable;
    std::string command;
  };
  std::vector<ProcessChoice> processes(const std::string &filter) const;

public:
  explicit Controller(Settings settings, std::ostream &out);
  ~Controller();
  bool execute(const Command &command);
  void shutdown();

  const SessionHandle &selected_session() const { return selected_; }

  std::string prompt() const;
  void prepare(const std::string &selector, const Command &options);
  std::vector<std::string> take_pending_imports();
  std::vector<clangtracer::v1::PreparedInjector> take_pending_replay_types();
  SessionHandle attach(std::uint32_t pid, std::uint64_t expected_start_time = 0,
                       const std::string &expected_executable = {});
  void detach(const SessionHandle &session);
  void require_session(const SessionHandle &session, bool scalar_overrides = false);
  std::uint64_t inject(const SessionHandle &session,
                       const std::string &selector, const std::string &source,
                       const Command &options);
  Collection collect(const SessionHandle &session, std::uint64_t count,
                     unsigned timeout);
  EventRange events(const SessionHandle& session, std::uint64_t count, unsigned timeout);
  EventRange events_async(const SessionHandle& session, std::uint64_t count, unsigned timeout);
  RecordResult record(const std::string &path, const RecordOptions &options);
  Collection replay(const std::string &path, const EventQuery &query) const;
  Collection batch(const std::string &name) const;
  void draft(const std::string &name, std::string source);
  void print(const Event &event) const;
  void print(const Collection &collection) const;
  void
  select_process(const std::string &filter,
                 const std::function<std::string(const std::string &)> &read);
};
} // namespace tracer_repl

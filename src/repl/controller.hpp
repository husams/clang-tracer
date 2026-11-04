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
  std::map<std::string, std::string> drafts_;
  std::map<std::string, EventBuffer> batches_;
  std::ostream &out_;
  void ensure_connected();
  void start_server(const Command &command);
  void connect(const std::string &endpoint);
  void disconnect();
  std::string reconcile_attaches(bool *snapshot_available = nullptr);
  clangtracer::v1::ProbeSpec resolve(const std::string &selector,
                                     const Command &command);
  void watch(const Command &command, bool display,
             const std::string &record_path = {});
  const EventBuffer &buffer(const std::string &name) const;

public:
  explicit Controller(Settings settings, std::ostream &out);
  ~Controller();
  bool execute(const Command &command);
  void shutdown();

  const SessionHandle &selected_session() const { return selected_; }

  std::string prompt() const;
  SessionHandle attach(std::uint32_t pid);
  void detach(const SessionHandle &session);
  void require_session(const SessionHandle &session);
  std::uint64_t inject(const SessionHandle &session,
                       const std::string &selector, const std::string &source,
                       const Command &options);
  Collection collect(const SessionHandle &session, std::uint64_t count,
                     unsigned timeout);
  Collection batch(const std::string &name) const;
  void draft(const std::string &name, std::string source);
  void print(const Event &event) const;
  std::vector<std::pair<unsigned, std::string>>
  processes(const std::string &filter) const;
  void
  select_process(const std::string &filter,
                 const std::function<std::string(const std::string &)> &read);
};
} // namespace tracer_repl

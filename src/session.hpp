#pragma once

#include <chrono>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "admission.hpp"

#include "tracer.grpc.pb.h"
#include "tracer/protocol.hpp"

namespace tracer {
namespace injector {
class Process;
class EntryTrap;
}
namespace object_watch {
class Watch;
}
namespace object_cache {
class Watch;
}

class AgentRuntimeIncompatible final : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class Session final {
public:
  Session(std::string id, std::uint32_t pid, std::uint64_t process_start,
          std::string agent_path, std::string event_path,
          std::string context_endpoint, std::string context_durable_directory,
          std::chrono::seconds operation_timeout,
          ContextTransportBudget context_budget = {});
  ~Session();

  void attach(const std::string &expected_executable);
  clangtracer::v1::Probe add_probe(const clangtracer::v1::ProbeSpec &spec,
                                   const std::string &source_directory,
                                   std::uint64_t probe_id,
                                   const std::array<std::uint8_t, 16> &lineage,
                                   const std::string &bulk_batch_id = {},
                                   std::uint64_t reload_probe_id = 0,
                                   const std::function<bool()> &cancelled = {});
  clangtracer::v1::ProbeSpec reload_spec(std::uint64_t probe_id, std::string source);
  clangtracer::v1::ValidationReport
  validate_probe(const clangtracer::v1::ProbeSpec &spec,
                 const std::string &source_directory,
                 clangtracer::v1::ValidateProbeRequest::Level level);
  clangtracer::v1::Probe remove_probe(std::uint64_t probe_id);
  void detach();
  clangtracer::v1::ObjectWatchStatus object_watch(
      const clangtracer::v1::ObjectWatchRequest &request, std::uint64_t id,
      std::function<void(clangtracer::v1::TraceEvent)> publish);
  static clangtracer::v1::ObjectWatchStatus prepare_object_watch(
      const clangtracer::v1::ObjectWatchRequest &request);
  void shutdown() noexcept;
  void record_failure(const std::string &message);
  void refresh_process_state() noexcept;

  [[nodiscard]] clangtracer::v1::Session snapshot() const;
  [[nodiscard]] std::optional<unsigned> agent_retained_count() const;
  [[nodiscard]] std::uint64_t events_sent(std::uint64_t probe_id) const;
  [[nodiscard]] bool accepts_event(std::uint64_t probe_id,
                                   std::uint32_t pid) const;
  [[nodiscard]] std::string id() const { return id_; }
  [[nodiscard]] std::uint32_t pid() const { return pid_; }
  [[nodiscard]] std::uint64_t process_start_time() const { return process_start_; }
  [[nodiscard]] std::mutex &operation_mutex() { return operation_mutex_; }
  [[nodiscard]] static std::uint64_t ProcessStartTime(std::uint32_t pid);

private:
  struct PthreadCreatePatch {
    std::uintptr_t entry = 0;
    std::uintptr_t trampoline = 0;
    std::uintptr_t wrapper = 0;
    std::uint32_t size = 0;
    std::array<std::uint8_t, kMaxPatchBytes> original{};
    std::array<std::uint8_t, kMaxPatchBytes> installed{};
  };

  struct ProbeRecord {
    clangtracer::v1::Probe value;
    clangtracer::v1::ProbeSpec reload_spec;
    std::uintptr_t remote_request = 0;
    std::string source_file;
    bool patch_committed = false;
    std::uint64_t events_sent = 0;
  };

  std::string id_;
  std::uint32_t pid_;
  std::uint64_t process_start_;
  std::string agent_path_;
  std::string event_path_;
  std::string context_endpoint_;
  std::string context_durable_directory_;
  std::chrono::seconds operation_timeout_;
  ContextTransportBudget context_budget_;
  std::vector<std::string> discovered_module_paths_;
  std::vector<injector::ApplicationModule> application_modules_;
  mutable std::mutex mutex_;
  std::mutex operation_mutex_;
  clangtracer::v1::Session::State state_ = clangtracer::v1::Session::ATTACHED;
  std::string error_;
  std::string executable_;
  std::optional<clangtracer::v1::JitRuntimeProfile> jit_runtime_;
  bool unsafe_target_ = false;
  std::uintptr_t start_address_ = 0;
  std::uintptr_t stop_address_ = 0;
  std::uintptr_t validate_address_ = 0;
  std::uintptr_t capacity_address_ = 0;
  std::uintptr_t object_cache_address_ = 0;
  std::uintptr_t pthread_detour_address_ = 0;
  std::map<std::uint64_t, ProbeRecord> probes_;
  std::unique_ptr<object_watch::Watch> object_watch_;
  std::unique_ptr<object_cache::Watch> object_cache_;
  std::unique_ptr<injector::EntryTrap> entry_trap_;
  std::uint64_t trap_probe_id_ = 0;
  std::optional<PthreadCreatePatch> pthread_create_patch_;
  bool pthread_create_patch_committed_ = false;

  void attach_process(tracer::injector::Process &process,
                      std::chrono::steady_clock::time_point deadline);
  void check_identity_locked();
  void require_probe_control_locked(std::uint64_t removing = 0) const;
  void stop_object_watch_locked();
  void stop_object_cache_locked(bool detaching = false);
  bool retire_changed_object_cache_locked(std::chrono::steady_clock::time_point deadline);
  void set_failed_locked(const std::string &message);
  ProbeRecord &find_probe_locked(std::uint64_t probe_id);
  void release_request_locked(ProbeRecord &probe,
                              std::chrono::steady_clock::time_point deadline);
  bool
  stop_probe_locked(ProbeRecord &probe, bool shutdown_mode,
                    std::chrono::steady_clock::time_point shutdown_deadline =
                        std::chrono::steady_clock::time_point::max());
};

} // namespace tracer

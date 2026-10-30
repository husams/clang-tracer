#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "admission.hpp"

#include "tracer.grpc.pb.h"
#include "tracer/protocol.hpp"

namespace tracer {
namespace injector {
class Process;
}

class Session final {
public:
  Session(std::string id, std::uint32_t pid, std::uint64_t process_start,
          std::string agent_path, std::string event_path,
          std::chrono::seconds operation_timeout);

  void attach();
  clangtracer::v1::Probe add_probe(const clangtracer::v1::ProbeSpec &spec,
                                   const std::string &source_directory,
                                   std::uint64_t probe_id);
  clangtracer::v1::Probe remove_probe(std::uint64_t probe_id);
  void detach();
  void shutdown() noexcept;
  void record_failure(const std::string &message);
  void refresh_process_state() noexcept;

  [[nodiscard]] clangtracer::v1::Session snapshot() const;
  [[nodiscard]] std::uint64_t events_sent(std::uint64_t probe_id) const;
  [[nodiscard]] bool accepts_event(std::uint64_t probe_id,
                                   std::uint32_t pid) const;
  [[nodiscard]] std::string id() const { return id_; }
  [[nodiscard]] std::uint32_t pid() const { return pid_; }
  [[nodiscard]] std::mutex &operation_mutex() { return operation_mutex_; }
  [[nodiscard]] static std::uint64_t ProcessStartTime(std::uint32_t pid);

private:
  struct ProbeRecord {
    clangtracer::v1::Probe value;
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
  std::chrono::seconds operation_timeout_;
  std::vector<std::string> discovered_module_paths_;
  std::vector<injector::ApplicationModule> application_modules_;
  mutable std::mutex mutex_;
  std::mutex operation_mutex_;
  clangtracer::v1::Session::State state_ = clangtracer::v1::Session::ATTACHED;
  std::string error_;
  bool unsafe_target_ = false;
  std::uintptr_t start_address_ = 0;
  std::uintptr_t stop_address_ = 0;
  std::map<std::uint64_t, ProbeRecord> probes_;

  void attach_process(tracer::injector::Process &process,
                      std::chrono::steady_clock::time_point deadline);
  void check_identity_locked();
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

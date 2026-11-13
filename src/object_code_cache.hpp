#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "tracer.pb.h"

namespace tracer::injector { class Process; }

namespace tracer::object_cache {
std::uintptr_t load_agent(injector::Process &process, const std::string &path);

class Watch {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  Watch(const clangtracer::v1::ObjectWatchRequest &request, std::uint64_t id,
        std::uint32_t pid, const std::string &executable, const std::string &event_path,
        std::function<void(clangtracer::v1::TraceEvent)> publish);
  ~Watch();
  void install(injector::Process &process, std::uintptr_t agent_entry,
               bool permanent_startup_module = false);
  void stop_runtime_admissions();
  bool restore(injector::Process &process);
  bool retire_if_mapping_changed(std::chrono::steady_clock::time_point deadline);
  void finish(std::chrono::steady_clock::time_point deadline,
      clangtracer::v1::ObjectObservation::ClosureReason reason);
  clangtracer::v1::ObjectWatchStatus status() const;
  clangtracer::v1::ObjectWatchStatus preview() const;
};
} // namespace tracer::object_cache

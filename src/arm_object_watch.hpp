#pragma once

#include "object_watch.hpp"

namespace tracer::arm_object_watch {
void validate(clangtracer::v1::ObjectWatchRequest &request);
clangtracer::v1::ObjectWatchStatus preview_snapshots(
    const clangtracer::v1::ObjectWatchRequest &request, std::uint32_t pid,
    const std::string &executable);

bool same_process(std::uint32_t pid, std::uint64_t start, const std::string &executable);

class NativeWatch {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  NativeWatch(const clangtracer::v1::ObjectWatchRequest &request, std::uint64_t id,
      std::uint32_t pid, std::uint64_t start, const std::string &executable,
      const std::string &event_path, std::chrono::seconds timeout,
      object_watch::Watch::Publish publish);
  ~NativeWatch();
  void start(const std::string &agent, const std::string &rendezvous);
  clangtracer::v1::ObjectWatchStatus status() const;
  clangtracer::v1::ObjectWatchStatus stop(clangtracer::v1::ObjectObservation::ClosureReason reason);
};
} // namespace tracer::arm_object_watch

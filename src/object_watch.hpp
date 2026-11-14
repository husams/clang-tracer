#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "tracer.pb.h"

namespace tracer::object_watch {
clangtracer::v1::ObjectWatchStatus preview(
    const clangtracer::v1::ObjectWatchRequest &request, std::uint32_t pid,
    const std::string &executable);

class Watch {
  struct Worker;
  std::unique_ptr<Worker> worker_;

public:
  using Publish = std::function<void(clangtracer::v1::TraceEvent)>;
  Watch(const clangtracer::v1::ObjectWatchRequest &, std::uint64_t id,
        std::uint32_t pid, std::uint64_t process_start, std::string executable,
        std::chrono::seconds timeout, Publish publish);
  Watch(const clangtracer::v1::ObjectWatchRequest &, std::uint64_t id,
        std::vector<std::string> argv, std::chrono::seconds timeout, Publish publish);
  ~Watch();
  clangtracer::v1::ObjectWatchStatus status() const;
  clangtracer::v1::ObjectWatchStatus stop(bool detaching = false);
  std::uint32_t target_pid() const;
  std::optional<int> target_exit_status() const;
};
} // namespace tracer::object_watch

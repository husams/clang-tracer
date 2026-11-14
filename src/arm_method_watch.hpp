#pragma once

#include "object_watch.hpp"

namespace tracer::arm_object_watch {
class MethodWatch {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  MethodWatch(const clangtracer::v1::ObjectWatchRequest &, std::uint64_t id,
      const clangtracer::v1::Session &, const clangtracer::v1::ObjectWatchStatus &preview,
      const std::string &directory, const std::string &agent,
      std::chrono::seconds timeout, object_watch::Watch::Publish publish);
  ~MethodWatch();
  void start();
  clangtracer::v1::ObjectWatchStatus status();
  clangtracer::v1::ObjectWatchStatus stop();
};
}

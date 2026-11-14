#pragma once

#include <functional>
#include <map>
#include <string_view>

#include <clang_tracer/live.hpp>

#include "live_data.hpp"

namespace tracer::live::data {
using ObjectView = clang_tracer::live_detail::ObjectView;
using Discover = void (*)(clang_tracer::ObjectSink&);
using Invoke = void (*)(void*, const std::shared_ptr<void>*);

struct Callback {
  Invoke invoke;
  bool requires_owner;
  std::size_t size, alignment;
};

struct UpdateProgram {
  std::shared_ptr<JitImage> image;
  std::map<std::string, Callback, std::less<>> callbacks;
};

struct DiscoveryProgram {
  std::shared_ptr<JitImage> image;
  Discover invoke;
  std::string root;
};

UpdateProgram compile_update(const JitRequest&);
DiscoveryProgram compile_discovery(const LiveDataDiscovery&);
void retain(std::shared_ptr<JitImage>);

struct Candidate {
  LiveHeapCandidate report;
  std::size_t size;
  std::uintptr_t vptr;
};

struct Scan {
  std::vector<Candidate> candidates;
  std::vector<std::string> limitations;
};

// Validates and retains a loader reference to the selected runtime image.
class TargetImage {
public:
  explicit TargetImage(const LiveDataTarget&);
  ~TargetImage();
  TargetImage(const TargetImage&) = delete;
  TargetImage& operator=(const TargetImage&) = delete;

  void verify() const;
  bool module_matches(std::string_view selector) const;
  bool verified_layout(const ObjectView&) const;

  Scan scan(const std::vector<std::string>& types) const;
  bool matches(const Candidate&) const;
  // Visit build-matched std::atomic<std::shared_ptr<T>> roots.
  void discover_owners(const std::vector<std::string>& types, clang_tracer::ObjectSink&) const;

private:
  struct Image;
  std::unique_ptr<Image> image_;
};
} // namespace tracer::live::data

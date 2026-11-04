#pragma once
#include <deque>
#include <iosfwd>

#include "api.hpp"
#include "tracer.pb.h"

namespace tracer_repl {
struct EventData {
  clangtracer::v1::TraceEvent message;
  std::size_t retained_bytes = 0;
};

class EventBuffer {
  std::deque<std::shared_ptr<const EventData>> events_;
  std::size_t max_events_, max_bytes_, bytes_ = 0;

public:
  std::uint64_t received = 0, evicted = 0;
  bool complete = false, cancelled = false, timed_out = false;
  explicit EventBuffer(std::size_t max_events = 4096,
                       std::size_t max_bytes = 64 * 1024 * 1024);
  void push(const clangtracer::v1::TraceEvent &event);

  const auto &events() const { return events_; }

  std::size_t bytes() const { return bytes_; }

  Collection snapshot() const;
};

std::string value_text(const clangtracer::v1::Value &value);
std::string event_summary(const clangtracer::v1::TraceEvent &event);
void event_statistics(const EventBuffer &buffer, std::ostream &out, bool calls);
void write_record(int fd, const clangtracer::v1::TraceEvent &event);
int create_recording(const std::string &path);
EventBuffer open_recording(const std::string &path, std::size_t max_events,
                           std::size_t max_bytes);
void export_json(const EventBuffer &buffer, const std::string &path);
} // namespace tracer_repl

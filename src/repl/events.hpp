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
  std::vector<clangtracer::v1::PreparedInjector> replay_types;
  explicit EventBuffer(std::size_t max_events = 4096,
                       std::size_t max_bytes = 64 * 1024 * 1024);
  void push(const clangtracer::v1::TraceEvent &event);

  const auto &events() const { return events_; }

  std::size_t bytes() const { return bytes_; }

  Collection snapshot() const;
};

std::string value_text(const clangtracer::v1::Value &value);
std::string event_summary(const clangtracer::v1::TraceEvent &event);
void print_event_stack(const clangtracer::v1::TraceEvent &event, std::ostream &out);
void event_statistics(const EventBuffer &buffer, std::ostream &out, bool calls);
void print_receiver_delta(const clangtracer::v1::TraceEvent &before,
                          const clangtracer::v1::TraceEvent &after,
                          const std::vector<std::string> &keys, std::ostream &out);
void write_record(int fd, const clangtracer::v1::TraceEvent &event);
int create_recording(const std::string &path);
EventBuffer open_recording(const std::string &path, std::size_t max_events,
                           std::size_t max_bytes, const EventQuery &query = {});
void export_json(const EventBuffer &buffer, const std::string &path);
} // namespace tracer_repl

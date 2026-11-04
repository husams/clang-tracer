#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tracer_repl {
class Controller;
struct EventData;

struct SessionHandle {
  std::string id;
  std::uint32_t pid = 0;
  std::uint64_t connection = 0;
};

class Event {
  std::shared_ptr<const EventData> data_;

public:
  explicit Event(std::shared_ptr<const EventData> data);
  std::uint64_t sequence() const;
  std::uint64_t probe_id() const;
  std::uint64_t call_id() const;
  std::string phase() const;
  std::string function_name() const;
  std::string argument(std::size_t index) const;
  std::string result() const;
  std::string describe() const;
};

struct Collection {
  std::vector<Event> events;
  std::uint64_t received = 0;
  std::uint64_t evicted = 0;
  bool complete = false;
  bool cancelled = false;
  bool timed_out = false;

  std::size_t size() const { return events.size(); }

  const Event &at(std::size_t index) const { return events.at(index); }

  auto begin() const { return events.begin(); }

  auto end() const { return events.end(); }
};

class Facade {
  Controller *controller_;

public:
  explicit Facade(Controller &controller);
  const SessionHandle &selected_session() const;
  SessionHandle attach(std::uint32_t pid);
  void detach(const SessionHandle &session);
  std::uint64_t inject(const SessionHandle &session,
                       const std::string &selector,
                       const std::string &callback_file);
  Collection collect(const SessionHandle &session, std::uint64_t count,
                     unsigned timeout_seconds = 30);
  Collection batch(const std::string &name) const;
  void print(const Event &event) const;
};
} // namespace tracer_repl

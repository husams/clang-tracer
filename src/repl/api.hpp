#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "event_value.hpp"
#include "event_ranges.hpp"

namespace tracer_repl {
class Controller;
struct EventData;
class BackgroundEvents;

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
  // Presence includes VOID results; reading void as a value is rejected.
  bool has_result() const;
  // Values own their scalar data and do not retain target addresses.
  ScalarValue argument_value(std::size_t index) const;
  ScalarValue result_value() const;
  EventValue argument_snapshot(std::size_t index) const;
  EventValue result_snapshot() const;
  // Owned receiver fields at method entry, available on PRE and POST.
  EventValue receiver_snapshot() const;
  // Owned state from a named Context event, in the event's state order.
  EventValue state_snapshot(std::size_t index = 0) const;

  template <class T> T state_as(std::size_t index = 0) const {
    return state_snapshot(index).template as<T>();
  }

  template <class T> T argument_as(std::size_t index) const {
    return argument_snapshot(index).template as<T>();
  }

  template <class T> T result_as() const {
    return result_snapshot().template as<T>();
  }

  std::string argument(std::size_t index) const;
  std::string result() const;
  std::string describe() const;
};

struct Collection {
  Collection() = default;
  Collection(const Collection &) = default;
  Collection(Collection &&) noexcept = default;
  Collection &operator=(const Collection &) = default;
  Collection &operator=(Collection &&) noexcept = default;
  // Export a stable destructor for JIT temporaries instead of relying on an
  // inline destructor being emitted by an earlier input cell.
  ~Collection();
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

struct EventQuery {
  // Match any exact function selector or custom event name.
  std::vector<std::string> names;
  std::string phase;
  std::uint32_t pid = 0;
  std::string session_id;
  // Zero reads all matches, subject to the collection's retention limits.
  std::uint64_t count = 0;
};

struct RecordOptions {
  // An omitted session uses the selected attachment.
  std::optional<SessionHandle> session;
  bool all_sessions = false;
  // Match any exact function selector or custom event name.
  std::vector<std::string> names;
  std::string phase;
  std::uint64_t probe_id = 0;
  std::uint64_t after_sequence = 0;
  // Zero count streams until cancelled or timed out.
  std::uint64_t count = 0;
  // Zero uses the console timeout for counted recordings.
  unsigned timeout_seconds = 0;
};

struct RecordResult {
  std::uint64_t received = 0;
  std::uint64_t cursor = 0;
  bool complete = false;
  bool cancelled = false;
  bool timed_out = false;
};

// Iteration visits a stable snapshot of the events retained at begin().
// It never waits for new events; collection continues independently.
class EventRange {
  std::shared_ptr<BackgroundEvents> state_;
  explicit EventRange(std::shared_ptr<BackgroundEvents> state);
  friend class Controller;

public:
  struct Sentinel {};

  class Iterator {
    std::shared_ptr<const Collection> snapshot_;
    std::size_t index_ = 0;

  public:
    explicit Iterator(Collection snapshot)
        : snapshot_(std::make_shared<const Collection>(std::move(snapshot))) {}

    const Event& operator*() const { return snapshot_->at(index_); }

    Iterator& operator++() {
      ++index_;
      return *this;
    }

    bool operator!=(Sentinel) const { return index_ < snapshot_->size(); }
  };

  Collection snapshot() const;
  bool running() const;
  std::string error() const;
  void stop();

  Iterator begin() const { return Iterator(snapshot()); }

  Sentinel end() const { return {}; }
};

class Facade {
  Controller *controller_;

public:
  explicit Facade(Controller &controller);
  const SessionHandle &selected_session() const;
  SessionHandle attach(std::uint32_t pid);
  void prepare(const std::string &selector, const std::string &header,
               const std::string &output, const std::string &compdb = {},
               const std::string &translation_unit = {});
  void prepare(const std::string &selector, const std::string &header,
               const std::string &compdb, const std::string &translation_unit,
               const std::string &module, const std::string &output);
  void prepare(const std::string &selector,
               const std::vector<std::string> &headers,
               const std::string &output, const std::string &compdb = {},
               const std::string &translation_unit = {});
  void prepare(const std::string &selector,
               const std::vector<std::string> &headers,
               const std::string &compdb, const std::string &translation_unit,
               const std::string &module, const std::string &output);
  void prepare(const std::string &selector,
               std::initializer_list<std::string> headers,
               const std::string &output, const std::string &compdb = {},
               const std::string &translation_unit = {});
  void prepare(const std::string &selector,
               std::initializer_list<std::string> headers,
               const std::string &compdb, const std::string &translation_unit,
               const std::string &module, const std::string &output);
  void detach(const SessionHandle &session);
  std::uint64_t inject(const SessionHandle &session,
                       const std::string &selector,
                       const std::string &callback_file);
  std::uint64_t inject(const SessionHandle &session,
                       const std::string &selector,
                       const std::string &callback_file,
                       const std::string &prepared_directory,
                       const std::string &prepared_mode = "require");
  Collection collect(const SessionHandle &session, std::uint64_t count,
                     unsigned timeout_seconds = 30);
  EventRange events(const SessionHandle& session, std::uint64_t count,
                    unsigned timeout_seconds = 30);
  EventRange events_async(const SessionHandle& session, std::uint64_t count = 0,
                          unsigned timeout_seconds = 0);
  RecordResult record(const std::string &path, const RecordOptions &options = {});
  Collection replay(const std::string &path, const EventQuery &query = {}) const;
  Collection batch(const std::string &name) const;
  void print(const Event &event) const;
  void print(const Collection &collection) const;
};
} // namespace tracer_repl

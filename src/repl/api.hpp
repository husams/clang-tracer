#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace tracer_repl {
class Controller;
struct EventData;
using ScalarValue = std::variant<bool, std::int64_t, std::uint64_t, double>;

struct SessionHandle {
  std::string id;
  std::uint32_t pid = 0;
  std::uint64_t connection = 0;
};

class Event {
  std::shared_ptr<const EventData> data_;

  template <class T> static T scalar_as(const ScalarValue &value) {
    using Target = std::remove_cv_t<T>;
    static_assert(
        std::is_arithmetic_v<Target>,
        "event accessors require bool, an integer, or a floating type");
    if constexpr (std::is_same_v<Target, bool>) {
      if (const auto *v = std::get_if<bool>(&value))
        return *v;
    } else if constexpr (std::is_integral_v<Target>) {
      static_assert(sizeof(Target) <= sizeof(std::uint64_t),
                    "event integers support at most 64 bits");
      if (const auto *v = std::get_if<std::int64_t>(&value)) {
        if constexpr (std::is_signed_v<Target>) {
          if (*v < std::numeric_limits<Target>::lowest() ||
              *v > std::numeric_limits<Target>::max())
            throw std::out_of_range("event integer is outside requested range");
        } else {
          if (*v < 0 || static_cast<std::uint64_t>(*v) >
                            std::numeric_limits<Target>::max())
            throw std::out_of_range("event integer is outside requested range");
        }
        return static_cast<T>(*v);
      }
      if (const auto *v = std::get_if<std::uint64_t>(&value)) {
        if (*v > static_cast<std::uint64_t>(std::numeric_limits<Target>::max()))
          throw std::out_of_range("event integer is outside requested range");
        return static_cast<T>(*v);
      }
    } else if constexpr (std::is_floating_point_v<Target>) {
      if (const auto *v = std::get_if<double>(&value)) {
        if (std::isfinite(*v) && (static_cast<long double>(*v) >
                                      std::numeric_limits<Target>::max() ||
                                  static_cast<long double>(*v) <
                                      std::numeric_limits<Target>::lowest()))
          throw std::out_of_range("event float is outside requested range");
        return static_cast<T>(*v);
      }
    }
    throw std::invalid_argument(
        "event value type does not match requested C++ type");
  }

public:
  explicit Event(std::shared_ptr<const EventData> data);
  std::uint64_t sequence() const;
  std::uint64_t probe_id() const;
  std::uint64_t call_id() const;
  std::string phase() const;
  std::string function_name() const;
  // Presence includes VOID results; typed scalar access rejects void/objects.
  bool has_result() const;
  // Values own their scalar data and do not retain target addresses.
  ScalarValue argument_value(std::size_t index) const;
  ScalarValue result_value() const;

  template <class T> T argument_as(std::size_t index) const {
    return scalar_as<T>(argument_value(index));
  }

  template <class T> T result_as() const {
    return scalar_as<T>(result_value());
  }

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
  void print(const Collection &collection) const;
};
} // namespace tracer_repl

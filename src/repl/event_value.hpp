#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace tracer_repl {
using ScalarValue = std::variant<bool, std::int64_t, std::uint64_t, double>;

// Specialize for a local record with type_name and fields(), or specialize
// EventCodec<T>::decode for classes requiring a constructor/factory.
template <class T> struct EventRecord;
template <class T, class Enable = void> struct EventCodec;

template <class Class, class Member> struct EventMember {
  const char *name;
  Member Class::*pointer;
};
template <class Class, class Member>
constexpr EventMember<Class, Member> event_member(const char *name,
                                                  Member Class::*pointer) {
  return {name, pointer};
}

namespace detail {
template <class T> T scalar_as(const ScalarValue &value) {
  using Target = std::remove_cv_t<T>;
  static_assert(std::is_arithmetic_v<Target>,
                "requested type is not arithmetic");
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
      } else if (*v < 0 || static_cast<std::uint64_t>(*v) >
                               std::numeric_limits<Target>::max()) {
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
      if (std::isfinite(*v) &&
          (static_cast<long double>(*v) > std::numeric_limits<Target>::max() ||
           static_cast<long double>(*v) <
               std::numeric_limits<Target>::lowest()))
        throw std::out_of_range("event float is outside requested range");
      return static_cast<T>(*v);
    }
  }
  throw std::invalid_argument(
      "event value type does not match requested C++ type");
}
} // namespace detail

// All strings, nested fields and elements own their data. Copies remain usable
// after detach, target exit, recording replay, or clearing the collection.
class EventValue {
public:
  using Object = std::map<std::string, EventValue>;
  using Array = std::vector<EventValue>;
  using Data = std::variant<std::monostate, bool, std::int64_t, std::uint64_t,
                            double, std::string, Object, Array>;

private:
  Data data_;
  std::string type_;

public:
  EventValue() = default;
  EventValue(const EventValue &) = default;
  EventValue(EventValue &&) noexcept = default;
  EventValue &operator=(const EventValue &) = default;
  EventValue &operator=(EventValue &&) noexcept = default;
  ~EventValue();
  explicit EventValue(Data data, std::string cpp_type = {})
      : data_(std::move(data)), type_(std::move(cpp_type)) {}
  const std::string &cpp_type() const { return type_; }
  const Data &data() const { return data_; }
  bool is_null() const { return std::holds_alternative<std::monostate>(data_); }
  const EventValue &field(const std::string &name) const {
    const auto *object = std::get_if<Object>(&data_);
    if (!object)
      throw std::invalid_argument("event value is not a class/struct");
    const auto found = object->find(name);
    if (found == object->end())
      throw std::out_of_range("event field unavailable: " + name);
    return found->second;
  }
  const Object &fields() const {
    if (const auto *object = std::get_if<Object>(&data_))
      return *object;
    throw std::invalid_argument("event value is not a class/struct");
  }
  const Array &elements() const {
    if (const auto *array = std::get_if<Array>(&data_))
      return *array;
    throw std::invalid_argument("event value is not an array/container");
  }
  const EventValue &at(std::size_t index) const { return elements().at(index); }
  std::size_t size() const {
    if (const auto *array = std::get_if<Array>(&data_))
      return array->size();
    if (const auto *object = std::get_if<Object>(&data_))
      return object->size();
    if (const auto *text = std::get_if<std::string>(&data_))
      return text->size();
    throw std::invalid_argument("event scalar has no size");
  }
  template <class T> T as() const {
    static_assert(!std::is_reference_v<T> && !std::is_pointer_v<T>,
                  "event conversion returns owning values, not target "
                  "references/pointers");
    if constexpr (std::is_same_v<std::remove_cv_t<T>, EventValue>) {
      return *this;
    } else if constexpr (std::is_same_v<std::remove_cv_t<T>, std::string>) {
      if (const auto *text = std::get_if<std::string>(&data_))
        return *text;
      throw std::invalid_argument("event value is not a string");
    } else if constexpr (std::is_enum_v<T>) {
      return static_cast<T>(as<std::underlying_type_t<T>>());
    } else if constexpr (std::is_arithmetic_v<T>) {
      if (const auto *v = std::get_if<bool>(&data_))
        return detail::scalar_as<T>(ScalarValue{*v});
      if (const auto *v = std::get_if<std::int64_t>(&data_))
        return detail::scalar_as<T>(ScalarValue{*v});
      if (const auto *v = std::get_if<std::uint64_t>(&data_))
        return detail::scalar_as<T>(ScalarValue{*v});
      if (const auto *v = std::get_if<double>(&data_))
        return detail::scalar_as<T>(ScalarValue{*v});
      throw std::invalid_argument("event value is not a scalar");
    } else {
      return EventCodec<std::remove_cv_t<T>>::decode(*this);
    }
  }
};

template <class T, class Enable> struct EventCodec {
  static T decode(const EventValue &) {
    static_assert(!std::is_same_v<T, T>,
                  "define EventRecord<T>::type_name and fields(), or "
                  "EventCodec<T>::decode(const EventValue&)");
  }
};

template <class T>
struct EventCodec<T, std::void_t<decltype(EventRecord<T>::fields()),
                                 decltype(EventRecord<T>::type_name)>> {
  static T decode(const EventValue &value) {
    if (value.cpp_type() != EventRecord<T>::type_name)
      throw std::invalid_argument("event record type mismatch: " +
                                  value.cpp_type());
    T result{};
    std::apply(
        [&](const auto &...member) {
          ((result.*(member.pointer) =
                value.field(member.name)
                    .template as<std::remove_cv_t<std::remove_reference_t<
                        decltype(result.*(member.pointer))>>>()),
           ...);
        },
        EventRecord<T>::fields());
    return result;
  }
};

template <> struct EventCodec<std::monostate> {
  static std::monostate decode(const EventValue &value) {
    if (!value.fields().empty())
      throw std::invalid_argument("event monostate is not empty");
    return {};
  }
};

template <class... T> struct EventCodec<std::variant<T...>> {
  using Variant = std::variant<T...>;

  template <std::size_t I = 0>
  static Variant decode_at(std::size_t index, const EventValue &value) {
    if (index == I)
      return Variant(std::in_place_index<I>,
                     value.template as<std::variant_alternative_t<I, Variant>>());
    if constexpr (I + 1 < sizeof...(T))
      return decode_at<I + 1>(index, value);
    throw std::out_of_range("event variant alternative is out of range");
  }

  static Variant decode(const EventValue &value) {
    const auto index = value.field("index").template as<std::uint64_t>();
    if (index == std::variant_npos)
      throw std::invalid_argument("event variant was valueless by exception");
    if (index >= sizeof...(T))
      throw std::out_of_range("event variant alternative is out of range");
    return decode_at(index, value.field("alternative_" + std::to_string(index)));
  }
};

template <class T, class Allocator>
struct EventCodec<std::vector<T, Allocator>> {
  static std::vector<T, Allocator> decode(const EventValue &value) {
    std::vector<T, Allocator> result;
    result.reserve(value.elements().size());
    for (const auto &item : value.elements())
      result.push_back(item.template as<T>());
    return result;
  }
};
template <class T, std::size_t N> struct EventCodec<std::array<T, N>> {
  static std::array<T, N> decode(const EventValue &value) {
    if (value.elements().size() != N)
      throw std::out_of_range("event array extent mismatch");
    std::array<T, N> result{};
    for (std::size_t i = 0; i < N; ++i)
      result[i] = value.at(i).template as<T>();
    return result;
  }
};
template <class T> struct EventCodec<std::optional<T>> {
  static std::optional<T> decode(const EventValue &value) {
    if (value.is_null())
      return std::nullopt;
    return value.template as<T>();
  }
};

template <class T, class Allocator> struct EventCodec<std::list<T, Allocator>> {
  static std::list<T, Allocator> decode(const EventValue &value) {
    std::list<T, Allocator> result;
    for (const auto &item : value.elements())
      result.push_back(item.template as<T>());
    return result;
  }
};
template <class T, class Allocator>
struct EventCodec<std::deque<T, Allocator>> {
  static std::deque<T, Allocator> decode(const EventValue &value) {
    std::deque<T, Allocator> result;
    for (const auto &item : value.elements())
      result.push_back(item.template as<T>());
    return result;
  }
};
template <class K, class V, class Compare, class Allocator>
struct EventCodec<std::map<K, V, Compare, Allocator>> {
  static std::map<K, V, Compare, Allocator> decode(const EventValue &value) {
    std::map<K, V, Compare, Allocator> result;
    for (const auto &item : value.elements())
      if (!result
               .emplace(item.field("key").template as<K>(),
                        item.field("value").template as<V>())
               .second)
        throw std::invalid_argument("duplicate event map key");
    return result;
  }
};
template <class K, class V, class Hash, class Equal, class Allocator>
struct EventCodec<std::unordered_map<K, V, Hash, Equal, Allocator>> {
  static std::unordered_map<K, V, Hash, Equal, Allocator>
  decode(const EventValue &value) {
    std::unordered_map<K, V, Hash, Equal, Allocator> result;
    for (const auto &item : value.elements())
      if (!result
               .emplace(item.field("key").template as<K>(),
                        item.field("value").template as<V>())
               .second)
        throw std::invalid_argument("duplicate event map key");
    return result;
  }
};
template <class T, class Compare, class Allocator>
struct EventCodec<std::set<T, Compare, Allocator>> {
  static std::set<T, Compare, Allocator> decode(const EventValue &value) {
    std::set<T, Compare, Allocator> result;
    for (const auto &item : value.elements())
      if (!result.insert(item.template as<T>()).second)
        throw std::invalid_argument("duplicate event set member");
    return result;
  }
};
template <class T, class Hash, class Equal, class Allocator>
struct EventCodec<std::unordered_set<T, Hash, Equal, Allocator>> {
  static std::unordered_set<T, Hash, Equal, Allocator>
  decode(const EventValue &value) {
    std::unordered_set<T, Hash, Equal, Allocator> result;
    for (const auto &item : value.elements())
      if (!result.insert(item.template as<T>()).second)
        throw std::invalid_argument("duplicate event set member");
    return result;
  }
};
} // namespace tracer_repl

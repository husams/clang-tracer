#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bounded_proto.hpp"
#include "context_schema.hpp"
#include "tracer/compat.hpp"

namespace tracer::context_engine {

struct EncodeBudget {
  using Selector = CapturePathSelector;
  using PathStep = CapturePathStep;
  using Truncation = bool (*)(void*, ::tracer::compat::span<const PathStep>, std::uint64_t,
                              std::uint64_t) noexcept;
  std::size_t max_items = 100, items = 0, visits = 0, depth = 0, path_size = 0;
  CaptureLimits limits{};
  std::array<PathStep, 32> path{};
  Truncation truncation = nullptr;
  void *truncation_context = nullptr;
  bool enter() noexcept {
    return ++visits <= limits.visits && ++depth <= limits.depth;
  }
  void leave() noexcept {
    if (depth)
      --depth;
  }
  bool push(PathStep step) noexcept {
    if (path_size >= path.size())
      return false;
    path[path_size++] = step;
    return true;
  }
  void pop(std::size_t saved) noexcept { path_size = saved; }
  bool container(std::uint32_t field, std::size_t n,
                 std::size_t &selected) noexcept {
    if (max_items > 4096)
      return false;
    selected = std::min<std::size_t>({n, max_items, 4096});
    if (selected > limits.items - std::min(items, limits.items))
      return false;
    items += selected;
    ++visits;
    const auto saved = path_size;
    const bool already =
        path_size && path[path_size - 1].selector == Selector::field_number &&
        path[path_size - 1].number == field;
    if (!already && !push({Selector::field_number, field}))
      return false;
    if (n > selected &&
        (!truncation || !truncation(truncation_context,
                                    {path.data(), path_size}, n, selected))) {
      pop(saved);
      return false;
    }
    pop(saved);
    return items <= limits.items && visits <= limits.visits;
  }
};

template <class T> struct Codec;
// Encodes an ordinary State<T> message body into its own writer without adding
// an extra field-1 wrapper. Reusing the same budget across event slots keeps
// item, visit, depth, and truncation accounting cumulative; this function never
// resets counters.
template <class T>
bool encode_state_payload(BoundedProtoWriter &w, const T &value,
                          EncodeBudget &budget) noexcept {
  std::uint32_t field_number = 1;
  return Codec<T>::write_fields(w, value, budget, field_number);
}
template <class T> struct IsContainer : std::false_type {};
template <class T, std::size_t N>
struct IsContainer<std::array<T, N>> : std::true_type {};
template <class T, class A>
struct IsContainer<std::vector<T, A>> : std::true_type {};
template <class T, class A>
struct IsContainer<std::deque<T, A>> : std::true_type {};
template <class T, class A>
struct IsContainer<std::list<T, A>> : std::true_type {};
template <class T, class C, class A>
struct IsContainer<std::set<T, C, A>> : std::true_type {};
template <class T, class H, class E, class A>
struct IsContainer<std::unordered_set<T, H, E, A>> : std::true_type {};
template <class K, class V, class C, class A>
struct IsContainer<std::map<K, V, C, A>> : std::true_type {};
template <class K, class V, class H, class E, class A>
struct IsContainer<std::unordered_map<K, V, H, E, A>> : std::true_type {};
template <class T>
bool encode_value(BoundedProtoWriter &w, std::uint32_t field, const T &v,
                  EncodeBudget &b) noexcept {
  if (!b.enter())
    return false;
  const auto saved = b.path_size;
  if (!b.push({EncodeBudget::Selector::field_number, field})) {
    b.leave();
    return false;
  }
  const bool ok = Codec<T>::write(w, field, v, b);
  b.pop(saved);
  b.leave();
  return ok;
}

template <class T> struct Codec {
  static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                "unsupported scalar codec");
  static bool write(BoundedProtoWriter &w, std::uint32_t f, T v,
                    EncodeBudget &) noexcept {
    if constexpr (std::is_same_v<T, bool>)
      return w.boolean(f, v);
    else if constexpr (std::is_floating_point_v<T> && sizeof(T) == 4)
      return w.f32(f, v);
    else if constexpr (std::is_floating_point_v<T>)
      return w.f64(f, v);
    // Protobuf enums use int32 varints, not ZigZag's sint32 encoding.
    else if constexpr (std::is_enum_v<T>)
      return w.u64(f, static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
    else if constexpr (std::is_signed_v<T>)
      return w.s64(f, static_cast<std::int64_t>(v));
    else
      return w.u64(f, static_cast<std::uint64_t>(v));
  }
};
template <> struct Codec<std::string> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::string &v, EncodeBudget &) noexcept {
    return w.string(f, v);
  }
};
template <class T> struct Codec<std::optional<T>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::optional<T> &v, EncodeBudget &b) noexcept {
    if (!v)
      return true;
    if (!w.begin_message(f))
      return false;
    const bool ok = encode_value(w, 1, *v, b);
    return w.end_message() && ok;
  }
};

template <class C>
bool encode_sequence(BoundedProtoWriter &w, std::uint32_t f, const C &v,
                     EncodeBudget &b) noexcept {
  std::size_t n = 0;
  if (!b.container(f, v.size(), n))
    return false;
  auto i = v.begin();
  for (std::size_t k = 0; k < n; ++k, ++i) {
    const auto saved = b.path_size;
    if (!b.push({EncodeBudget::Selector::list_index, k}))
      return false;
    if (!b.enter()) {
      b.pop(saved);
      return false;
    }
    bool ok = false;
    if constexpr (IsContainer<typename C::value_type>::value) {
      ok = w.begin_message(f) &&
           Codec<typename C::value_type>::write(w, 1, *i, b) && w.end_message();
    } else
      ok = Codec<typename C::value_type>::write(w, f, *i, b);
    b.leave();
    b.pop(saved);
    if (!ok)
      return false;
  }
  return true;
}
template <class T, std::size_t N> struct Codec<std::array<T, N>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::array<T, N> &v, EncodeBudget &b) noexcept {
    return encode_sequence(w, f, v, b);
  }
};
template <class T, class A> struct Codec<std::vector<T, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::vector<T, A> &v, EncodeBudget &b) noexcept {
    return encode_sequence(w, f, v, b);
  }
};
template <class T, class A> struct Codec<std::deque<T, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::deque<T, A> &v, EncodeBudget &b) noexcept {
    return encode_sequence(w, f, v, b);
  }
};
template <class T, class A> struct Codec<std::list<T, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::list<T, A> &v, EncodeBudget &b) noexcept {
    return encode_sequence(w, f, v, b);
  }
};
template <class T, class C, class A> struct Codec<std::set<T, C, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::set<T, C, A> &v, EncodeBudget &b) noexcept {
    return encode_sequence(w, f, v, b);
  }
};
template <class T, class H, class E, class A>
struct Codec<std::unordered_set<T, H, E, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::unordered_set<T, H, E, A> &v,
                    EncodeBudget &b) noexcept {
    return encode_sequence(w, f, v, b);
  }
};

template <class K> EncodeBudget::PathStep map_selector(const K &k) noexcept {
  using T = std::remove_cv_t<K>;
  if constexpr (std::is_same_v<T, std::string>)
    return {EncodeBudget::Selector::string_key, 0, 0, false, k};
  else if constexpr (std::is_same_v<T, bool>)
    return {EncodeBudget::Selector::bool_key, 0, 0, k, {}};
  else if constexpr (std::is_enum_v<T> || std::is_integral_v<T>) {
    if constexpr (std::is_signed_v<T>)
      return {EncodeBudget::Selector::signed_key, 0,
              static_cast<std::int64_t>(k)};
    else
      return {EncodeBudget::Selector::unsigned_key,
              static_cast<std::uint64_t>(k)};
  } else
    return {};
}
template <class Map>
bool encode_map(BoundedProtoWriter &w, std::uint32_t f, const Map &v,
                EncodeBudget &b) noexcept {
  std::size_t n = 0;
  if (!b.container(f, v.size(), n))
    return false;
  auto i = v.begin();
  for (std::size_t k = 0; k < n; ++k, ++i) {
    if (!w.begin_message(f) || !encode_value(w, 1, i->first, b))
      return false;
    const auto saved = b.path_size;
    if (!b.push(map_selector(i->first)))
      return false;
    const bool ok = encode_value(w, 2, i->second, b);
    b.pop(saved);
    if (!ok || !w.end_message())
      return false;
  }
  return true;
}
template <class K, class V, class C, class A>
struct Codec<std::map<K, V, C, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::map<K, V, C, A> &v, EncodeBudget &b) noexcept {
    return encode_map(w, f, v, b);
  }
};
template <class K, class V, class H, class E, class A>
struct Codec<std::unordered_map<K, V, H, E, A>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::unordered_map<K, V, H, E, A> &v,
                    EncodeBudget &b) noexcept {
    return encode_map(w, f, v, b);
  }
};

} // namespace tracer::context_engine

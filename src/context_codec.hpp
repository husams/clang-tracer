#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
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
struct IsContainer<T[N]> : std::true_type {};
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
template <> struct Codec<std::string_view> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    std::string_view v, EncodeBudget &) noexcept {
    return w.string(f, v);
  }
};
template <class T> struct Codec<T *> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f, T *v,
                    EncodeBudget &) noexcept {
    return w.u64(f, reinterpret_cast<std::uintptr_t>(v));
  }
};
template <class A, class B> struct Codec<std::pair<A, B>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::pair<A, B> &v, EncodeBudget &b) noexcept {
    if (!w.begin_message(f))
      return false;
    const bool ok = encode_value(w, 1, v.first, b) && encode_value(w, 2, v.second, b);
    return w.end_message() && ok;
  }
};
template <class... T> struct Codec<std::tuple<T...>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const std::tuple<T...> &v, EncodeBudget &b) noexcept {
    if (!w.begin_message(f))
      return false;
    std::uint32_t field = 1;
    const bool ok = std::apply([&](const auto &...item) {
      return (encode_value(w, field++, item, b) && ...);
    }, v);
    return w.end_message() && ok;
  }
};
template <class T>
bool encode_present_value(BoundedProtoWriter &w, std::uint32_t field,
                          const T &value, EncodeBudget &budget) noexcept {
  if (!w.begin_message(field))
    return false;
  const bool ok = encode_value(w, 1, value, budget);
  return w.end_message() && ok;
}

// Field numbers match the std::any snapshot descriptor in context_schema.cpp.
template <> struct Codec<std::any> {
  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    const std::any &value, EncodeBudget &budget) noexcept {
    if (!w.begin_message(field))
      return false;
    const auto write_payload = [&] {
      if (!value.has_value())
        return w.string(1, "empty");
      if (const auto *integer = std::any_cast<int>(&value))
        return w.string(1, "int") && encode_present_value(w, 2, *integer, budget);
      if (const auto *flag = std::any_cast<bool>(&value))
        return w.string(1, "bool") && encode_present_value(w, 3, *flag, budget);
      if (const auto *text = std::any_cast<std::string>(&value))
        return w.string(1, "string") && encode_present_value(w, 4, *text, budget);
#if defined(__cpp_rtti)
      return w.string(1, "opaque") &&
             encode_present_value(w, 5, std::string_view(value.type().name()), budget);
#else
      return w.string(1, "opaque");
#endif
    };
    const bool ok = write_payload();
    return w.end_message() && ok;
  }
};

template <class T> struct Codec<std::optional<T>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    const std::optional<T> &value, EncodeBudget &budget) noexcept {
    return !value || encode_present_value(w, field, *value, budget);
  }
};

template <> struct Codec<std::monostate> {
  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    std::monostate, EncodeBudget &) noexcept {
    return w.begin_message(field) && w.end_message();
  }
};

template <class... T> struct Codec<std::variant<T...>> {
  using Variant = std::variant<T...>;

  template <std::size_t... I>
  static bool write_active(BoundedProtoWriter &w, const Variant &value,
                           EncodeBudget &budget, std::index_sequence<I...>) noexcept {
    return ((value.index() == I &&
             encode_present_value(w, I + 2, std::get<I>(value), budget)) || ...);
  }

  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    const Variant &value, EncodeBudget &budget) noexcept {
    if (!w.begin_message(field))
      return false;
    const bool ok = w.u64(1, value.index()) &&
        (value.valueless_by_exception() ||
         write_active(w, value, budget, std::index_sequence_for<T...>{}));
    return w.end_message() && ok;
  }
};

template <class T, class = void> struct CanSnapshotPointee : std::false_type {};
template <class T>
struct CanSnapshotPointee<T, std::void_t<decltype(sizeof(T))>>
    : std::bool_constant<std::is_object_v<T> && !std::is_array_v<T> &&
                         !std::is_volatile_v<T>> {};

template <class T, class Pointer>
bool encode_snapshot_pointer(BoundedProtoWriter &w, std::uint32_t field,
                             Pointer *pointer, bool owned,
                             EncodeBudget &budget) noexcept {
  if (!w.begin_message(field))
    return false;
  bool ok = w.u64(1, reinterpret_cast<std::uintptr_t>(pointer));
  if constexpr (CanSnapshotPointee<T>::value)
    if (ok && owned && pointer)
      ok = encode_present_value(w, 2, *pointer, budget);
  return w.end_message() && ok;
}

template <class T> struct Codec<std::unique_ptr<T>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    const std::unique_ptr<T> &value, EncodeBudget &budget) noexcept {
    return encode_snapshot_pointer<T>(w, field, value.get(), true, budget);
  }
};

template <class T> struct Codec<std::shared_ptr<T>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    const std::shared_ptr<T> &value, EncodeBudget &budget) noexcept {
    return encode_snapshot_pointer<T>(w, field, value.get(), value.use_count() != 0, budget);
  }
};

template <class T> struct Codec<std::weak_ptr<T>> {
  static bool write(BoundedProtoWriter &w, std::uint32_t field,
                    const std::weak_ptr<T> &value, EncodeBudget &) noexcept {
    return w.begin_message(field) && w.boolean(1, value.expired()) && w.end_message();
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
template <class T, std::size_t N> struct Codec<T[N]> {
  static bool write(BoundedProtoWriter &w, std::uint32_t f,
                    const T (&v)[N], EncodeBudget &b) noexcept {
    struct View {
      using value_type = T;
      const T *data;
      const T *begin() const { return data; }
      std::size_t size() const { return N; }
    };
    return encode_sequence(w, f, View{v}, b);
  }
};
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

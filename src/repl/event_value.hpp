#ifndef CLANG_TRACER_REPL_EVENT_VALUE_HPP
#define CLANG_TRACER_REPL_EVENT_VALUE_HPP

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
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
class EventValue;
template <class T> class Reconstructed;

namespace detail {
// True when EventCodec<T> decodes into Reconstructed<T> rather than T.
template <class T, class = void>
struct reconstructed_codec_impl : std::false_type {};
template <class T>
struct reconstructed_codec_impl<
    T, std::void_t<decltype(EventCodec<T>::decode(std::declval<const EventValue &>()))>>
    : std::is_same<decltype(EventCodec<T>::decode(std::declval<const EventValue &>())),
                   Reconstructed<T>> {};
template <class T, bool = std::is_class_v<T>>
struct reconstructed_codec : std::false_type {};
template <class T>
struct reconstructed_codec<T, true> : reconstructed_codec_impl<T> {};
} // namespace detail

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
  std::string descriptor_type_;
  std::string container_role_;

public:
  EventValue() = default;
  EventValue(const EventValue &) = default;
  EventValue(EventValue &&) noexcept = default;
  EventValue &operator=(const EventValue &) = default;
  EventValue &operator=(EventValue &&) noexcept = default;
  ~EventValue();
  explicit EventValue(Data data, std::string cpp_type = {},
                      std::string descriptor_type = {}, std::string container_role = {})
      : data_(std::move(data)), type_(std::move(cpp_type)),
        descriptor_type_(std::move(descriptor_type)), container_role_(std::move(container_role)) {}
  bool matches_record_type(std::string_view name) const;
  const std::string &cpp_type() const { return type_; }
  const std::string &descriptor_type() const { return descriptor_type_; }
  const Data &data() const { return data_; }
  const std::string &container_role() const { return container_role_; }
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
  template <class T, std::enable_if_t<!detail::reconstructed_codec<std::remove_cv_t<T>>::value, int> = 0>
  T as() const {
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
  template <class T, std::enable_if_t<detail::reconstructed_codec<std::remove_cv_t<T>>::value, int> = 0>
  Reconstructed<std::remove_cv_t<T>> as() const {
    return EventCodec<std::remove_cv_t<T>>::decode(*this);
  }
};

namespace detail {
template <class T> T *pointee_address(Reconstructed<T> &value) {
  return value.get();
}
template <class T> T *pointee_address(T &value) { return std::addressof(value); }
} // namespace detail

// Specialize with static Reconstructed<T> decode(const EventValue &) to
// rebuild a record that cannot be constructed locally. Generated mappings also
// provide fill(), which rebuilds the record in place over existing storage.
template <class T> struct ReconstructedRecord;

// A record rebuilt field by field for inspection. No constructor or destructor
// of T runs and its vptr is null, so read fields and call non-virtual members only.
template <class T> class Reconstructed {
public:
  Reconstructed() {
    constexpr std::size_t alignment =
        alignof(T) > sizeof(void *) ? alignof(T) : sizeof(void *);
    constexpr std::size_t size =
        (sizeof(T) + alignment - 1) / alignment * alignment;
    storage_ = std::aligned_alloc(alignment, size);
    if (!storage_)
      throw std::bad_alloc();
    for (std::size_t i = 0; i < size; ++i)
      static_cast<unsigned char *>(storage_)[i] = 0;
    owner_ = true;
  }
  Reconstructed(const Reconstructed &) = delete;
  Reconstructed &operator=(const Reconstructed &) = delete;
  Reconstructed &operator=(Reconstructed &&) = delete;
  Reconstructed(Reconstructed &&other) noexcept
      : storage_(std::exchange(other.storage_, nullptr)),
        owner_(std::exchange(other.owner_, false)),
        cleanup_(std::move(other.cleanup_)) {
    other.cleanup_.clear();
  }
  ~Reconstructed() {
    if (!owner_)
      return;
    for (auto cleanup = cleanup_.rbegin(); cleanup != cleanup_.rend(); ++cleanup)
      cleanup->first(cleanup->second);
    std::free(storage_);
  }

  T &operator*() const { return *get(); }
  T *operator->() const { return get(); }
  T *get() const { return static_cast<T *>(storage_); }

  // Move-construct one member from its decoded value.
  template <class C, class M>
  void decode(M C::*member, const EventValue &source) {
    using Value = std::remove_cv_t<M>;
    list().reserve(list().size() + 1);
    void *address = slot(member);
    ::new (address) Value(source.template as<Value>());
    if constexpr (!std::is_trivially_destructible_v<Value>) {
      void (*destroy)(void *) = [](void *object) {
        static_cast<Value *>(object)->~Value();
      };
      list().emplace_back(destroy, address);
    }
  }

  // Rebuild a member whose record is itself reconstructed, in place.
  template <class Nested, class C, class M>
  void fill_nested(M C::*member, const EventValue &source) {
    Reconstructed<Nested> part(static_cast<Nested *>(slot(member)), &list());
    ReconstructedRecord<Nested>::fill(part, source);
  }

  // Point a raw pointer member at a decoded pointee when `owner` has
  // `<name>_snapshot` whose record type is `type_name`; otherwise leave it null.
  template <class Pointee, class C, class M>
  bool follow(M C::*member, const EventValue &owner, const char *name,
              const char *type_name) {
    using Slot = std::remove_cv_t<M>;
    static_assert(std::is_pointer_v<Slot>, "only raw pointer members can follow");
    Pointee *pointee = hold<Pointee>(owner, name, type_name);
    if (!pointee)
      return false;
    ::new (slot(member)) Slot(pointee);
    return true;
  }

  // Like follow() for a reference member of type `Declared &` stored at
  // `offset`, which no member pointer can name.
  template <class Declared, class Pointee>
  bool follow_reference(std::size_t offset, const EventValue &owner,
                        const char *name, const char *type_name) {
    Pointee *pointee = hold<Pointee>(owner, name, type_name);
    if (!pointee)
      return false;
    Declared *reference = pointee;
    ::new (static_cast<unsigned char *>(storage_) + offset) Declared *(reference);
    return true;
  }

  // A char pointer member whose snapshot holds the string; it points at a copy
  // owned by this object. The member stays null when no string was captured.
  template <class C, class M>
  void follow_text(M C::*member, const EventValue &owner, const char *name) {
    const auto &fields = owner.fields();
    const auto found = fields.find(std::string(name) + "_snapshot");
    if (found == fields.end() || found->second.is_null())
      return;
    const std::string *text = hold_value<std::string>(found->second);
    ::new (slot(member)) std::remove_cv_t<M>(
        reinterpret_cast<std::remove_cv_t<M>>(const_cast<char *>(text->c_str())));
  }

  // A pointer member to an arithmetic or enum value whose snapshot holds one
  // copy of that value.
  template <class C, class M>
  void follow_scalar(M C::*member, const EventValue &owner, const char *name) {
    using Slot = std::remove_cv_t<M>;
    using Element = std::remove_cv_t<std::remove_pointer_t<Slot>>;
    const auto &fields = owner.fields();
    const auto found = fields.find(std::string(name) + "_snapshot");
    if (found == fields.end() || found->second.is_null())
      return;
    ::new (slot(member)) Slot(hold_value<Element>(found->second));
  }

  // A std::unique_ptr member to a reconstructed record. The pointee is held by
  // this object, so the member is a non-owning view that is never destroyed.
  template <class C, class P>
  void adopt(P C::*member, const EventValue &source) {
    using Element = typename P::element_type;
    const auto &fields = source.fields();
    const auto found = fields.find("value");
    if (found == fields.end() || found->second.is_null())
      return;
    ::new (slot(member)) P(hold_value<std::remove_cv_t<Element>>(found->second));
  }

private:
  template <class U> friend class Reconstructed;

  // A view over storage that its parent owns; cleanups join the parent's list.
  Reconstructed(T *storage,
                std::vector<std::pair<void (*)(void *), void *>> *shared)
      : storage_(storage), shared_(shared) {}

  std::vector<std::pair<void (*)(void *), void *>> &list() {
    return shared_ ? *shared_ : cleanup_;
  }

  template <class Pointee>
  Pointee *hold(const EventValue &owner, const char *name, const char *type_name) {
    const auto &fields = owner.fields();
    const auto found = fields.find(std::string(name) + "_snapshot");
    if (found == fields.end() || !found->second.matches_record_type(type_name))
      return nullptr;
    return hold_value<std::remove_cv_t<Pointee>>(found->second);
  }

  // Decode `snapshot` into a heap object that lives as long as this record.
  template <class Pointee> Pointee *hold_value(const EventValue &snapshot) {
    using Held = decltype(snapshot.template as<Pointee>());
    Held *holder = new Held(snapshot.template as<Pointee>());
    Pointee *pointee = nullptr;
    void (*destroy)(void *) = [](void *object) {
      delete static_cast<Held *>(object);
    };
    try {
      pointee = detail::pointee_address(*holder);
      list().reserve(list().size() + 1);
      list().emplace_back(destroy, holder);
    } catch (...) {
      delete holder;
      throw;
    }
    return pointee;
  }

  template <class C, class M> void *slot(M C::*member) const {
    auto &field = (**this).*member;
    return const_cast<void *>(
        static_cast<const volatile void *>(std::addressof(field)));
  }

  void *storage_ = nullptr;
  bool owner_ = false;
  std::vector<std::pair<void (*)(void *), void *>> *shared_ = nullptr;
  std::vector<std::pair<void (*)(void *), void *>> cleanup_;
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
    if (!value.matches_record_type(EventRecord<T>::type_name))
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

namespace detail {
template <class T, class = void>
struct has_reconstructed_record : std::false_type {};
template <class T>
struct has_reconstructed_record<
    T, std::void_t<decltype(ReconstructedRecord<T>::decode(
           std::declval<const EventValue &>()))>> : std::true_type {};
} // namespace detail

template <class T>
struct EventCodec<T, std::enable_if_t<detail::has_reconstructed_record<T>::value>> {
  static Reconstructed<T> decode(const EventValue &value) {
    return ReconstructedRecord<T>::decode(value);
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

// Smart pointers carry their address and, when the pointee could be read, an
// inline copy of it. A copy cannot own the live target object, so a pointer
// without a captured pointee decodes as empty.
template <class T> struct EventCodec<std::unique_ptr<T, std::default_delete<T>>> {
  static std::unique_ptr<T> decode(const EventValue &value) {
    static_assert(!detail::reconstructed_codec<std::remove_cv_t<T>>::value,
                  "a reconstructed record cannot be owned by a decoded unique_ptr");
    const auto &fields = value.fields();
    const auto found = fields.find("value");
    if (found == fields.end() || found->second.is_null())
      return nullptr;
    return std::unique_ptr<T>(new T(found->second.template as<T>()));
  }
};
template <class T> struct EventCodec<std::shared_ptr<T>> {
  static std::shared_ptr<T> decode(const EventValue &value) {
    const auto &fields = value.fields();
    const auto found = fields.find("value");
    if (found == fields.end() || found->second.is_null())
      return nullptr;
    if constexpr (detail::reconstructed_codec<std::remove_cv_t<T>>::value) {
      auto holder = std::make_shared<Reconstructed<std::remove_cv_t<T>>>(
          found->second.template as<std::remove_cv_t<T>>());
      return std::shared_ptr<T>(holder, holder->get());
    } else {
      return std::shared_ptr<T>(new T(found->second.template as<T>()));
    }
  }
};
template <class T> struct EventCodec<std::weak_ptr<T>> {
  static std::weak_ptr<T> decode(const EventValue &) { return {}; }
};
} // namespace tracer_repl

#endif // CLANG_TRACER_REPL_EVENT_VALUE_HPP

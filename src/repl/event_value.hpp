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

// A captured smart pointer is a record with the fields address, value (the
// copy of the object it owns, when there is one), suppressed and omitted.
// omitted says why an owned object has no copy: "alias" (the same object was
// captured earlier in the snapshot, through another pointer), "cycle" (the
// object is an owner of this pointer) or the capture limit that left it out
// ("depth", "bytes", "elements", "string").

// Spans one typed conversion: the objects its shared pointers rebuild are kept
// until the outermost conversion returns, so an alias can share them.
class PointeeScope {
public:
  PointeeScope() noexcept;
  ~PointeeScope();
  PointeeScope(const PointeeScope &) = delete;
  PointeeScope &operator=(const PointeeScope &) = delete;
};
// The omitted field of a captured smart pointer; empty when it has none.
std::string omitted_pointee(const EventValue &pointer);
// Throws, naming the reason, when the pointer owns an object whose copy was
// left out: a typed value built without it would not be the captured graph.
void require_captured_pointee(const EventValue &pointer);
// The object rebuilt for the pointer an alias refers to, or null.
std::shared_ptr<const void> rebuilt_pointee(const EventValue &pointer);
void keep_rebuilt_pointee(const EventValue &pointer, std::shared_ptr<const void> object);
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

template <class T> struct is_int128 : std::false_type {};
#if defined(__SIZEOF_INT128__)
template <> struct is_int128<__int128> : std::true_type {};
template <> struct is_int128<unsigned __int128> : std::true_type {};
#endif

// The value of a captured long double: `bytes` holds it in the target's `format`, least
// significant byte first. It is exact when the local long double has that format, and the
// nearest local value otherwise.
inline long double decode_long_double(std::string_view format, std::string_view bytes) {
  const auto bits = [&](std::size_t first, std::size_t count) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < count; ++i)
      value |= std::uint64_t(static_cast<unsigned char>(bytes[first + i])) << (8 * i);
    return value;
  };
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  constexpr bool little_endian = true;
#else
  constexpr bool little_endian = false;
#endif
  const auto same_format = [&](int digits) {
    return little_endian && std::numeric_limits<long double>::digits == digits &&
           bytes.size() <= sizeof(long double);
  };
  const auto exact = [&] {
    long double value = 0;
    __builtin_memcpy(&value, bytes.data(), bytes.size());
    return value;
  };
  const auto special = [](bool nan) {
    return nan ? std::numeric_limits<long double>::quiet_NaN()
               : std::numeric_limits<long double>::infinity();
  };
  long double magnitude = 0;
  bool negative = false;
  if (format == "x87-extended-80" && bytes.size() == 10) {
    if (same_format(64))
      return exact();
    const auto significand = bits(0, 8);
    const auto exponent = static_cast<int>(bits(8, 2) & 0x7fff);
    negative = (bits(8, 2) & 0x8000) != 0;
    magnitude = exponent == 0x7fff
                    ? special((significand << 1) != 0)
                    : std::ldexp(static_cast<long double>(significand),
                                 (exponent ? exponent : 1) - 16383 - 63);
  } else if (format == "ieee-binary128" && bytes.size() == 16) {
    if (same_format(113))
      return exact();
    const auto low = bits(0, 8), high = bits(8, 8);
    const auto exponent = static_cast<int>((high >> 48) & 0x7fff);
    const auto fraction = high & 0xffffffffffffULL;
    negative = (high >> 63) != 0;
    if (exponent == 0x7fff) {
      magnitude = special(fraction != 0 || low != 0);
    } else {
      auto significand = std::ldexp(static_cast<long double>(fraction), 64) +
                         static_cast<long double>(low);
      if (exponent)
        significand += std::ldexp(1.0L, 112);
      magnitude = std::ldexp(significand, (exponent ? exponent : 1) - 16383 - 112);
    }
  } else if (format == "ieee-binary64" && bytes.size() == 8) {
    const auto raw = bits(0, 8);
    double value = 0;
    __builtin_memcpy(&value, &raw, sizeof(value));
    return value;
  } else {
    throw std::invalid_argument("event long double has an unsupported format: " +
                                std::string(format));
  }
  return negative ? -magnitude : magnitude;
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
  // Members of this record that capture left out when the probe was installed.
  std::map<std::string, std::string> unavailable_;

public:
  EventValue() = default;
  EventValue(const EventValue &) = default;
  EventValue(EventValue &&) noexcept = default;
  EventValue &operator=(const EventValue &) = default;
  EventValue &operator=(EventValue &&) noexcept = default;
  ~EventValue();
  explicit EventValue(Data data, std::string cpp_type = {},
                      std::string descriptor_type = {}, std::string container_role = {},
                      std::map<std::string, std::string> unavailable = {})
      : data_(std::move(data)), type_(std::move(cpp_type)),
        descriptor_type_(std::move(descriptor_type)), container_role_(std::move(container_role)),
        unavailable_(std::move(unavailable)) {}
  bool matches_record_type(std::string_view name) const;
  const std::string &cpp_type() const { return type_; }
  const std::string &descriptor_type() const { return descriptor_type_; }
  const Data &data() const { return data_; }
  const std::string &container_role() const { return container_role_; }
  // Member name -> why capture left it out; empty for a record captured whole.
  const std::map<std::string, std::string> &unavailable() const { return unavailable_; }
  // True when this record, or one nested in it, was captured without a member.
  bool is_partial() const {
    if (!unavailable_.empty())
      return true;
    if (const auto *object = std::get_if<Object>(&data_)) {
      for (const auto &item : *object)
        if (item.second.is_partial())
          return true;
    } else if (const auto *array = std::get_if<Array>(&data_)) {
      for (const auto &item : *array)
        if (item.is_partial())
          return true;
    }
    return false;
  }
  bool is_null() const { return std::holds_alternative<std::monostate>(data_); }
  const EventValue &field(const std::string &name) const {
    const auto *object = std::get_if<Object>(&data_);
    if (!object)
      throw std::invalid_argument("event value is not a class/struct");
    const auto found = object->find(name);
    if (found == object->end()) {
      const auto left_out = unavailable_.find(name);
      if (left_out != unavailable_.end())
        throw std::out_of_range("event field was not captured: " + name + ": " +
                                left_out->second);
      throw std::out_of_range("event field unavailable: " + name);
    }
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
    } else if constexpr (detail::is_int128<std::remove_cv_t<T>>::value) {
      return this->template wide_integer<std::remove_cv_t<T>>();
    } else if constexpr (std::is_arithmetic_v<T>) {
      if constexpr (std::is_floating_point_v<T>) {
        // A captured long double. A narrower type is an explicit request for the nearest
        // value it has.
        if (std::holds_alternative<Object>(data_)) {
          const auto value = long_double();
          if constexpr (!std::is_same_v<std::remove_cv_t<T>, long double>)
            if (std::isfinite(value) && (value > std::numeric_limits<T>::max() ||
                                         value < std::numeric_limits<T>::lowest()))
              throw std::out_of_range("event float is outside requested range");
          return static_cast<T>(value);
        }
      }
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
      const detail::PointeeScope scope;
      return EventCodec<std::remove_cv_t<T>>::decode(*this);
    }
  }
  template <class T, std::enable_if_t<detail::reconstructed_codec<std::remove_cv_t<T>>::value, int> = 0>
  Reconstructed<std::remove_cv_t<T>> as() const {
    const detail::PointeeScope scope;
    return EventCodec<std::remove_cv_t<T>>::decode(*this);
  }

private:
  long double long_double() const {
    const auto &object = std::get<Object>(data_);
    const auto format = object.find("format"), bytes = object.find("bytes");
    const auto *format_text =
        format == object.end() ? nullptr : std::get_if<std::string>(&format->second.data_);
    const auto *value_bytes =
        bytes == object.end() ? nullptr : std::get_if<std::string>(&bytes->second.data_);
    if (!format_text || !value_bytes)
      throw std::invalid_argument("event value is not a scalar");
    return detail::decode_long_double(*format_text, *value_bytes);
  }

#if defined(__SIZEOF_INT128__)
  // A 128-bit integer is captured as two 64-bit halves: value = high * 2^64 + low, and the
  // high half of a signed one carries the sign. A 64-bit event integer widens.
  template <class T> T wide_integer() const {
    using Unsigned = unsigned __int128;
    constexpr bool wants_signed = std::is_same_v<T, __int128>;
    const auto out_of_range = [] {
      return std::out_of_range("event integer is outside requested range");
    };
    if (const auto *v = std::get_if<std::int64_t>(&data_)) {
      if (!wants_signed && *v < 0)
        throw out_of_range();
      return static_cast<T>(*v);
    }
    if (const auto *v = std::get_if<std::uint64_t>(&data_))
      return static_cast<T>(*v);
    const auto *object = std::get_if<Object>(&data_);
    if (!object)
      throw std::invalid_argument("event value is not an integer");
    const auto low = object->find("low"), high = object->find("high");
    const auto *low_bits =
        low == object->end() ? nullptr : std::get_if<std::uint64_t>(&low->second.data_);
    if (!low_bits || high == object->end())
      throw std::invalid_argument("event value is not an integer");
    if (const auto *signed_high = std::get_if<std::int64_t>(&high->second.data_)) {
      if (!wants_signed && *signed_high < 0)
        throw out_of_range();
      return static_cast<T>((static_cast<Unsigned>(static_cast<std::uint64_t>(*signed_high)) << 64) |
                            *low_bits);
    }
    const auto *unsigned_high = std::get_if<std::uint64_t>(&high->second.data_);
    if (!unsigned_high)
      throw std::invalid_argument("event value is not an integer");
    if (wants_signed && (*unsigned_high >> 63) != 0)
      throw out_of_range();
    return static_cast<T>((static_cast<Unsigned>(*unsigned_high) << 64) | *low_bits);
  }
#endif
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
    if (found == fields.end() || found->second.is_null()) {
      detail::require_captured_pointee(source);
      return;
    }
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

// The explicit null marker a std::nullptr_t is captured as.
template <> struct EventCodec<std::nullptr_t> {
  static std::nullptr_t decode(const EventValue &value) {
    if (!value.field("null").template as<bool>())
      throw std::invalid_argument("event null marker is not set");
    return nullptr;
  }
};

// A pointer to a data member is the offset of the member, which is also its local value
// when both sides use the Itanium C++ ABI and lay the class out alike. A pointer to a
// member function names code of the target: only a null one has a local value.
template <class C, class M> struct EventCodec<M C::*> {
  static M C::*decode(const EventValue &value) {
    if (value.field("null").template as<bool>())
      return nullptr;
    if constexpr (std::is_member_function_pointer_v<M C::*>) {
      throw std::invalid_argument(
          "a member function pointer of the target has no local value");
    } else {
      if (value.field("abi").template as<std::string>() != "itanium" ||
          sizeof(M C::*) != sizeof(std::ptrdiff_t))
        throw std::invalid_argument("event member pointer uses another C++ ABI");
      const auto offset =
          static_cast<std::ptrdiff_t>(value.field("offset").template as<std::int64_t>());
      M C::*result = nullptr;
      __builtin_memcpy(&result, &offset, sizeof(offset));
      return result;
    }
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
    // Root snapshots retain the optional's C++ type while exposing its value
    // directly. Decode with the contained type, keeping the original snapshot
    // and the record codec's identity check intact. Nested optional fields
    // already carry their contained value's type.
    std::string_view type = value.cpp_type();
    if (type.substr(0, 2) == "::")
      type.remove_prefix(2);
    constexpr std::string_view prefix = "std::optional<";
    if (type.substr(0, prefix.size()) == prefix && type.back() == '>') {
      type.remove_prefix(prefix.size());
      type.remove_suffix(1);
      while (!type.empty() && type.front() == ' ')
        type.remove_prefix(1);
      while (!type.empty() && type.back() == ' ')
        type.remove_suffix(1);
      return EventValue{value.data(), std::string(type), value.descriptor_type(),
                        value.container_role(), value.unavailable()}.template as<T>();
    }
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
// that owned nothing, or whose pointee could not be read, decodes as empty.
// A pointer whose pointee the capture left out does not: an alias of an
// object a std::shared_ptr already rebuilt shares that object, and every other
// case (an ownership cycle, a capture limit) is an error, because the typed
// value would not be the graph that was captured.
template <class T> struct EventCodec<std::unique_ptr<T, std::default_delete<T>>> {
  static std::unique_ptr<T> decode(const EventValue &value) {
    static_assert(!detail::reconstructed_codec<std::remove_cv_t<T>>::value,
                  "a reconstructed record cannot be owned by a decoded unique_ptr");
    const auto &fields = value.fields();
    const auto found = fields.find("value");
    if (found == fields.end() || found->second.is_null()) {
      detail::require_captured_pointee(value);
      return nullptr;
    }
    return std::unique_ptr<T>(new T(found->second.template as<T>()));
  }
};
template <class T> struct EventCodec<std::shared_ptr<T>> {
  static std::shared_ptr<T> decode(const EventValue &value) {
    const detail::PointeeScope scope;
    const auto &fields = value.fields();
    const auto found = fields.find("value");
    if (found == fields.end() || found->second.is_null()) {
      if (auto shared = detail::rebuilt_pointee(value))
        return std::const_pointer_cast<T>(std::static_pointer_cast<const T>(shared));
      detail::require_captured_pointee(value);
      return nullptr;
    }
    std::shared_ptr<T> result;
    if constexpr (detail::reconstructed_codec<std::remove_cv_t<T>>::value) {
      auto holder = std::make_shared<Reconstructed<std::remove_cv_t<T>>>(
          found->second.template as<std::remove_cv_t<T>>());
      result = std::shared_ptr<T>(holder, holder->get());
    } else {
      result = std::shared_ptr<T>(new T(found->second.template as<T>()));
    }
    detail::keep_rebuilt_pointee(value, result);
    return result;
  }
};
template <class T> struct EventCodec<std::weak_ptr<T>> {
  static std::weak_ptr<T> decode(const EventValue &) { return {}; }
};
} // namespace tracer_repl

#endif // CLANG_TRACER_REPL_EVENT_VALUE_HPP

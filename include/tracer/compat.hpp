#ifndef TRACER_COMPAT_HPP
#define TRACER_COMPAT_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(__has_include)
#if __has_include(<span>)
#include <span>
#endif
#endif

#if defined(__cpp_lib_span) && !defined(TRACER_FORCE_COMPAT_LIBRARY)
#define TRACER_COMPAT_HAS_STD_SPAN 1
#else
#define TRACER_COMPAT_HAS_STD_SPAN 0
#endif

namespace tracer {
namespace compat {

template <class T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

#if TRACER_COMPAT_HAS_STD_SPAN

using std::as_bytes;
using std::as_writable_bytes;
using std::span;

#else

static const std::size_t dynamic_extent = (std::numeric_limits<std::size_t>::max)();

template <class T>
class span {
public:
  typedef T element_type;
  typedef typename std::remove_cv<T>::type value_type;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;
  typedef T* pointer;
  typedef T& reference;
  typedef pointer iterator;
  typedef const T* const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;

  static const size_type extent = dynamic_extent;

  span() noexcept
      : data_(0)
      , size_(0) {}

  span(pointer data, size_type count) noexcept
      : data_(data)
      , size_(count) {}

  template <std::size_t N>
  span(element_type (&array)[N]) noexcept
      : data_(array)
      , size_(N) {}

  template <class U, std::size_t N,
            typename std::enable_if<std::is_convertible<U (*)[], T (*)[]>::value, int>::type = 0>
  span(std::array<U, N>& array) noexcept
      : data_(array.data())
      , size_(N) {}

  template <
      class U, std::size_t N,
      typename std::enable_if<std::is_convertible<const U (*)[], T (*)[]>::value, int>::type = 0>
  span(const std::array<U, N>& array) noexcept
      : data_(array.data())
      , size_(N) {}

  template <
      class Container, class Data = decltype(std::declval<Container&>().data()),
      class Size = decltype(std::declval<Container&>().size()),
      typename std::enable_if<
          std::is_pointer<Data>::value &&
              std::is_convertible<typename std::remove_pointer<Data>::type (*)[], T (*)[]>::value &&
              std::is_convertible<Size, size_type>::value &&
              !std::is_same<
                  typename std::remove_cv<typename std::remove_reference<Container>::type>::type,
                  span>::value,
          int>::type = 0>
  span(Container& container) noexcept
      : data_(container.data())
      , size_(static_cast<size_type>(container.size())) {}

  template <class U,
            typename std::enable_if<std::is_convertible<U (*)[], T (*)[]>::value, int>::type = 0>
  span(const span<U>& other) noexcept
      : data_(other.data())
      , size_(other.size()) {}

  pointer data() const noexcept { return data_; }

  size_type size() const noexcept { return size_; }

  size_type size_bytes() const noexcept { return size_ * sizeof(element_type); }

  bool empty() const noexcept { return size_ == 0; }

  reference operator[](size_type index) const noexcept { return data_[index]; }

  reference front() const noexcept { return data_[0]; }

  reference back() const noexcept { return data_[size_ - 1]; }

  iterator begin() const noexcept { return data_; }

  iterator end() const noexcept { return size_ ? data_ + size_ : data_; }

  reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }

  reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

  span first(size_type count) const noexcept { return span(data_, count); }

  span last(size_type count) const noexcept { return span(data_ + size_ - count, count); }

  span subspan(size_type offset, size_type count = dynamic_extent) const noexcept {
    const size_type remaining = size_ - offset;
    return span(offset ? data_ + offset : data_, count == dynamic_extent ? remaining : count);
  }

private:
  pointer data_;
  size_type size_;
};

template <class T, std::size_t N>
span(T (&)[N]) -> span<T>;
template <class T, std::size_t N>
span(std::array<T, N>&) -> span<T>;
template <class T, std::size_t N>
span(const std::array<T, N>&) -> span<const T>;
template <class Container>
span(Container&)
    -> span<typename std::remove_pointer<decltype(std::declval<Container&>().data())>::type>;
template <class T>
span(T*, std::size_t) -> span<T>;

template <class T>
span<const std::byte> as_bytes(span<T> view) noexcept {
  return span<const std::byte>(reinterpret_cast<const std::byte*>(view.data()), view.size_bytes());
}

template <class T, typename std::enable_if<!std::is_const<T>::value, int>::type = 0>
span<std::byte> as_writable_bytes(span<T> view) noexcept {
  return span<std::byte>(reinterpret_cast<std::byte*>(view.data()), view.size_bytes());
}

#endif // TRACER_COMPAT_HAS_STD_SPAN

namespace detail {
template <class...>
struct make_void {
  typedef void type;
};
template <class... T>
using void_t = typename make_void<T...>::type;

template <class Container, class Key, class = void>
struct has_contains : std::false_type {};

template <class Container, class Key>
struct has_contains<
    Container, Key,
    void_t<decltype(std::declval<const Container&>().contains(std::declval<const Key&>()))>>
    : std::true_type {};

template <class Container, class Key>
auto contains_impl(const Container& container, const Key& key, std::true_type)
    -> decltype(container.contains(key), bool()) {
  return container.contains(key);
}

template <class Container, class Key>
auto contains_impl(const Container& container, const Key& key, std::false_type)
    -> decltype(container.find(key) != container.end(), bool()) {
  return container.find(key) != container.end();
}

template <class T, class = void>
struct has_string_data_size : std::false_type {};

template <class T>
struct has_string_data_size<
    T, void_t<decltype(std::declval<const T&>().data()), decltype(std::declval<const T&>().size())>>
    : std::integral_constant<
          bool,
          std::is_convertible<decltype(std::declval<const T&>().data()), const char*>::value &&
              std::is_convertible<decltype(std::declval<const T&>().size()), std::size_t>::value> {
};

template <class T, class = void>
struct is_string_view_constructible : std::false_type {};

template <class T>
struct is_string_view_constructible<T, void_t<decltype(std::string_view(std::declval<const T&>()))>>
    : std::true_type {};

template <class T>
auto as_string_view(const T& value, int) -> decltype(std::string_view(value)) {
  return std::string_view(value);
}

template <class T>
auto as_string_view(const T& value, long) ->
    typename std::enable_if<has_string_data_size<T>::value, std::string_view>::type {
  return std::string_view(value.data(), static_cast<std::size_t>(value.size()));
}

template <class T>
struct is_string_like : std::integral_constant<bool, is_string_view_constructible<T>::value ||
                                                         has_string_data_size<T>::value> {};
} // namespace detail

template <class Container, class Key,
          typename std::enable_if<!detail::is_string_like<Container>::value ||
                                      !detail::is_string_like<Key>::value,
                                  int>::type = 0>
auto contains(const Container& container, const Key& key)
    -> decltype(detail::contains_impl(container, key,
                                      typename detail::has_contains<Container, Key>::type())) {
  return detail::contains_impl(container, key,
                               typename detail::has_contains<Container, Key>::type());
}

template <class Haystack, class Needle,
          typename std::enable_if<detail::is_string_like<Haystack>::value &&
                                      detail::is_string_like<Needle>::value,
                                  int>::type = 0>
bool contains(const Haystack& haystack, const Needle& needle) {
  return detail::as_string_view(haystack, 0).find(detail::as_string_view(needle, 0)) !=
         std::string_view::npos;
}

template <class Haystack, class Needle,
          typename std::enable_if<detail::is_string_like<Haystack>::value &&
                                      detail::is_string_like<Needle>::value,
                                  int>::type = 0>
bool starts_with(const Haystack& haystack, const Needle& needle) {
  const std::string_view text = detail::as_string_view(haystack, 0);
  const std::string_view prefix = detail::as_string_view(needle, 0);
  return prefix.size() <= text.size() && text.compare(0, prefix.size(), prefix) == 0;
}

template <class Haystack, class Needle,
          typename std::enable_if<detail::is_string_like<Haystack>::value &&
                                      detail::is_string_like<Needle>::value,
                                  int>::type = 0>
bool ends_with(const Haystack& haystack, const Needle& needle) {
  const std::string_view text = detail::as_string_view(haystack, 0);
  const std::string_view suffix = detail::as_string_view(needle, 0);
  return suffix.size() <= text.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

template <class To, class From>
To bit_cast(const From& source) noexcept {
  static_assert(sizeof(To) == sizeof(From), "bit_cast requires equal-sized types");
  static_assert(std::is_trivially_copyable<To>::value,
                "bit_cast destination must be trivially copyable");
  static_assert(std::is_trivially_copyable<From>::value,
                "bit_cast source must be trivially copyable");
#if defined(__has_builtin)
#if __has_builtin(__builtin_bit_cast)
  return __builtin_bit_cast(To, source);
#else
  To result;
  std::memcpy(&result, &source, sizeof(To));
  return result;
#endif
#elif defined(__GNUC__) && __GNUC__ >= 11
  return __builtin_bit_cast(To, source);
#else
  To result;
  std::memcpy(&result, &source, sizeof(To));
  return result;
#endif
}

template <class UInt,
          typename std::enable_if<std::is_integral<UInt>::value && std::is_unsigned<UInt>::value,
                                  int>::type = 0>
unsigned int countr_zero(UInt value) noexcept {
  const unsigned int width = std::numeric_limits<UInt>::digits;
  if (value == 0) {
    return width;
  }
  if constexpr (sizeof(UInt) <= sizeof(unsigned int)) {
    return __builtin_ctz(static_cast<unsigned int>(value));
  } else if constexpr (sizeof(UInt) <= sizeof(unsigned long)) {
    return __builtin_ctzl(static_cast<unsigned long>(value));
  } else {
    return __builtin_ctzll(static_cast<unsigned long long>(value));
  }
}

template <class UInt>
constexpr bool has_single_bit(UInt value) noexcept {
  static_assert(std::is_unsigned<UInt>::value, "bit predicates require unsigned values");
  return value != 0 && (value & (value - 1)) == 0;
}

template <class T, class... Args>
T* construct_at(T* location, Args&&... args) {
  return ::new (static_cast<void*>(location)) T(std::forward<Args>(args)...);
}

template <class Sequence, class Predicate>
typename Sequence::size_type erase_if(Sequence& values, Predicate predicate) {
  const auto before = values.size();
  values.erase(std::remove_if(values.begin(), values.end(), predicate), values.end());
  return before - values.size();
}

} // namespace compat
} // namespace tracer

#undef TRACER_COMPAT_HAS_STD_SPAN

#endif // TRACER_COMPAT_HPP

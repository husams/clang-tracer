#ifndef CLANG_TRACER_REPL_EVENT_RANGES_HPP
#define CLANG_TRACER_REPL_EVENT_RANGES_HPP
#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace tracer_repl {
namespace range_detail {
struct Sentinel {};

template <class Range> class TakeView {
  Range range_;
  std::size_t count_;
  using Current = decltype(std::declval<const Range &>().begin());
  using End = decltype(std::declval<const Range &>().end());

public:
  TakeView(Range range, std::size_t count)
      : range_(std::move(range)), count_(count) {}
  class Iterator {
    std::optional<Current> current_;
    std::optional<End> end_;
    std::size_t left_ = 0;

  public:
    Iterator() = default;
    Iterator(Current current, End end, std::size_t count)
        : current_(std::move(current)), end_(std::move(end)), left_(count) {}
    decltype(auto) operator*() const { return **current_; }
    Iterator &operator++() {
      if (--left_)
        ++*current_;
      return *this;
    }
    bool operator!=(Sentinel) const { return left_ && *current_ != *end_; }
  };
  Iterator begin() const {
    if (!count_)
      return {};
    return {range_.begin(), range_.end(), count_};
  }
  Sentinel end() const { return {}; }
};

template <class Range, class Predicate, bool Until> class PredicateView {
  Range range_;
  Predicate predicate_;
  using Current = decltype(std::declval<const Range &>().begin());
  using End = decltype(std::declval<const Range &>().end());

public:
  PredicateView(Range range, Predicate predicate)
      : range_(std::move(range)), predicate_(std::move(predicate)) {}
  class Iterator {
    Current current_;
    End end_;
    Predicate predicate_;
    bool stopped_ = false;
    void seek() {
      while (current_ != end_) {
        if (std::invoke(predicate_, *current_))
          return;
        if (Until) {
          stopped_ = true;
          return;
        }
        ++current_;
      }
    }

  public:
    Iterator(Current current, End end, Predicate predicate)
        : current_(std::move(current)), end_(std::move(end)),
          predicate_(std::move(predicate)) {
      seek();
    }
    decltype(auto) operator*() const { return *current_; }
    Iterator &operator++() {
      ++current_;
      seek();
      return *this;
    }
    bool operator!=(Sentinel) const { return !stopped_ && current_ != end_; }
  };
  Iterator begin() const { return {range_.begin(), range_.end(), predicate_}; }
  Sentinel end() const { return {}; }
};

template <class Range> class DropView {
  Range range_;
  std::size_t count_;

public:
  DropView(Range range, std::size_t count)
      : range_(std::move(range)), count_(count) {}
  auto begin() const {
    auto current = range_.begin();
    const auto end = range_.end();
    for (std::size_t n = 0; n < count_ && current != end; ++n)
      ++current;
    return current;
  }
  auto end() const { return range_.end(); }
};

inline std::size_t count(std::ptrdiff_t value) {
  if (value < 0)
    throw std::invalid_argument("range count must not be negative");
  return static_cast<std::size_t>(value);
}

template <class Range, class Value> class TransformView {
  Range range_;
  using Source = std::decay_t<decltype(*std::declval<const Range &>().begin())>;
  using Function = std::function<Value(const Source &)>;
  Function function_;
  using Current = decltype(std::declval<const Range &>().begin());
  using End = decltype(std::declval<const Range &>().end());

public:
  TransformView(Range range, Function function)
      : range_(std::move(range)), function_(std::move(function)) {}
  class Iterator {
    Current current_;
    End end_;
    Function function_;

  public:
    Iterator(Current current, End end, Function function)
        : current_(std::move(current)), end_(std::move(end)),
          function_(std::move(function)) {}
    Value operator*() const { return function_(*current_); }
    Iterator &operator++() {
      ++current_;
      return *this;
    }
    bool operator!=(Sentinel) const { return current_ != end_; }
  };
  Iterator begin() const { return {range_.begin(), range_.end(), function_}; }
  Sentinel end() const { return {}; }
};

template <class Range> class StrideView {
  Range range_;
  std::size_t step_;
  using Current = decltype(std::declval<const Range &>().begin());
  using End = decltype(std::declval<const Range &>().end());

public:
  StrideView(Range range, std::size_t step)
      : range_(std::move(range)), step_(step) {}
  class Iterator {
    Current current_;
    End end_;
    std::size_t step_;

  public:
    Iterator(Current current, End end, std::size_t step)
        : current_(std::move(current)), end_(std::move(end)), step_(step) {}
    decltype(auto) operator*() const { return *current_; }
    Iterator &operator++() {
      for (std::size_t n = 0; n < step_ && current_ != end_; ++n)
        ++current_;
      return *this;
    }
    bool operator!=(Sentinel) const { return current_ != end_; }
  };
  Iterator begin() const { return {range_.begin(), range_.end(), step_}; }
  Sentinel end() const { return {}; }
};

template <class Range> class EnumerateView {
  Range range_;
  using Current = decltype(std::declval<const Range &>().begin());
  using End = decltype(std::declval<const Range &>().end());

public:
  explicit EnumerateView(Range range) : range_(std::move(range)) {}
  class Iterator {
    Current current_;
    End end_;
    std::size_t index_ = 0;

  public:
    Iterator(Current current, End end)
        : current_(std::move(current)), end_(std::move(end)) {}
    auto operator*() const { return std::make_pair(index_, *current_); }
    Iterator &operator++() {
      ++current_;
      ++index_;
      return *this;
    }
    bool operator!=(Sentinel) const { return current_ != end_; }
  };
  Iterator begin() const { return {range_.begin(), range_.end()}; }
  Sentinel end() const { return {}; }
};

template <class Range> class DropWhileView {
  Range range_;
  using Value = std::decay_t<decltype(*std::declval<const Range &>().begin())>;
  std::function<bool(const Value &)> predicate_;

public:
  DropWhileView(Range range, std::function<bool(const Value &)> predicate)
      : range_(std::move(range)), predicate_(std::move(predicate)) {}
  auto begin() const {
    auto current = range_.begin();
    const auto end = range_.end();
    auto predicate = predicate_;
    while (current != end && predicate(*current))
      ++current;
    return current;
  }
  auto end() const { return range_.end(); }
};
} // namespace range_detail

struct Take {
  std::size_t count;
};
struct Drop {
  std::size_t count;
};
struct First {};
struct Stride {
  std::size_t step;
};
struct Enumerate {};
struct ToVector {};
struct Reverse {};
template <class Function> struct Transform {
  Function function;
};
template <class Predicate> struct DropWhile {
  Predicate predicate;
};
template <class Predicate, bool Until> struct Select {
  Predicate predicate;
};

inline Take take(std::ptrdiff_t count) { return {range_detail::count(count)}; }
inline Drop drop(std::ptrdiff_t count) { return {range_detail::count(count)}; }
inline First first() { return {}; }
inline Stride stride(std::ptrdiff_t step) {
  if (!step)
    throw std::invalid_argument("stride must be positive");
  return {range_detail::count(step)};
}
inline Enumerate enumerate() { return {}; }
inline ToVector to_vector() { return {}; }
inline Reverse reverse() { return {}; }
template <class Function> auto transform(Function function) {
  return Transform<Function>{std::move(function)};
}
template <class Predicate> auto drop_while(Predicate predicate) {
  return DropWhile<Predicate>{std::move(predicate)};
}
template <class Predicate> auto filter(Predicate predicate) {
  return Select<Predicate, false>{std::move(predicate)};
}
template <class Predicate> auto take_while(Predicate predicate) {
  return Select<Predicate, true>{std::move(predicate)};
}

template <class Range> auto operator|(Range &&range, Take adaptor) {
  return range_detail::TakeView<std::decay_t<Range>>(std::forward<Range>(range),
                                                     adaptor.count);
}
template <class Range> auto operator|(Range &&range, Drop adaptor) {
  return range_detail::DropView<std::decay_t<Range>>(std::forward<Range>(range),
                                                     adaptor.count);
}
template <class Range, class Predicate, bool Until>
auto operator|(Range &&range, Select<Predicate, Until> adaptor) {
  using Value = std::decay_t<decltype(*range.begin())>;
  // Stable view types avoid Clang Interpreter lambda-mangling problems across
  // cells. The callable itself still runs lazily on the foreground thread.
  using Function = std::function<bool(const Value &)>;
  return range_detail::PredicateView<std::decay_t<Range>, Function, Until>(
      std::forward<Range>(range), Function(std::move(adaptor.predicate)));
}
template <class Range> auto operator|(Range &&range, First) {
  auto current = range.begin();
  using Value = std::decay_t<decltype(*current)>;
  if (current != range.end())
    return std::optional<Value>(*current);
  return std::optional<Value>{};
}
template <class Range> auto operator|(Range &&range, Stride adaptor) {
  return range_detail::StrideView<std::decay_t<Range>>(
      std::forward<Range>(range), adaptor.step);
}
template <class Range> auto operator|(Range &&range, Enumerate) {
  return range_detail::EnumerateView<std::decay_t<Range>>(
      std::forward<Range>(range));
}
template <class Range, class Function>
auto operator|(Range &&range, Transform<Function> adaptor) {
  using Source = std::decay_t<decltype(*range.begin())>;
  using Value = std::decay_t<std::invoke_result_t<Function, const Source &>>;
  return range_detail::TransformView<std::decay_t<Range>, Value>(
      std::forward<Range>(range), std::move(adaptor.function));
}
template <class Range, class Predicate>
auto operator|(Range &&range, DropWhile<Predicate> adaptor) {
  return range_detail::DropWhileView<std::decay_t<Range>>(
      std::forward<Range>(range), std::move(adaptor.predicate));
}
template <class Range> auto operator|(Range &&range, ToVector) {
  using Value = std::decay_t<decltype(*range.begin())>;
  std::vector<Value> result;
  for (const auto &value : range)
    result.push_back(value);
  return result;
}
template <class Range> auto operator|(Range &&range, Reverse) {
  auto result = std::forward<Range>(range) | to_vector();
  std::reverse(result.begin(), result.end());
  return result;
}
} // namespace tracer_repl

#endif // CLANG_TRACER_REPL_EVENT_RANGES_HPP

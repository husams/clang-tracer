#pragma once

#if defined(__linux__) && defined(__x86_64__)

#include <cstdint>
#include <cxxabi.h>
#include <charconv>
#include <cmath>
#include <functional>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tracer::launch {
namespace detail {
static_assert(sizeof(int) == 4 && sizeof(long) == 8 && sizeof(long long) == 8 &&
              sizeof(float) == 4 && sizeof(double) == 8,
              "named calls require the Linux x86-64 scalar ABI");
inline constexpr char call_ready = 'C';
inline constexpr std::uint32_t call_protocol = 1;
inline constexpr unsigned max_call_arguments = 8;

enum class CallStatus : std::uint32_t {
  ok, unknown_name, ambiguous_name, wrong_count, invalid_argument, exception
};
enum class CallType : std::uint32_t {
  none, boolean, i32, u32, i64, u64, f32, f64
};

// Private SOCK_SEQPACKET protocol between the REPL and its child.
struct CallRequest {
  std::uint32_t version = call_protocol;
  std::uint32_t count = 0;
  char name[256]{};
  char arguments[max_call_arguments][64]{};
};
struct CallReply {
  std::uint32_t version = call_protocol;
  CallStatus status = CallStatus::ok;
  CallType type = CallType::none;
  std::uint32_t detail = 0;
  std::int64_t signed_value = 0;
  std::uint64_t unsigned_value = 0;
  double real_value = 0;
};

template <class T> using CallValue = std::remove_cv_t<std::remove_reference_t<T>>;

template <class Value>
constexpr CallType call_type() {
  using T = CallValue<Value>;
  if constexpr (std::is_same_v<T, bool>)
    return CallType::boolean;
  if constexpr (std::is_same_v<T, int>)
    return CallType::i32;
  if constexpr (std::is_same_v<T, unsigned int>)
    return CallType::u32;
  if constexpr (std::is_same_v<T, long> || std::is_same_v<T, long long>)
    return CallType::i64;
  if constexpr (std::is_same_v<T, unsigned long> || std::is_same_v<T, unsigned long long>)
    return CallType::u64;
  if constexpr (std::is_same_v<T, float>)
    return CallType::f32;
  if constexpr (std::is_same_v<T, double>)
    return CallType::f64;
  return CallType::none;
}

template <class T>
bool parse_scalar(std::string_view text, T &value) {
  if constexpr (std::is_same_v<T, bool>) {
    if (text == "true" || text == "false") {
      value = text == "true";
      return true;
    }
    return false;
  } else if constexpr (std::is_integral_v<T>) {
    if (text.empty())
      return false;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
  } else {
    // GCC 8's standard library has no floating-point from_chars overload.
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    input >> std::noskipws >> value;
    return input && input.eof() && std::isfinite(value);
  }
}

template <class R, class... Args>
class NamedCall {
  static_assert(sizeof...(Args) <= max_call_arguments,
                "a named call accepts up to eight scalar arguments");
  static_assert(((call_type<Args>() != CallType::none) && ...),
                "a named call requires supported scalar argument types");
  static_assert(((!std::is_rvalue_reference_v<Args> &&
                  !std::is_volatile_v<std::remove_reference_t<Args>>) && ...),
                "a named call accepts only value or nonvolatile lvalue arguments");
  static_assert(std::is_void_v<R> || call_type<R>() != CallType::none,
                "a named call requires a supported scalar or void result");
  static_assert(!std::is_rvalue_reference_v<R> && !std::is_volatile_v<std::remove_reference_t<R>>,
                "a named call accepts only value or nonvolatile lvalue results");

  std::string name_;
  std::function<R(Args...)> function_;

  template <std::size_t... I>
  CallReply invoke(const CallRequest &request, std::index_sequence<I...>) const {
    std::tuple<CallValue<Args>...> values{};
    unsigned index = 0;
    if (!((index = I, parse_scalar(request.arguments[I], std::get<I>(values))) && ...)) {
      CallReply reply;
      reply.status = CallStatus::invalid_argument;
      reply.detail = index + 1;
      return reply;
    }
    CallReply reply;
    reply.type = call_type<R>();
    if constexpr (std::is_void_v<R>) {
      std::apply(function_, values);
    } else {
      const auto value = std::apply(function_, values);
      if constexpr (std::is_floating_point_v<CallValue<R>>)
        reply.real_value = value;
      else if constexpr (std::is_signed_v<CallValue<R>>)
        reply.signed_value = value;
      else
        reply.unsigned_value = value;
    }
    return reply;
  }

public:
  NamedCall(std::string name, std::function<R(Args...)> function)
      : name_(std::move(name)), function_(std::move(function)) {}

  bool matches(std::string_view name) const {
    return function_ && name_ == name;
  }

  CallReply execute(const CallRequest &request) const {
    if (request.count != sizeof...(Args)) {
      CallReply reply;
      reply.status = CallStatus::wrong_count;
      reply.detail = sizeof...(Args);
      return reply;
    }
    try {
      return invoke(request, std::index_sequence_for<Args...>{});
    } catch (const __cxxabiv1::__forced_unwind &) {
      throw; // Preserve pthread cancellation and pthread_exit.
    } catch (...) {
      CallReply reply;
      reply.status = CallStatus::exception;
      return reply;
    }
  }
};

template <class... Calls>
CallReply dispatch_call(const CallRequest &request, const Calls &... calls) {
  const std::string_view name(request.name);
  const unsigned matches = (0u + ... + static_cast<unsigned>(calls.matches(name)));
  CallReply reply;
  if (matches != 1) {
    reply.status = matches ? CallStatus::ambiguous_name : CallStatus::unknown_name;
    return reply;
  }
  (void)((calls.matches(name) ? (reply = calls.execute(request), true) : false) || ...);
  return reply;
}
} // namespace detail

// Register one exact name and its compiler-checked function pointer.
template <class R, class... Args>
auto named_call(std::string name, R (*function)(Args...)) {
  return detail::NamedCall<R, Args...>(std::move(name), function);
}

// The target supplies a receiver that stays alive through its checkpoint.
template <class C, class R, class... Args>
auto named_call(std::string name, std::reference_wrapper<C> receiver, R (C::*method)(Args...)) {
  std::function<R(Args...)> bound;
  if (method)
    bound = [receiver, method](Args... values) -> R {
      return std::invoke(method, receiver, std::forward<Args>(values)...);
    };
  return detail::NamedCall<R, Args...>(std::move(name), std::move(bound));
}

template <class Receiver, class C, class R, class... Args>
auto named_call(std::string name, std::reference_wrapper<Receiver> receiver,
                R (C::*method)(Args...) const) {
  static_assert(std::is_invocable_r_v<R, decltype(method), Receiver &, Args...>,
                "receiver must be compatible with the registered method");
  std::function<R(Args...)> bound;
  if (method)
    bound = [receiver, method](Args... values) -> R {
      return std::invoke(method, receiver, std::forward<Args>(values)...);
    };
  return detail::NamedCall<R, Args...>(std::move(name), std::move(bound));
}
} // namespace tracer::launch
#endif

#ifndef CLANG_TRACER_CALLBACK_HPP
#define CLANG_TRACER_CALLBACK_HPP

#ifndef CLANG_TRACER_CALLBACK_TYPES_DEFINED
#include "tracer/protocol.hpp"
#endif

#ifndef CLANG_TRACER_CONTEXT_TYPES_DEFINED
#include "tracer/context.hpp"
#endif

#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace tracer {

template <std::size_t> struct ArgumentType;
namespace context_facade { struct NativeHookContextFactory; }

class Call {
public:
  explicit Call(const TraceContext &context) noexcept : context_(context) {}

  const Context &context() const;

  template <class T> T arg(std::size_t index) const {
    if (index >= context_.argument_count || index >= kMaxArguments) {
      throw std::out_of_range("callback argument index is out of range");
    }
    return decode<T>(context_.arguments[index]);
  }

  template <class T> T result() const {
    if (context_.phase != TracePhase::Post) {
      throw std::logic_error("callback result is only available in post()");
    }
    if constexpr (std::is_void_v<T>) {
      require_type(context_.result, ValueType::Void);
      return;
    } else {
      return decode<T>(context_.result);
    }
  }

  std::uintptr_t object_address() const noexcept {
    return static_cast<std::uintptr_t>(context_.object_address);
  }

#if defined(CLANG_TRACER_HAS_TYPED_RECEIVER)
  const ResolvedTarget *self() const {
    if (context_.object_address == 0) {
      throw std::logic_error(
          "callback receiver is unavailable for this invocation");
    }
    return reinterpret_cast<const ResolvedTarget *>(
        static_cast<std::uintptr_t>(context_.object_address));
  }
#else
  template <class T = void> T *self() const {
    static_assert(
        !std::is_same_v<T, T>,
        "typed self() requires a debug-selected supported non-static method");
    return nullptr;
  }
#endif

  std::uint64_t probe_id() const noexcept { return context_.probe_id; }

  std::uint64_t id() const noexcept { return context_.call_id; }

  bool log(std::string_view message,
           LogLevel level = LogLevel::Info) const noexcept {
    return context_.log(message, level);
  }

  // Read-only rendering preferences; both are off unless explicitly enabled.
  const CallbackPrintOptions &print_options() const noexcept {
    return context_.printing;
  }
  std::uint32_t argument_count() const noexcept {
    return context_.argument_count;
  }
  const TraceValue &argument_value(std::size_t index) const {
    if (index >= context_.argument_count || index >= kMaxArguments)
      throw std::out_of_range("callback argument index is out of range");
    return context_.arguments[index];
  }
  const char *argument_name(std::size_t index) const {
    (void)argument_value(index);
    static constexpr const char *fallback[kMaxArguments] = {
        "arg0", "arg1", "arg2", "arg3", "arg4", "arg5", "arg6", "arg7"};
    const char *name = context_.argument_names[index];
    return name && name[0] ? name : fallback[index];
  }
  const TraceValue &result_value() const {
    if (context_.phase != TracePhase::Post)
      throw std::logic_error("callback result is only available in post()");
    return context_.result;
  }

  // Both hooks observe the native stack at entry, before the original call.
  // Capture resolves function/module/file/line before returning; stack() reads
  // that same snapshot, including when --stack supplied automatic capture.
  const StackTrace &capture_stack(unsigned depth = 16) const {
    return capture_stack(depth, context_.stack_application_only);
  }
  const StackTrace &capture_stack(unsigned depth, bool application_only) const {
    if (!depth || depth > kMaxStackDepth)
      throw std::out_of_range("stack depth must be between 1 and 64");
    if (!context_.stack_capture)
      throw std::logic_error("native stack capture is unavailable");
    context_.stack_capture(&context_, depth, application_only);
    return context_.stack;
  }
  const StackTrace &stack() const noexcept { return context_.stack; }

  // Types and passing modes are supplied by the validated DWARF description.
  template <std::size_t Index> decltype(auto) argument() const {
    static_assert(Index < kMaxArguments,
                  "argument index exceeds the supported limit");
    using T = typename ArgumentType<Index>::type;
    if constexpr (std::is_reference_v<T>) {
      if (Index >= context_.argument_count)
        throw std::out_of_range("callback argument index is out of range");
      const auto &value = context_.arguments[Index];
      require_type(value, ArgumentType<Index>::object_value
                              ? ValueType::Object
                              : ValueType::Pointer);
      if (!value.unsigned_value)
        throw std::logic_error("null reference argument");
      return *reinterpret_cast<std::remove_reference_t<T> *>(
          static_cast<std::uintptr_t>(value.unsigned_value));
    } else {
      return arg<T>(Index);
    }
  }

  template <class T> T argument(const char *name, std::size_t index) const {
    const T value = arg<T>(index);
    record(name, context_.arguments[index], 1,
           static_cast<std::uint32_t>(index));
    return value;
  }

  template <class T> void field(T) const {
#ifndef CLANG_TRACER_FIELD_INFERENCE
    static_assert(kUnsupportedType<T>,
                  "field(value) requires Clang member-name inference; use "
                  "field(name, value) for other expressions");
#endif
  }
  template <class T> T argument(std::size_t index) const {
    return arg<T>(index);
  }

  template <class T> void field(const char *name, T value) const {
    record(name, encode(value), 2, 0);
  }

private:
  friend struct context_facade::NativeHookContextFactory;
  void record(const char *name, const TraceValue &value, std::uint32_t kind,
              std::uint32_t index) const {
    if (!name || !name[0]) {
      throw std::invalid_argument(
          "event value name must be a nonempty identifier");
    }
    std::size_t length = 0;
    while (name[length]) {
      const char c = name[length];
      if (length >= 63 ||
          !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
            (length && name[length - 1] != '.' && c >= '0' && c <= '9') ||
            (c == '.' && length && name[length - 1] != '.' &&
             name[length + 1]))) {
        throw std::invalid_argument(
            "event value name must be an identifier of at most 63 bytes");
      }
      ++length;
    }
    unsigned field_count = 0;
    for (std::uint32_t i = 0; i < context_.named_count && i < 16; ++i)
      if (context_.named_values[i].kind == 2)
        ++field_count;
    if (kind == 2 && field_count + context_.argument_count >= 16)
      throw std::out_of_range(
          "at most 16 arguments and fields combined per hook");
    if (context_.named_count >= 16) {
      throw std::out_of_range("at most 16 named values per hook are supported");
    }
    for (std::uint32_t i = 0; i < context_.named_count; ++i) {
      const auto &previous = context_.named_values[i];
      if (previous.kind != kind)
        continue;
      std::size_t j = 0;
      while (j < length && previous.name[j] == name[j])
        ++j;
      if ((j == length && previous.name[j] == '\0') ||
          (kind == 1 && previous.argument_index == index)) {
        throw std::invalid_argument(
            "duplicate event value name or argument index");
      }
    }
    auto &entry = context_.named_values[context_.named_count++];
    entry.kind = kind;
    entry.argument_index = index;
    for (std::size_t i = 0; i < length; ++i)
      entry.name[i] = name[i];
    entry.name[length] = '\0';
    entry.value = value;
  }

  template <class T> static TraceValue encode(T value) {
    TraceValue result{};
    if constexpr (std::is_pointer_v<T>) {
      result.type = ValueType::Pointer;
      result.unsigned_value = reinterpret_cast<std::uintptr_t>(value);
    } else if constexpr (std::is_same_v<T, float> ||
                         std::is_same_v<T, double>) {
      result.type =
          std::is_same_v<T, float> ? ValueType::Float32 : ValueType::Float64;
      result.real_value = value;
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> &&
                         sizeof(T) <= 4) {
      result.type = ValueType::Int32;
      result.signed_value = value;
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> &&
                         sizeof(T) == 8) {
      result.type = ValueType::Int64;
      result.signed_value = value;
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> &&
                         sizeof(T) <= 8) {
      result.type = ValueType::UInt64;
      result.unsigned_value = value;
    } else {
      static_assert(
          kUnsupportedType<T>,
          "named fields support scalar integers, float, double, and pointers");
    }
    return result;
  }

  template <class> static constexpr bool kUnsupportedType = false;

  static void require_type(const TraceValue &value, ValueType expected) {
    if (value.type != expected) {
      throw std::invalid_argument(
          "callback value type does not match requested type");
    }
  }

  template <class T> static T decode(const TraceValue &value) {
    using U = std::remove_cv_t<T>;
    if constexpr (std::is_pointer_v<U>) {
      require_type(value, ValueType::Pointer);
      return reinterpret_cast<U>(
          static_cast<std::uintptr_t>(value.unsigned_value));
    } else if constexpr (std::is_same_v<U, float>) {
      require_type(value, ValueType::Float32);
      return static_cast<T>(value.real_value);
    } else if constexpr (std::is_same_v<U, double>) {
      require_type(value, ValueType::Float64);
      return static_cast<T>(value.real_value);
    } else if constexpr (std::is_integral_v<U> && !std::is_same_v<U, bool> &&
                         std::is_signed_v<U> && sizeof(U) == 4) {
      require_type(value, ValueType::Int32);
      return static_cast<T>(value.signed_value);
    } else if constexpr (std::is_integral_v<U> && !std::is_same_v<U, bool> &&
                         std::is_signed_v<U> && sizeof(U) == 8) {
      require_type(value, ValueType::Int64);
      return static_cast<T>(value.signed_value);
    } else if constexpr (std::is_integral_v<U> && !std::is_same_v<U, bool> &&
                         std::is_unsigned_v<U> && sizeof(U) == 8) {
      require_type(value, ValueType::UInt64);
      return static_cast<T>(value.unsigned_value);
    } else {
      static_assert(kUnsupportedType<T>, "Call values support signed 32/64-bit "
                                         "integers, unsigned 64-bit integers, "
                                         "float, double, and pointers only");
    }
  }

  const TraceContext &context_;
};

} // namespace tracer

using TraceContext = tracer::TraceContext;
using TraceValue = tracer::TraceValue;
using ValueType = tracer::ValueType;
using Call = tracer::Call;
using TracePhase = tracer::TracePhase;

#endif // CLANG_TRACER_CALLBACK_HPP

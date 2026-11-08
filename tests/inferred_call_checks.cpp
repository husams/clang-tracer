#include "tracer/callback.hpp"

#include <cassert>
#include <stdexcept>
#include <type_traits>

using tracer::ReadOnlyObservationType;
using tracer::ReadOnlyPointee;

static_assert(std::is_same<ReadOnlyObservationType<int **>::type,
                           const int *const *>::value,
              "every pointee pointer layer must be read-only");
static_assert(std::is_same<ReadOnlyObservationType<int **&>::type,
                           const int *const *const &>::value,
              "pointer references must not allow reassignment at any layer");
static_assert(std::is_same<ReadOnlyObservationType<int *const *>::type,
                           const int *const *>::value,
              "existing pointer const must be preserved");
static_assert(std::is_same<ReadOnlyObservationType<int *volatile *>::type,
                           const int *const volatile *>::value,
              "volatile on inner pointer layers must be preserved");
static_assert(std::is_same<ReadOnlyObservationType<int *volatile &>::type,
                           const int *const volatile &>::value,
              "volatile pointer references must retain volatile qualification");
static_assert(std::is_same<ReadOnlyObservationType<int *const volatile &>::type,
                           const int *const volatile &>::value,
              "const volatile pointer slots must preserve both qualifiers");
static_assert(std::is_same<ReadOnlyObservationType<int *const volatile &&>::type,
                           const int *const volatile &>::value,
              "rvalue pointer slots must preserve volatile while borrowing");
static_assert(std::is_same<ReadOnlyObservationType<volatile int **>::type,
                           const volatile int *const *>::value,
              "volatile leaf qualifiers must be preserved");

struct BorrowedRecord {
  int value = 42;
  BorrowedRecord() = default;
  BorrowedRecord(const BorrowedRecord &) = delete;
};

namespace tracer {
template <> struct ResultType<0> {
#if TRACER_TEST_OBJECT_RESULT
  using type = BorrowedRecord;
  static constexpr bool object_value = true;
#else
  using type = int;
  static constexpr bool object_value = false;
#endif
};
template <> struct ArgumentType<0> {
  using type = const BorrowedRecord &;
  static constexpr bool object_value = false;
};
} // namespace tracer

template <class Exception, class F> void requires_failure(F function) {
  bool failed = false;
  try {
    function();
  } catch (const Exception &) {
    failed = true;
  }
  assert(failed);
}

int main() {
  BorrowedRecord record;
  tracer::TraceContext context{};
  context.phase = tracer::TracePhase::Post;
  context.argument_count = 1;
  context.arguments[0].type = tracer::ValueType::Pointer;
  context.arguments[0].unsigned_value =
      reinterpret_cast<std::uintptr_t>(&record);
  tracer::Call call(context);
  assert(&call.argument<0>() == &record);
  assert(call.argument<0>().value == 42);

#if TRACER_TEST_OBJECT_RESULT
  context.result.type = tracer::ValueType::Object;
  context.result.unsigned_value = reinterpret_cast<std::uintptr_t>(&record);
  static_assert(std::is_same<decltype(call.result()),
                             const BorrowedRecord &>::value,
                "object observation must preserve borrowed const reference");
  assert(&call.result() == &record);
  assert(call.result().value == 42);
  context.result.unsigned_value = 0;
  requires_failure<std::logic_error>([&] { (void)call.result(); });
#else
  context.result.type = tracer::ValueType::Int32;
  context.result.signed_value = 17;
  assert(call.result() == 17);
  assert(call.result<int>() == 17);
#endif

  context.phase = tracer::TracePhase::Pre;
  requires_failure<std::logic_error>([&] { (void)call.result(); });
  context.phase = tracer::TracePhase::Post;
  context.result.type = tracer::ValueType::Void;
  call.result<void>(); // Preserve the existing explicit void accessor.
  requires_failure<std::invalid_argument>([&] { (void)call.result(); });
}

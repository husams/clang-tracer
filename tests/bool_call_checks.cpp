#include "tracer/callback.hpp"

#include <cassert>

namespace tracer {
template <> struct ArgumentType<0> {
  using type = bool;
  static constexpr bool object_value = false;
};
template <> struct ResultType<0> {
  using type = bool;
  static constexpr bool object_value = false;
};
} // namespace tracer

int main() {
  tracer::TraceContext context{};
  context.phase = tracer::TracePhase::Post;
  tracer::TraceValue argument_storage[1]{};
  const char* argument_names[1]{};
  tracer::detail::NamedValueStorage named_storage;
  named_storage.values.resize(1);
  context.arguments = argument_storage;
  context.argument_names = argument_names;
  context.named_values = named_storage.values.data();
  context.named_storage = &named_storage;
  context.argument_count = 1;
  context.arguments[0].type = tracer::ValueType::Bool;
  context.arguments[0].signed_value = 1;
  context.result.type = tracer::ValueType::Bool;
  context.result.signed_value = 0;

  const tracer::Call call(context);
  assert(call.argument<0>());
  assert(!call.result());
}

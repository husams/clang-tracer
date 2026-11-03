#include "tracer/callback.hpp"
#include <cassert>

int main() {
  // Standalone legacy users do not have generated ResultType specializations.
  tracer::TraceContext context{};
  context.phase = tracer::TracePhase::Post;
  context.result.type = tracer::ValueType::Void;
  tracer::Call call(context);
  call.result<void>();
  context.result.type = tracer::ValueType::Int32;
  context.result.signed_value = 19;
  assert(call.result<int>() == 19);
}

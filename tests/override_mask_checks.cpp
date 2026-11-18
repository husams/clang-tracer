// Argument overrides must be addressable for every argument index the build accepts.
// Built with TRACER_MAX_ARGUMENTS=64 so indices above 31 exist.
#include "tracer/callback.hpp"

#include <cassert>
#include <cstdint>

namespace tracer {
template <> struct ArgumentType<0> {
  using type = int;
  static constexpr bool object_value = false;
};
} // namespace tracer

int main() {
  static_assert(tracer::kMaxArguments == 64, "this check needs the largest argument limit");
  tracer::ScalarOverrides overrides{};
  overrides.allow_arguments = true;
  tracer::TraceContext context{};
  context.phase = tracer::TracePhase::Pre;
  context.argument_count = tracer::kMaxArguments;
  context.scalar_overrides = &overrides;
  for (unsigned i = 0; i < tracer::kMaxArguments; ++i)
    context.arguments[i].type = tracer::ValueType::Int32;
  const tracer::Call call(context);
  for (const unsigned index : {0u, 31u, 32u, 63u}) {
    call.override_argument<int>(index, 100 + static_cast<int>(index));
    assert(overrides.argument_mask & (std::uint64_t{1} << index));
    assert(tracer::ScalarOverrides::unpack<int>(overrides.arguments[index]) == 100 + static_cast<int>(index));
  }
  // Only the requested arguments are marked.
  assert(overrides.argument_mask ==
         ((std::uint64_t{1} << 0) | (std::uint64_t{1} << 31) | (std::uint64_t{1} << 32) |
          (std::uint64_t{1} << 63)));
  bool rejected = false;
  try { call.override_argument<int>(tracer::kMaxArguments, 1); } catch (const std::out_of_range &) { rejected = true; }
  assert(rejected);
}

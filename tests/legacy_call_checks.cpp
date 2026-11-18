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

  // Dynamic native ABI: cross both the former 8-argument limit and 32-bit mask.
  std::vector<tracer::TraceValue> arguments(40);
  std::vector<const char*> names(40);
  tracer::detail::NamedValueStorage named;
  context.argument_count = arguments.size();
  context.arguments = arguments.data();
  context.argument_names = names.data();
  context.named_storage = &named;
  tracer::ScalarOverrides overrides;
  overrides.allow_arguments = true;
  std::vector<std::uint64_t> override_values(arguments.size());
  std::vector<unsigned char> override_set(arguments.size());
  overrides.arguments = override_values.data();
  overrides.argument_set = override_set.data();
  context.scalar_overrides = &overrides;
  context.phase = tracer::TracePhase::Pre;
  for (unsigned i = 0; i < arguments.size(); ++i) {
    arguments[i].type = tracer::ValueType::Int32;
    arguments[i].signed_value = i;
    assert(call.arg<int>(i) == static_cast<int>(i));
    call.override_argument<int>(i, i + 100);
    assert(overrides.argument_set[i]);
    assert(tracer::ScalarOverrides::unpack<int>(overrides.arguments[i]) ==
           static_cast<int>(i + 100));
    call.field(("field" + std::to_string(i)).c_str(), static_cast<int>(i));
  }
  assert(context.named_count == 40 && named.values.size() == 40);
  const std::string long_name(200, 'x');
  call.field(long_name.c_str(), 42);
  assert(std::string(named.values.back().name) == long_name);
  assert(std::string(call.argument_name(39)) == "arg39");
}

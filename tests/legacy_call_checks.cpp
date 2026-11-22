#include "tracer/callback.hpp"
#include <cassert>
#include <stdexcept>

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

  // Narrow integers are decoded from their carriers: signed up to 32 bits from int32 and unsigned
  // of every width from uint64. A value the requested type cannot hold is reported.
  arguments[0].type = tracer::ValueType::Int32;
  arguments[0].signed_value = -128;
  assert(call.arg<signed char>(0) == -128);
  arguments[0].signed_value = -3;
  assert(call.arg<short>(0) == -3);
  arguments[0].signed_value = 70000;
  assert(call.arg<int>(0) == 70000);
  bool out_of_range = false;
  try {
    (void)call.arg<short>(0);
  } catch (const std::out_of_range &) {
    out_of_range = true;
  }
  assert(out_of_range);
  arguments[1].type = tracer::ValueType::UInt64;
  arguments[1].unsigned_value = 250;
  assert(call.arg<unsigned char>(1) == 250);
  arguments[1].unsigned_value = 65535;
  assert(call.arg<unsigned short>(1) == 65535);
  arguments[1].unsigned_value = 0xFFFE;
  assert(call.arg<char16_t>(1) == u'\xFFFE');
  arguments[1].unsigned_value = 0x10FFFF;
  assert(call.arg<char32_t>(1) == U'\x10FFFF');
  arguments[1].unsigned_value = 4000000000u;
  assert(call.arg<unsigned int>(1) == 4000000000u);
  assert(call.arg<unsigned long>(1) == 4000000000ul);
  out_of_range = false;
  try {
    (void)call.arg<unsigned short>(1);
  } catch (const std::out_of_range &) {
    out_of_range = true;
  }
  assert(out_of_range);
  bool mismatch = false;
  try {
    (void)call.arg<unsigned char>(0); // An int32 slot is not an unsigned one.
  } catch (const std::invalid_argument &) {
    mismatch = true;
  }
  assert(mismatch);
}

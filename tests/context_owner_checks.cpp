#include "context_owner_generated.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace

void verify_generated_owner() {
  namespace ce = tracer::context_engine;
  const auto states = generated_owner_test_slots();
  const auto names = generated_owner_test_state_names();
  static_assert(states.size() == 3);
  auto state_index = [&](std::string_view name) {
    const auto found = std::find(names.begin(), names.end(), name);
    require(found != names.end(), "owner fixture omitted a named state");
    return static_cast<std::size_t>(found - names.begin());
  };
  const auto data_index = state_index("owner_demo::Data");
  const auto local_index = state_index("owner_demo::Local");
  const auto failed_index = state_index("owner_demo::Failed");
  require(states[data_index].policy == ce::Policy::shared &&
              states[local_index].policy == ce::Policy::confined,
          "owner factory state policy differs from its markers");
  std::any data, local, failed;
  require(states[data_index].construct({}, data) ==
                  ce::ErrorCode::construction_failed &&
              !data.has_value(),
          "owner invented root-call initialization data");
  tracer::TraceContext raw;
  raw.argument_count = 1;
  raw.arguments[0].type = tracer::ValueType::Int32;
  raw.arguments[0].signed_value = 42;
  const tracer::Call call(raw);
  require(states[data_index].construct({&call}, data) == ce::ErrorCode::ok &&
              owner_demo::made == 1,
          "generated owner did not select the ScopeInit constructor");
  auto *value =
      static_cast<owner_demo::Data *>(states[data_index].address(data));
  require(
      value && value->value == 42 && value->text == std::string(80, 'x'),
      "generated owner lost constructor data or copied a noncopyable payload");
  require(states[data_index].construct({&call}, data) ==
                  ce::ErrorCode::incompatible_registration &&
              owner_demo::made == 1,
          "owner replaced an already published payload");
  require(
      states[local_index].construct({}, local) == ce::ErrorCode::ok &&
          static_cast<owner_demo::Local *>(states[local_index].address(local))
                  ->values.size() == 3,
      "generated owner did not select ordinary default construction");
  require(!states[data_index].address(local),
          "owner cast a different state type");
  require(states[failed_index].construct({}, failed) ==
                  ce::ErrorCode::construction_failed &&
              !failed.has_value(),
          "owner bridge leaked a constructor exception or partial payload");
  states[data_index].destroy(data);
  states[data_index].destroy(data);
  states[local_index].destroy(local);
  require(!data.has_value() && !local.has_value() && owner_demo::destroyed == 1,
          "owner cleanup did not destroy its noncopyable state exactly once");
  std::cout << "generated owner factories, root metadata, noncopyable state "
               "and contained failure checks passed\n";
}

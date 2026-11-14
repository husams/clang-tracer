#include "record.hpp"

extern "C" void plugin_write(demo::PlanRecord &record) {
  record.value = TRACE_TEST_PLUGIN_VALUE;
}

namespace {
[[gnu::noinline]] void plugin_nested_store(demo::PlanRecord &record) {
  record.value = TRACE_TEST_PLUGIN_VALUE;
}

[[gnu::noinline]] void plugin_relay(demo::PlanRecord &record) {
  plugin_nested_store(record);
  asm volatile("" ::: "memory");
}
}

extern "C" [[gnu::noinline]] void plugin_nested(demo::PlanRecord &record) {
  plugin_relay(record);
  asm volatile("" ::: "memory");
}

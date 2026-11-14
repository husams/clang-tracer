#include "../object_plan/record.hpp"

extern "C" __attribute__((noinline, noipa)) void arm_shared_store(demo::PlanRecord &record) {
  record.value = 23;
}

#include "record.hpp"

namespace demo {
#ifdef OBJECT_PLAN_UNION
PlanRecord::PlanRecord(int) : floating(1.0f) {}
#else
PlanRecord::PlanRecord(int initial) : value(initial) {}
#endif
PlanRecord::~PlanRecord() = default;

void write_value(PlanRecord &record) {
  record.value = 23;
}
} // namespace demo

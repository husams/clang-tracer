#pragma once

namespace demo {
#ifdef OBJECT_PLAN_UNION
union PlanRecord {
  float floating;
#else
struct PlanRecord {
#ifdef OBJECT_PLAN_PADDED
  int sentinel = 99;
#endif
#endif
  int value;
  explicit PlanRecord(int initial);
  ~PlanRecord();
};

void write_value(PlanRecord &record);
} // namespace demo

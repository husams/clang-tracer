#pragma once

namespace prepared_demo {
int compute(int value);
double compute(double value);
struct LargeResult {
  int marker;
  long long payload[3];
};
LargeResult compute_struct(int value);
} // namespace prepared_demo

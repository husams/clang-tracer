#include "calculator.hpp"

namespace prepared_demo {
__attribute__((noinline)) int compute(int value) {
  volatile int scaled = value * 3;
  volatile int result = scaled + PREPARED_DEMO_BIAS;
  return result;
}

__attribute__((noinline)) double compute(double value) {
  volatile double result = value * 3.0 + PREPARED_DEMO_BIAS;
  return result;
}

__attribute__((noinline)) LargeResult compute_struct(int value) {
  volatile long long scaled = static_cast<long long>(value) * 1000;
  return {value + PREPARED_DEMO_BIAS,
          {scaled + 1, scaled + 2, scaled + 3}};
}
} // namespace prepared_demo

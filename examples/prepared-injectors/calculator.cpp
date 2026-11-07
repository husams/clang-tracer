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
} // namespace prepared_demo

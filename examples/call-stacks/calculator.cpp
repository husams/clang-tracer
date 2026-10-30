#include "calculator.hpp"

int stack_add(int left, int right) {
  volatile int saved = left;
  volatile int answer = saved + right;
  return answer;
}

int StackCalculator::multiply(int value, int factor) {
  ++calls;
  const int product = value * factor;
  return stack_add(product, bias + calls);
}

int library_dispatch(StackCalculator &calculator, int value, int factor) {
  const int answer = calculator.multiply(value, factor);
  return answer;
}

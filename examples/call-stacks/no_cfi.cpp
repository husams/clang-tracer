#include "calculator.hpp"

// Built without unwind tables to demonstrate an honestly partial snapshot.
int no_cfi_iteration(StackCalculator &calculator, int value, int factor) {
  const int answer = library_dispatch(calculator, value, factor);
  return answer;
}

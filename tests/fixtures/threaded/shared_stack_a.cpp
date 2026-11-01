#include "shared_stack.hpp"

extern "C" __attribute__((visibility("default"), noinline)) void
shared_stack_enter(SharedStackState *state) {
  SharedStackB method;
  method.run(state);
  volatile int keep_frame = 1;
  (void)keep_frame;
}

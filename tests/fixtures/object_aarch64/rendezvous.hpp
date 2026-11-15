#pragma once

#include <unistd.h>

extern "C" __attribute__((noinline, noipa)) void arm_rendezvous() {
  while (::access("write.now", F_OK))
    for (unsigned spin = 0; spin != 16384; ++spin)
      asm volatile("nop" ::: "memory");
}

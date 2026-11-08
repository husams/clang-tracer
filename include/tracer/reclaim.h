#pragma once

#include <stdint.h>

struct TracerCodeRange {
  uint64_t begin, end;
};
struct TracerRetiredCode {
  struct TracerCodeRange ranges[3];
};

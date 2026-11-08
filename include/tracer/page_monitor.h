#pragma once

#include <stdint.h>

enum { TRACER_PAGE_MONITOR_MAX_PAGES = 16 };
struct TracerPageAccess {
  uint64_t address;
  uint64_t pc;
  uint32_t tid;
  uint32_t access; /* 1 read, 2 write */
};
struct TracerPageMonitorResult {
  uint32_t count;
  uint32_t expired;
  struct TracerPageAccess accesses[TRACER_PAGE_MONITOR_MAX_PAGES];
};

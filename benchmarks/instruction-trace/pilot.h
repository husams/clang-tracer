#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t (*PilotWork)(unsigned rounds, int pid, int tid, int signal);
enum { PILOT_MAX_RECORDS = 8192 };

typedef struct {
  uint64_t value;
  uint64_t work_ns;
  uint32_t count;
  uint32_t truncated;
  uint32_t signals;
} PilotResult;

typedef int (*PilotGumRun)(PilotWork work, unsigned rounds, int pid, int tid,
                           int signal, uintptr_t begin, uintptr_t end,
                           uint32_t limit, PilotResult *result, uint32_t *offsets);
typedef int (*PilotGumLiveStart)(int tid, uintptr_t begin, uintptr_t end, uint32_t limit);
typedef int (*PilotGumLiveStop)(PilotResult *result, uint32_t *offsets, uint32_t capacity);

#ifdef __cplusplus
}
#endif

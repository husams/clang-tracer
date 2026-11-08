#pragma once

#include <stdint.h>

/* Versioned wire layout shared by the local server and optional Gum module. */
enum {
  TRACER_INSTRUCTION_MAX_BYTES = 65536,
  TRACER_INSTRUCTION_MAX_RECORDS = 8192,
  TRACER_INSTRUCTION_MAX_SECONDS = 60,
  TRACER_INSTRUCTION_RUNNING = 0,
  TRACER_INSTRUCTION_STOPPED = 1,
  TRACER_INSTRUCTION_LIMIT = 2,
  TRACER_INSTRUCTION_TIMEOUT = 3
};

typedef struct {
  uint32_t count;
  uint32_t reason;
  uint32_t offsets[TRACER_INSTRUCTION_MAX_RECORDS];
} TracerInstructionResultV1;

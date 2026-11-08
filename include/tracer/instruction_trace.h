#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Versioned wire layout shared by the local server and optional Gum module. */
enum {
  TRACER_INSTRUCTION_MAX_BYTES = 65536,
  TRACER_INSTRUCTION_MAX_RECORDS = 8192,
  TRACER_INSTRUCTION_MAX_SECONDS = 60,
  TRACER_INSTRUCTION_RUNNING = 0,
  TRACER_INSTRUCTION_STOPPED = 1,
  TRACER_INSTRUCTION_LIMIT = 2,
  TRACER_INSTRUCTION_TIMEOUT = 3,
  TRACER_INSTRUCTION_REGISTERS = 1,
  TRACER_INSTRUCTION_DISCOVER_CALLS = 2,
  TRACER_INSTRUCTION_MEMORY_READ = 1,
  TRACER_INSTRUCTION_MEMORY_WRITE = 2,
  TRACER_INSTRUCTION_REGISTER_COUNT = 16
};

typedef struct {
  uint32_t count;
  uint32_t reason;
  uint32_t offsets[TRACER_INSTRUCTION_MAX_RECORDS];
} TracerInstructionResultV1;

/* Pre-instruction general-purpose state. Order is part of the callback ABI. */
typedef struct {
  uint64_t rip;
  uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
} TracerInstructionRegistersV2;

/* Optional trusted C++ shared library entry point. It runs on the followed
 * thread, must return promptly, and must never throw or retain the pointer. */
uint64_t tracer_instruction_callback_v1(const TracerInstructionRegistersV2 *registers)
#ifdef __cplusplus
    noexcept
#endif
    ;

typedef struct {
  uint64_t begin, end;
  uint64_t memory_begin, memory_end;
  uint64_t callback;
  uint32_t tid, limit, seconds, flags;
} TracerInstructionRequestV2;

typedef struct {
  TracerInstructionRegistersV2 registers;
  uint64_t callback_value;
  union {
    uint64_t memory_address; // v2 memory capture
    uint64_t call_target; // v3 discovery
  };
  uint32_t memory_size;
  uint32_t memory_access;
} TracerInstructionDetailV2;

typedef struct {
  TracerInstructionResultV1 trace;
  TracerInstructionDetailV2 details[TRACER_INSTRUCTION_MAX_RECORDS];
} TracerInstructionResultV2;

#ifdef __cplusplus
}
#endif

#include "tracer/instruction_trace.h"

#include <cerrno>

extern "C" __attribute__((visibility("default")))
uint64_t tracer_instruction_callback_v1(const TracerInstructionRegistersV2 *registers) noexcept {
  static thread_local uint64_t calls = 0;
  errno = EDOM;
  return (registers->rip ^ registers->rdi ^ registers->rsi) + ++calls;
}

#define _GNU_SOURCE
#include "pilot.h"
#include "instruction_trace.h"
#include <frida-gum.h>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/* Linux UAPI value for SDKs whose headers predate Yama. */
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif

typedef struct {
  GumStalker *stalker;
  uintptr_t begin, end;
  uint32_t limit;
  uint32_t *offsets;
  uint64_t deadline_ns;
  uintptr_t memory_begin, memory_end;
  uint64_t (*callback)(const TracerInstructionRegistersV2 *);
  TracerInstructionDetailV2 *details;
  int registers;
  atomic_uint count, truncated;
  atomic_uint reason;
} Capture;

/* Live control operations are serialized on the fixture's leader. Only the
 * followed worker writes records, and the leader reads them after collection. */
static Capture live;
static GumStalkerTransformer *live_transformer;
static uint32_t live_offsets[PILOT_MAX_RECORDS];
static atomic_int preserve_ptracer;
static _Thread_local int *follow_result;
static pthread_t watchdog;
static int watchdog_joinable;
static int followed_tid;
static TracerInstructionResultV2 completed;

extern int __real_prctl(int option, ...);
int __wrap_prctl(int option, unsigned long a, unsigned long b,
                 unsigned long c, unsigned long d) {
  // The supported backend admits unrestricted same-UID ptrace. Gum's
  // helper needs no authorization override in that configuration.
  if (option == PR_SET_PTRACER && atomic_load(&preserve_ptracer))
    return 0;
  return __real_prctl(option, a, b, c, d);
}

extern gboolean __real_gum_process_modify_thread(
    GumThreadId, GumModifyThreadFunc, gpointer, GumModifyThreadFlags);
gboolean __wrap_gum_process_modify_thread(
    GumThreadId tid, GumModifyThreadFunc func, gpointer data, GumModifyThreadFlags flags) {
  const gboolean result = __real_gum_process_modify_thread(tid, func, data, flags);
  if (follow_result)
    *follow_result = result;
  return result;
}

static void *libdl_pin;

/* Gum 17.26.1 uses the private glibc entry point only to keep libdl
 * resident. Use public loader flags and release our reference at shutdown.
 * RTLD_NODELETE keeps the library resident after that reference is released. */
void *__wrap___libc_dlopen_mode(const char *name, int flags) {
  (void)flags;
  if (!libdl_pin)
    libdl_pin = dlopen(name, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
  return libdl_pin;
}

static uint64_t now_ns(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

static void record_instruction(const GumCpuContext *cpu, Capture *capture,
                               uintptr_t address, unsigned size, unsigned access) {
  if (atomic_load_explicit(&capture->reason, memory_order_relaxed) !=
      TRACER_INSTRUCTION_RUNNING)
    return;
  if (capture->deadline_ns && now_ns() >= capture->deadline_ns) {
    unsigned running = TRACER_INSTRUCTION_RUNNING;
    if (atomic_compare_exchange_strong(&capture->reason, &running, TRACER_INSTRUCTION_TIMEOUT))
      gum_stalker_unfollow_me(capture->stalker);
    return;
  }
  const unsigned count = atomic_load_explicit(&capture->count, memory_order_relaxed);
  if (count < capture->limit) {
    capture->offsets[count] = (uint32_t)(cpu->rip - capture->begin);
    if (capture->details) {
      TracerInstructionDetailV2 *detail = &capture->details[count];
      detail->memory_address = address;
      detail->memory_size = size;
      detail->memory_access = access;
      if (capture->registers || capture->callback) {
        const TracerInstructionRegistersV2 registers = {
          cpu->rip, cpu->rax, cpu->rbx, cpu->rcx, cpu->rdx,
          cpu->rsi, cpu->rdi, cpu->rbp, cpu->rsp,
          cpu->r8, cpu->r9, cpu->r10, cpu->r11, cpu->r12, cpu->r13, cpu->r14, cpu->r15
        };
        if (capture->registers)
          detail->registers = registers;
        if (capture->callback) {
          const int saved_errno = errno;
          detail->callback_value = capture->callback(&registers);
          errno = saved_errno;
        }
      }
    }
    atomic_store_explicit(&capture->count, count + 1, memory_order_release);
  } else {
    unsigned running = TRACER_INSTRUCTION_RUNNING;
    if (atomic_compare_exchange_strong(&capture->reason, &running, TRACER_INSTRUCTION_LIMIT)) {
      atomic_store_explicit(&capture->truncated, 1, memory_order_release);
      gum_stalker_unfollow_me(capture->stalker);
    }
  }
}

static void record(GumCpuContext *cpu, gpointer opaque) {
  record_instruction(cpu, opaque, 0, 0, 0);
}

typedef struct {
  Capture *capture;
  x86_reg base, index;
  int scale;
  int64_t displacement;
  uint64_t next_pc;
  unsigned size, access;
} MemoryOperand;

static int64_t register_offset(x86_reg reg) {
#define REGISTER(name, field) case X86_REG_##name: return offsetof(GumCpuContext, field)
  switch (reg) {
    REGISTER(RAX, rax); REGISTER(RBX, rbx); REGISTER(RCX, rcx); REGISTER(RDX, rdx);
    REGISTER(RSI, rsi); REGISTER(RDI, rdi); REGISTER(RBP, rbp); REGISTER(RSP, rsp);
    REGISTER(R8, r8); REGISTER(R9, r9); REGISTER(R10, r10); REGISTER(R11, r11);
    REGISTER(R12, r12); REGISTER(R13, r13); REGISTER(R14, r14); REGISTER(R15, r15);
    default: return -1;
  }
#undef REGISTER
}

static uint64_t register_value(const GumCpuContext *cpu, x86_reg reg, uint64_t next_pc) {
  if (reg == X86_REG_INVALID)
    return 0;
  if (reg == X86_REG_RIP)
    return next_pc;
  uint64_t value;
  memcpy(&value, (const char *)cpu + register_offset(reg), sizeof(value));
  return value;
}

static void record_memory(GumCpuContext *cpu, gpointer opaque) {
  const MemoryOperand *operand = opaque;
  Capture *capture = operand->capture;
  const uint64_t address = register_value(cpu, operand->base, operand->next_pc) +
      register_value(cpu, operand->index, operand->next_pc) * operand->scale +
      operand->displacement;
  if (address < capture->memory_end &&
      (address >= capture->memory_begin || capture->memory_begin - address < operand->size))
    record_instruction(cpu, capture, address, operand->size, operand->access);
}

/* Admit single scalar memory operands whose attempted access is known before
 * execution. Implicit, conditional, vector and segmented forms are excluded. */
static int scalar_memory_instruction(unsigned id) {
  switch (id) {
    case X86_INS_MOV: case X86_INS_MOVABS: case X86_INS_MOVSX:
    case X86_INS_MOVSXD: case X86_INS_MOVZX:
    case X86_INS_ADD: case X86_INS_ADC: case X86_INS_SUB: case X86_INS_SBB:
    case X86_INS_AND: case X86_INS_OR: case X86_INS_XOR:
    case X86_INS_INC: case X86_INS_DEC: case X86_INS_NEG: case X86_INS_NOT:
    case X86_INS_CMP: case X86_INS_TEST: case X86_INS_XCHG: case X86_INS_XADD:
      return 1;
    default:
      return 0;
  }
}

static void instrument_memory(GumStalkerIterator *iterator, const cs_insn *instruction,
                              Capture *capture) {
  if (!instruction->detail || !scalar_memory_instruction(instruction->id))
    return;
  const cs_x86 *x86 = &instruction->detail->x86;
  if (x86->addr_size != 8)
    return;
  for (uint8_t i = 0; i < x86->op_count; ++i) {
    const cs_x86_op *op = &x86->operands[i];
    if (op->type != X86_OP_MEM || !op->size || op->size > 8 ||
        op->mem.segment != X86_REG_INVALID)
      continue;
    const unsigned access = op->access & (CS_AC_READ | CS_AC_WRITE);
    if (!access ||
        (op->mem.base != X86_REG_INVALID && op->mem.base != X86_REG_RIP &&
         register_offset(op->mem.base) < 0) ||
        (op->mem.index != X86_REG_INVALID && register_offset(op->mem.index) < 0))
      continue;
    MemoryOperand *operand = g_new(MemoryOperand, 1);
    *operand = (MemoryOperand){capture, op->mem.base, op->mem.index, op->mem.scale,
        op->mem.disp, instruction->address + instruction->size, op->size, access};
    gum_stalker_iterator_put_callout(iterator, record_memory, operand, g_free);
  }
}

static void transform(GumStalkerIterator *iterator, GumStalkerOutput *output,
                      gpointer opaque) {
  (void)output;
  Capture *capture = opaque;
  const cs_insn *instruction;
  while (gum_stalker_iterator_next(iterator, &instruction)) {
    if (instruction->address >= capture->begin && instruction->address < capture->end) {
      if (capture->memory_begin)
        instrument_memory(iterator, instruction, capture);
      else
        gum_stalker_iterator_put_callout(iterator, record, capture, NULL);
    }
    gum_stalker_iterator_keep(iterator);
  }
}

static int drain(GumStalker *stalker) {
  gum_stalker_stop(stalker);
  for (unsigned attempt = 0; gum_stalker_garbage_collect(stalker); ++attempt) {
    if (attempt == 100)
      return -1;
    usleep(1000);
  }
  return 0;
}

static GumStalkerTransformer *prepare(Capture *capture, uintptr_t begin, uintptr_t end,
                                      uint32_t limit, uint32_t *offsets, uint64_t deadline_ns,
                                      const TracerInstructionRequestV2 *options) {
  capture->stalker = gum_stalker_new();
  capture->begin = begin;
  capture->end = end;
  capture->limit = limit;
  capture->offsets = offsets;
  capture->deadline_ns = deadline_ns;
  capture->memory_begin = options ? options->memory_begin : 0;
  capture->memory_end = options ? options->memory_end : 0;
  capture->callback = options ? (void *)options->callback : NULL;
  capture->registers = options && (options->flags & TRACER_INSTRUCTION_REGISTERS);
  capture->details = options ? completed.details : NULL;
  atomic_init(&capture->count, 0);
  atomic_init(&capture->truncated, 0);
  atomic_init(&capture->reason, TRACER_INSTRUCTION_RUNNING);
  return gum_stalker_transformer_make_from_callback(transform, capture, NULL);
}

static void read_result(const Capture *capture, PilotResult *result) {
  result->count = atomic_load_explicit(&capture->count, memory_order_acquire);
  result->truncated = atomic_load_explicit(&capture->truncated, memory_order_acquire);
}

__attribute__((visibility("default"))) int
pilot_gum_run(PilotWork work, unsigned rounds, int pid, int tid, int signal,
              uintptr_t begin, uintptr_t end, uint32_t limit, PilotResult *result,
              uint32_t *offsets) {
  Capture capture;
  GumStalkerTransformer *transformer = prepare(&capture, begin, end, limit, offsets, 0, NULL);
  GumStalker *stalker = capture.stalker;
  const uint64_t started = now_ns();
  gum_stalker_follow_me(stalker, transformer, NULL);
  result->value = work(rounds, pid, tid, signal);
  if (gum_stalker_is_following_me(stalker))
    gum_stalker_unfollow_me(stalker);
  result->work_ns = now_ns() - started;
  if (drain(stalker) != 0)
    return -1; /* Caller exits the owned fixture while capture storage is live. */
  read_result(&capture, result);
  const int still_following = gum_stalker_is_following_me(stalker);
  g_object_unref(transformer);
  g_object_unref(stalker);
  return still_following ? -1 : 0;
}

static int live_start(int tid, uintptr_t begin, uintptr_t end, uint32_t limit,
                      uint64_t deadline_ns, const TracerInstructionRequestV2 *options) {
  if (live.stalker || tid <= 0 || tid == (int)gum_process_get_current_thread_id() ||
      begin >= end || end - begin > UINT32_MAX || !limit || limit > PILOT_MAX_RECORDS ||
      syscall(SYS_tgkill, getpid(), tid, 0) != 0)
    return -1;
  live_transformer = prepare(&live, begin, end, limit, live_offsets, deadline_ns, options);
  // Keep dispatch checks at block boundaries so a running worker can observe
  // cross-thread unfollow even if it returns to a tight, already translated loop.
  gum_stalker_set_trust_threshold(live.stalker, -1);
  int followed = 0;
  follow_result = &followed;
  gum_stalker_follow(live.stalker, tid, live_transformer, NULL);
  follow_result = NULL;
  if (!followed) {
    // The helper can fail after writing the worker's registers. Keep the
    // backend mapped until the context is confirmed drained.
    if (drain(live.stalker) != 0)
      return -2;
    g_object_unref(live_transformer);
    g_object_unref(live.stalker);
    live.stalker = NULL;
    return -3;
  }
  return 0;
}

__attribute__((visibility("default"))) int
pilot_gum_live_start(int tid, uintptr_t begin, uintptr_t end, uint32_t limit) {
  return live_start(tid, begin, end, limit, 0, NULL);
}

__attribute__((visibility("default"))) int
pilot_gum_live_stop(PilotResult *result, uint32_t *offsets, uint32_t capacity) {
  if (!live.stalker || capacity < live.limit)
    return -1;
  if (drain(live.stalker) != 0) {
    read_result(&live, result);
    fprintf(stderr, "Gum context still pending after stop (%u records)\n", result->count);
    return -2; /* Keep live storage and the module resident for recovery. */
  }
  read_result(&live, result);
  memcpy(offsets, live_offsets, result->count * sizeof(*offsets));
  g_object_unref(live_transformer);
  g_object_unref(live.stalker);
  live.stalker = NULL;
  return 0;
}

__attribute__((visibility("default"))) int pilot_gum_init(void) {
  gum_init_embedded();
  return gum_stalker_is_supported() ? 0 : -1;
}

__attribute__((visibility("default"))) void pilot_gum_shutdown(void) {
  gum_deinit_embedded();
  if (libdl_pin) {
    dlclose(libdl_pin);
    libdl_pin = NULL;
  }
}

static void *expire_capture(void *unused) {
  (void)unused;
  while (atomic_load(&live.reason) == TRACER_INSTRUCTION_RUNNING) {
    if (now_ns() >= live.deadline_ns) {
      unsigned running = TRACER_INSTRUCTION_RUNNING;
      if (atomic_compare_exchange_strong(&live.reason, &running, TRACER_INSTRUCTION_TIMEOUT))
        gum_stalker_unfollow(live.stalker, followed_tid);
      break;
    }
    // A poll wait keeps the watchdog out of the injector's sleep admission
    // path, so a control call cannot run on the thread it needs to join.
    (void)syscall(SYS_poll, NULL, 0, 5);
  }
  return NULL;
}

static int start_capture(int tid, uintptr_t begin, uintptr_t end,
                         uint32_t limit, uint32_t seconds,
                         const TracerInstructionRequestV2 *options) {
  if (live.stalker || completed.trace.reason || !seconds ||
      seconds > TRACER_INSTRUCTION_MAX_SECONDS || end <= begin ||
      end - begin > TRACER_INSTRUCTION_MAX_BYTES ||
      prctl(PR_GET_DUMPABLE, 0UL, 0UL, 0UL, 0UL) != 1)
    return -1;
  atomic_store(&preserve_ptracer, 1);
  if (pilot_gum_init() != 0) {
    pilot_gum_shutdown();
    atomic_store(&preserve_ptracer, 0);
    return -1;
  }
  const uint64_t deadline_ns = now_ns() + (uint64_t)seconds * 1000000000;
  const int status = live_start(tid, begin, end, limit, deadline_ns, options);
  if (status != 0) {
    if (status != -2) {
      pilot_gum_shutdown();
      atomic_store(&preserve_ptracer, 0);
    }
    return status;
  }
  followed_tid = tid;
  if (pthread_create(&watchdog, NULL, expire_capture, NULL) != 0) {
    // Keep the handle reachable for stop if draining fails.
    unsigned running = TRACER_INSTRUCTION_RUNNING;
    atomic_compare_exchange_strong(&live.reason, &running, TRACER_INSTRUCTION_STOPPED);
    gum_stalker_unfollow(live.stalker, followed_tid);
    return -2;
  }
  watchdog_joinable = 1;
  return 0;
}

__attribute__((visibility("default"))) int
tracer_gum_instruction_start_v1(int tid, uintptr_t begin, uintptr_t end,
                               uint32_t limit, uint32_t seconds) {
  return start_capture(tid, begin, end, limit, seconds, NULL);
}

__attribute__((visibility("default"))) int
tracer_gum_instruction_start_v2(const TracerInstructionRequestV2 *request) {
  if (!request || request->flags & ~TRACER_INSTRUCTION_REGISTERS ||
      (request->memory_begin && request->memory_end <= request->memory_begin) ||
      (!request->memory_begin && request->memory_end))
    return -1;
  return start_capture(request->tid, request->begin, request->end,
      request->limit, request->seconds, request);
}

static int stop_capture(void) {
  if (!completed.trace.reason) {
    unsigned running = TRACER_INSTRUCTION_RUNNING;
    atomic_compare_exchange_strong(&live.reason, &running, TRACER_INSTRUCTION_STOPPED);
    if (watchdog_joinable) {
      if (pthread_join(watchdog, NULL) != 0)
        return -2;
      watchdog_joinable = 0;
    }
    PilotResult captured = {0};
    if (pilot_gum_live_stop(&captured, completed.trace.offsets, TRACER_INSTRUCTION_MAX_RECORDS) != 0)
      return -2;
    completed.trace.count = captured.count;
    completed.trace.reason = atomic_load(&live.reason);
    pilot_gum_shutdown();
    atomic_store(&preserve_ptracer, 0);
  }
  return 0;
}

__attribute__((visibility("default"))) int
tracer_gum_instruction_stop_v1(TracerInstructionResultV1 *result) {
  if (!result)
    return -1;
  const int status = stop_capture();
  if (!status)
    *result = completed.trace;
  return status;
}

__attribute__((visibility("default"))) int
tracer_gum_instruction_stop_v2(TracerInstructionResultV2 *result) {
  if (!result)
    return -1;
  const int status = stop_capture();
  if (!status) {
    result->trace = completed.trace;
    memcpy(result->details, completed.details, completed.trace.count * sizeof(*result->details));
  }
  return status;
}

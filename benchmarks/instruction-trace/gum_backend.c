#define _GNU_SOURCE
#include "pilot.h"
#include "instruction_trace.h"
#include <frida-gum.h>
#include <dlfcn.h>
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
static TracerInstructionResultV1 completed;

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

static void record(GumCpuContext *cpu, gpointer opaque) {
  Capture *capture = opaque;
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
    atomic_store_explicit(&capture->count, count + 1, memory_order_release);
  } else {
    unsigned running = TRACER_INSTRUCTION_RUNNING;
    if (atomic_compare_exchange_strong(&capture->reason, &running, TRACER_INSTRUCTION_LIMIT)) {
      atomic_store_explicit(&capture->truncated, 1, memory_order_release);
      gum_stalker_unfollow_me(capture->stalker);
    }
  }
}

static void transform(GumStalkerIterator *iterator, GumStalkerOutput *output,
                      gpointer opaque) {
  (void)output;
  Capture *capture = opaque;
  const cs_insn *instruction;
  while (gum_stalker_iterator_next(iterator, &instruction)) {
    if (instruction->address >= capture->begin && instruction->address < capture->end)
      gum_stalker_iterator_put_callout(iterator, record, capture, NULL);
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
                                      uint32_t limit, uint32_t *offsets, uint64_t deadline_ns) {
  capture->stalker = gum_stalker_new();
  capture->begin = begin;
  capture->end = end;
  capture->limit = limit;
  capture->offsets = offsets;
  capture->deadline_ns = deadline_ns;
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
  GumStalkerTransformer *transformer = prepare(&capture, begin, end, limit, offsets, 0);
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
                      uint64_t deadline_ns) {
  if (live.stalker || tid <= 0 || tid == (int)gum_process_get_current_thread_id() ||
      begin >= end || end - begin > UINT32_MAX || !limit || limit > PILOT_MAX_RECORDS ||
      syscall(SYS_tgkill, getpid(), tid, 0) != 0)
    return -1;
  live_transformer = prepare(&live, begin, end, limit, live_offsets, deadline_ns);
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
  return live_start(tid, begin, end, limit, 0);
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

__attribute__((visibility("default"))) int
tracer_gum_instruction_start_v1(int tid, uintptr_t begin, uintptr_t end,
                               uint32_t limit, uint32_t seconds) {
  if (live.stalker || completed.reason || !seconds ||
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
  const int status = live_start(tid, begin, end, limit, deadline_ns);
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
tracer_gum_instruction_stop_v1(TracerInstructionResultV1 *result) {
  if (!result)
    return -1;
  if (!completed.reason) {
    unsigned running = TRACER_INSTRUCTION_RUNNING;
    atomic_compare_exchange_strong(&live.reason, &running, TRACER_INSTRUCTION_STOPPED);
    if (watchdog_joinable) {
      if (pthread_join(watchdog, NULL) != 0)
        return -2;
      watchdog_joinable = 0;
    }
    PilotResult captured = {0};
    if (pilot_gum_live_stop(&captured, completed.offsets, TRACER_INSTRUCTION_MAX_RECORDS) != 0)
      return -2;
    completed.count = captured.count;
    completed.reason = atomic_load(&live.reason);
    pilot_gum_shutdown();
    atomic_store(&preserve_ptracer, 0);
  }
  *result = completed;
  return 0;
}

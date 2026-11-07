#define _GNU_SOURCE
#include "pilot.h"
#include <frida-gum.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  GumStalker *stalker;
  uintptr_t begin, end;
  uint32_t limit;
  uint32_t *offsets;
  atomic_uint count, truncated;
} Capture;

/* Live control operations are serialized on the fixture's leader. Only the
 * followed worker writes records, and the leader reads them after collection. */
static Capture live;
static GumStalkerTransformer *live_transformer;
static uint32_t live_offsets[PILOT_MAX_RECORDS];

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
  const unsigned count = atomic_load_explicit(&capture->count, memory_order_relaxed);
  if (count < capture->limit) {
    capture->offsets[count] = (uint32_t)(cpu->rip - capture->begin);
    atomic_store_explicit(&capture->count, count + 1, memory_order_release);
  } else if (!atomic_exchange_explicit(&capture->truncated, 1, memory_order_release)) {
    gum_stalker_unfollow_me(capture->stalker);
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
                                      uint32_t limit, uint32_t *offsets) {
  capture->stalker = gum_stalker_new();
  capture->begin = begin;
  capture->end = end;
  capture->limit = limit;
  capture->offsets = offsets;
  atomic_init(&capture->count, 0);
  atomic_init(&capture->truncated, 0);
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
  GumStalkerTransformer *transformer = prepare(&capture, begin, end, limit, offsets);
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

__attribute__((visibility("default"))) int
pilot_gum_live_start(int tid, uintptr_t begin, uintptr_t end, uint32_t limit) {
  if (live.stalker || tid <= 0 || tid == (int)gum_process_get_current_thread_id() ||
      begin >= end || end - begin > UINT32_MAX || !limit || limit > PILOT_MAX_RECORDS ||
      syscall(SYS_tgkill, getpid(), tid, 0) != 0)
    return -1;
  live_transformer = prepare(&live, begin, end, limit, live_offsets);
  // Keep dispatch checks at block boundaries so a running worker can observe
  // cross-thread unfollow even if it returns to a tight, already translated loop.
  gum_stalker_set_trust_threshold(live.stalker, -1);
  gum_stalker_follow(live.stalker, tid, live_transformer, NULL);
  return 0;
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

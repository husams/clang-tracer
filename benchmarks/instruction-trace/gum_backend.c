#define _GNU_SOURCE
#include "pilot.h"
#include <frida-gum.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  GumStalker *stalker;
  uintptr_t begin, end;
  uint32_t limit;
  PilotResult *result;
  uint32_t *offsets;
} Capture;

static uint64_t now_ns(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

static void record(GumCpuContext *cpu, gpointer opaque) {
  Capture *capture = opaque;
  if (capture->result->count < capture->limit) {
    capture->offsets[capture->result->count++] = (uint32_t)(cpu->rip - capture->begin);
  } else if (!capture->result->truncated) {
    capture->result->truncated = 1;
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

__attribute__((visibility("default"))) int
pilot_gum_run(PilotWork work, unsigned rounds, int pid, int tid, int signal,
              uintptr_t begin, uintptr_t end, uint32_t limit, PilotResult *result,
              uint32_t *offsets) {
  GumStalker *stalker = gum_stalker_new();
  Capture capture = {stalker, begin, end, limit, result, offsets};
  GumStalkerTransformer *transformer =
      gum_stalker_transformer_make_from_callback(transform, &capture, NULL);
  const uint64_t started = now_ns();
  gum_stalker_follow_me(stalker, transformer, NULL);
  result->value = work(rounds, pid, tid, signal);
  if (gum_stalker_is_following_me(stalker))
    gum_stalker_unfollow_me(stalker);
  result->work_ns = now_ns() - started;
  gum_stalker_stop(stalker);
  for (unsigned attempt = 0; gum_stalker_garbage_collect(stalker); ++attempt) {
    if (attempt == 100)
      return -1; /* Caller exits the owned fixture while capture storage is live. */
    usleep(1000);
  }
  const int still_following = gum_stalker_is_following_me(stalker);
  g_object_unref(transformer);
  g_object_unref(stalker);
  return still_following ? -1 : 0;
}

__attribute__((visibility("default"))) int pilot_gum_init(void) {
  gum_init_embedded();
  return gum_stalker_is_supported() ? 0 : -1;
}

__attribute__((visibility("default"))) void pilot_gum_shutdown(void) {
  gum_deinit_embedded();
}

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *name;
  const char *help;
  uint64_t (*run)(void);
} TracerReplCommandV1;

typedef struct {
  size_t size;
  size_t command_count;
  const TracerReplCommandV1 *commands;
} TracerReplPluginV1;

// Return a pointer to a constant descriptor owned by this plugin generation.
// Commands run synchronously and return an unsigned integer. Keep their state
// in the plugin, and do not start background work or let exceptions escape.
const TracerReplPluginV1 *tracer_repl_plugin_v1(void);

#ifdef __cplusplus
}
#endif

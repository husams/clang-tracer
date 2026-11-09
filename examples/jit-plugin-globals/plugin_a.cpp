#include "clang_tracer_repl_plugin.h"

extern "C" {
unsigned plugin_counter = 0;

unsigned tick() { return ++plugin_counter; }
}

namespace {
uint64_t tick_command() { return tick(); }
uint64_t address() { return reinterpret_cast<uintptr_t>(&plugin_counter); }
uint64_t value() { return plugin_counter; }

constexpr TracerReplCommandV1 commands[] = {
    {"tick", "Advance the counter and return its new value.", tick_command},
    {"address", "Return the address of this plugin's counter.", address},
    {"value", "Read the counter.", value},
};
constexpr TracerReplPluginV1 plugin = {sizeof(plugin), 3, commands};
} // namespace

const TracerReplPluginV1 *tracer_repl_plugin_v1() { return &plugin; }

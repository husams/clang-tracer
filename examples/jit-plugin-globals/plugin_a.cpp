#include "clang_tracer_repl_plugin.hpp"

unsigned plugin_counter = 0;

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
public:
  uint64_t tick() noexcept { return ++plugin_counter; }

  uint64_t address() const noexcept { return reinterpret_cast<uintptr_t>(&plugin_counter); }

  uint64_t value() const noexcept { return plugin_counter; }

  static constexpr Command commands[] = {
      command<&Plugin::tick>("tick", "Advance the counter and return its new value."),
      command<&Plugin::address>("address", "Return the address of this plugin's counter."),
      command<&Plugin::value>("value", "Read the counter."),
  };
};

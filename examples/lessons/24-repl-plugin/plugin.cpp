// A REPL plugin that counts events and reports a running total.
#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  uint64_t count_ = 0;

public:
  uint64_t tick() noexcept { return ++count_; }
  uint64_t value() const noexcept { return count_; }

  static constexpr Command commands[] = {
      command<&Plugin::tick>("tick", "Add one to the count and return it."),
      command<&Plugin::value>("value", "Return the count."),
  };
};

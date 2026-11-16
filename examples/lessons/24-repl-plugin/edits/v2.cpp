// The edited plugin: tick now adds ten, and reset is a new command.
#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  uint64_t count_ = 0;

public:
  uint64_t tick() noexcept { return count_ += 10; }
  uint64_t value() const noexcept { return count_; }
  uint64_t reset() noexcept { return count_ = 0; }

  static constexpr Command commands[] = {
      command<&Plugin::tick>("tick", "Add ten to the count and return it."),
      command<&Plugin::value>("value", "Return the count."),
      command<&Plugin::reset>("reset", "Set the count to zero."),
  };
};

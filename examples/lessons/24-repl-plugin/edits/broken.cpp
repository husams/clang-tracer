// A plugin that does not compile, to show that a failed reload keeps the old one.
#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
public:
  uint64_t tick() noexcept { return missing_name; }

  static constexpr Command commands[] = {
      command<&Plugin::tick>("tick", "Does not compile."),
  };
};

#include <my_api/my_type.hpp>

#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  uint64_t value() const noexcept { return my_api::my_instance.count; }

public:
  static constexpr Command commands[] = {
      command<&Plugin::value>("value", "Read the shared counter."),
  };
};

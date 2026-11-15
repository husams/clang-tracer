#pragma once

#include <type_traits>

#include "clang_tracer_repl_plugin.h"

namespace tracer::repl {

class Plugin;
using Command = TracerReplCommandV1;

template <typename Implementation>
class PluginInterface {
  static Implementation& instance() noexcept {
    static_assert(std::is_trivially_destructible_v<Implementation>,
                  "plugin instances must be trivially destructible");
    static constinit Implementation plugin{};
    return plugin;
  }

protected:
  template <auto Method>
  static constexpr Command command(const char* name, const char* help) {
    static_assert(noexcept(static_cast<uint64_t>((instance().*Method)())),
                  "plugin commands must be noexcept and return a value convertible to uint64_t");
    return {name, help,
            +[]() noexcept -> uint64_t { return static_cast<uint64_t>((instance().*Method)()); }};
  }
};

namespace detail {
template <typename Implementation>
const TracerReplPluginV1* descriptor() {
  constexpr auto& commands = Implementation::commands;
  static constexpr TracerReplPluginV1 value{sizeof(TracerReplPluginV1),
                                            sizeof(commands) / sizeof(commands[0]), commands};
  return &value;
}
} // namespace detail
} // namespace tracer::repl

// One plugin translation unit defines tracer::repl::Plugin. Instantiate the
// adapter after the class definition so author code only supplies the class.
extern "C" [[gnu::visibility("default")]] const TracerReplPluginV1* tracer_repl_plugin_v1() {
  return tracer::repl::detail::descriptor<tracer::repl::Plugin>();
}

#pragma once

#include <iosfwd>

#include "../native_plugins.hpp"
#include "commands.hpp"

namespace tracer_repl {
class CppEngine;

class Plugins {
  tracer::NativePlugins plugins_;

public:
  void execute(const Command &command, const CppEngine &cpp, std::ostream &out);
};

bool is_plugin_command(CommandKind kind);
} // namespace tracer_repl

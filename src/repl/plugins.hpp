#pragma once

#include <iosfwd>
#include <memory>

#include "commands.hpp"

namespace tracer_repl {
class CppEngine;

class Plugins {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  Plugins();
  ~Plugins();
  void execute(const Command &command, const CppEngine &cpp, std::ostream &out);
};

bool is_plugin_command(CommandKind kind);
} // namespace tracer_repl

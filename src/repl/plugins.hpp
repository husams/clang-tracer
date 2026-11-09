#pragma once

#include <iosfwd>

#include "../native_plugins.hpp"
#include "commands.hpp"

namespace clangtracer::v1 {
class AgentPluginRequest;
}

namespace tracer_repl {
class CppEngine;

class Plugins {
  tracer::NativePlugins plugins_;

public:
  void execute(const Command &command, const CppEngine &cpp, std::ostream &out);
  void offer_agent_object(clangtracer::v1::AgentPluginRequest &request,
                          const std::string &path, const CppEngine &cpp);
};

bool is_plugin_command(CommandKind kind);
} // namespace tracer_repl

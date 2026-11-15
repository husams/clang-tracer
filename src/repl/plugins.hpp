#pragma once

#include <iosfwd>
#include <map>
#include <set>

#include "../native_plugins.hpp"
#include "commands.hpp"

namespace clangtracer::v1 {
class AgentPluginRequest;
class Session;
}

namespace tracer_repl {
class Controller;
class CppEngine;

class Plugins {
  tracer::NativePlugins plugins_;

  struct InstalledPackage {
    std::filesystem::path manifest;
    bool has_include;
  };

  std::map<std::string, InstalledPackage> installed_;
  std::set<std::filesystem::path> public_headers_;

  void load(const std::string &id, const std::filesystem::path &source,
            const CppEngine &cpp, std::ostream &out, bool reload = false);

public:
  void load_installed(CppEngine& cpp, std::ostream& out, std::ostream& errors);
  void load_attached(Controller& controller, const clangtracer::v1::Session& session,
                     const CppEngine& cpp, std::ostream& errors);
  void load_enabled(const CppEngine &cpp, std::ostream &out, std::ostream &errors);
  void execute(const Command &command, const CppEngine &cpp, std::ostream &out);
  void offer_agent_object(clangtracer::v1::AgentPluginRequest &request,
                          const std::string &path, const CppEngine &cpp);
};

bool is_plugin_command(CommandKind kind);
} // namespace tracer_repl

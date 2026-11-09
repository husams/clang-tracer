#include "plugins.hpp"

#include "../agent_plugin_protocol.hpp"
#include "interpreter.hpp"
#include "mapping_cache.hpp"
#include "repl_config.hpp"
#include "server_process.hpp"
#include "tracer.pb.h"

#include <filesystem>
#include <ostream>

namespace tracer_repl {
namespace {
namespace fs = std::filesystem;

fs::path plugin_sdk_directory() {
  auto sdk = fs::path(executable_path()).parent_path() /
             fs::path(TRACER_REPL_API_RELATIVE).parent_path();
  if (!fs::is_regular_file(sdk / "clang_tracer_repl_plugin.h"))
    sdk = fs::path(TRACER_REPL_API_HEADER).parent_path();
  return fs::absolute(sdk).lexically_normal();
}

std::vector<std::string> plugin_profile(const CppEngine &cpp,
                                        const fs::path &sdk) {
  auto arguments = cpp.effective_compiler_options();
  arguments.insert(arguments.begin(), {"-I", sdk.string()});
  return arguments;
}

} // namespace

bool is_plugin_command(CommandKind kind) {
  using K = CommandKind;
  return kind == K::plugin_load || kind == K::plugin_reload ||
         kind == K::plugin_list || kind == K::plugin_run ||
         kind == K::plugin_commands;
}

void Plugins::execute(const Command &command, const CppEngine &cpp,
                      std::ostream &out) {
  using K = CommandKind;
  if (command.kind == K::plugin_list) {
    const auto list = plugins_.list();
    if (list.empty())
      out << "(no plugins loaded)\n";
    for (const auto &plugin : list)
      out << plugin.id << " generation " << plugin.generation << "  "
          << plugin.source.string() << '\n';
    return;
  }
  if (command.kind == K::plugin_load || command.kind == K::plugin_reload) {
    const bool reload = command.kind == K::plugin_reload;
    const auto id = reload ? command.arguments.at(0) : command.option("--as");
    const auto path = reload ? plugins_.info(id).source
                             : fs::absolute(command.arguments.at(0));
    const auto result = plugins_.load(
        id, {path, {}, {}, {}}, plugin_profile(cpp, plugin_sdk_directory()),
        mapping_cache_directory().parent_path() / "plugins", reload);
    out << id << " generation " << result.generation << " active ("
        << (result.cache_hit ? "cache hit" : "compiled") << ")\n";
    return;
  }

  const auto &id = command.arguments.at(0);
  if (command.kind == K::plugin_commands) {
    for (const auto &[name, help] : plugins_.commands(id))
      out << "%plugin run " << id << ' ' << name << "  " << help << '\n';
  } else {
    const auto &name = command.arguments.at(1);
    const auto value = plugins_.run(id, name);
    out << id << '.' << name << " = " << value << '\n';
  }
}
void Plugins::offer_agent_object(clangtracer::v1::AgentPluginRequest &request,
                                 const std::string &path,
                                 const CppEngine &cpp) {
#if defined(__linux__) && defined(__x86_64__)
  try {
    const auto source = fs::absolute(path);
    const auto sdk = plugin_sdk_directory();
    auto object = plugins_.prepare(
        {source, request.source(), {}, {}}, plugin_profile(cpp, sdk),
        mapping_cache_directory().parent_path() / "plugins");
    if (object.bytes.size() > tracer::kMaxAgentPluginOffer)
      return;
    auto *offer = request.mutable_repl_object();
    offer->set_source_path(source.string());
    offer->set_sdk_directory(sdk.string());
    offer->set_fingerprint(std::move(object.fingerprint));
    offer->set_checksum(std::move(object.checksum));
    offer->set_object(std::move(object.bytes));
  } catch (const std::exception &) {
    // The target can compile the same source with its own profile.
    request.clear_repl_object();
  }
#else
  (void)request;
  (void)path;
  (void)cpp;
#endif
}
} // namespace tracer_repl

#include "plugins.hpp"

#include "interpreter.hpp"
#include "mapping_cache.hpp"
#include "repl_config.hpp"
#include "server_process.hpp"

#include <filesystem>
#include <ostream>

namespace tracer_repl {
namespace {
namespace fs = std::filesystem;

std::vector<std::string> plugin_profile(const CppEngine &cpp) {
  auto sdk = fs::path(executable_path()).parent_path() /
             fs::path(TRACER_REPL_API_RELATIVE).parent_path();
  if (!fs::is_regular_file(sdk / "clang_tracer_repl_plugin.h"))
    sdk = fs::path(TRACER_REPL_API_HEADER).parent_path();
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
        id, {path, {}, {}}, plugin_profile(cpp),
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
} // namespace tracer_repl

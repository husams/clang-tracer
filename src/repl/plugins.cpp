#include "plugins.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <system_error>

#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../agent_plugin_protocol.hpp"
#include "compiler_profile.hpp"
#include "controller.hpp"
#include "interpreter.hpp"
#include "mapping_cache.hpp"
#include "plugin_package.hpp"
#include "repl_config.hpp"
#include "server_process.hpp"
#include "tracer.pb.h"

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

fs::path enabled_plugin_directory(bool create) {
  fs::path config;
  if (const auto *xdg = std::getenv("XDG_CONFIG_HOME");
      xdg && fs::path(xdg).is_absolute()) {
    config = xdg;
  } else if (const auto *home = std::getenv("HOME");
             home && fs::path(home).is_absolute()) {
    config = fs::path(home) / ".config";
  } else if (create) {
    throw std::runtime_error("set XDG_CONFIG_HOME or HOME to enable a plugin");
  } else {
    return {};
  }
  const auto directory = config / "clang-tracer/repl/enabled-plugins";
  if (!create && !fs::exists(fs::symlink_status(directory)))
    return {};
  return tracer::private_plugin_directory(directory, create);
}

fs::path plugin_record(const fs::path &directory, const std::string &id) {
  // Keep IDs distinct even on case-insensitive filesystems.
  return directory / (mapping_digest(id) + ".plugin");
}

void save_enabled_plugin(const fs::path &record, const std::string &id,
                         const fs::path &source) {
  auto file = llvm::sys::fs::TempFile::create(
      record.string() + ".%%%%%%",
      llvm::sys::fs::owner_read | llvm::sys::fs::owner_write);
  if (!file)
    throw std::runtime_error(llvm::toString(file.takeError()));
  std::error_code error;
  {
    llvm::raw_fd_ostream output(file->FD, false);
    output << id << '\n' << source.string();
    output.flush();
    error = output.error();
    output.clear_error();
  }
  if (error)
    throw std::runtime_error(llvm::toString(
        llvm::joinErrors(llvm::errorCodeToError(error), file->discard())));
  if (auto keep_error = file->keep(record.string()))
    throw std::runtime_error(llvm::toString(std::move(keep_error)));
}

std::vector<tracer::NativePluginUnit> plugin_units(const PluginPackage& package,
                                                   const CppEngine& cpp) {
  const auto sdk = plugin_sdk_directory();
  const auto extras = resolve_plugin_flag_paths(package.extra_flags, package.directory);
  std::vector<tracer::NativePluginUnit> units;
  for (const auto& source : package.sources) {
    auto arguments = plugin_profile(cpp, sdk);
    if (!package.compile_commands.empty()) {
      const auto file = package.translation_unit.empty() ? source : package.translation_unit;
      const auto profile = compilation_database_profile(
          package.compile_commands.string(), file.string(),
          package.translation_unit.empty() ? DatabaseSelection::ExactOrUnique
                                           : DatabaseSelection::Exact);
      arguments.insert(arguments.end(), profile.arguments.begin(), profile.arguments.end());
    }
    if (!package.include.empty()) {
      arguments.insert(arguments.end(), {"-I", package.include.string()});
    }
    arguments.insert(arguments.end(), extras.begin(), extras.end());
    units.push_back({{source, {}, {}, {}, {}}, std::move(arguments)});
  }
  return units;
}

void report_load(std::ostream& out, const std::string& id,
                 const tracer::NativePlugins::LoadResult& result) {
  out << id << " generation " << result.generation << " active ("
      << (result.cache_hit ? "cache hit" : "compiled") << ")\n";
}

std::set<fs::path> agent_package_files(const PluginPackage& package) {
  std::set<fs::path> paths(package.sources.begin(), package.sources.end());
  std::set<fs::path> roots;
  if (!package.include.empty()) {
    roots.insert(package.include);
  }
  for (const auto& source : package.sources) {
    roots.insert(source.parent_path());
  }
  for (const auto& root : roots) {
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
      if (!fs::is_regular_file(entry.symlink_status())) {
        continue;
      }
      const auto extension = entry.path().extension();
      if (root == package.include || extension == ".h" || extension == ".hh" ||
          extension == ".hpp" || extension == ".hxx" || extension == ".inc" ||
          extension == ".inl" || extension == ".ipp" || extension == ".tpp") {
        paths.insert(entry.path());
      }
    }
  }
  return paths;
}

} // namespace

bool is_plugin_command(CommandKind kind) {
  using K = CommandKind;
  return kind == K::plugin_load || kind == K::plugin_enable ||
         kind == K::plugin_disable || kind == K::plugin_reload ||
         kind == K::plugin_list || kind == K::plugin_run ||
         kind == K::plugin_commands;
}

void Plugins::load(const std::string &id, const fs::path &source,
                   const CppEngine &cpp, std::ostream &out, bool reload) {
  const auto result = plugins_.load(
      id, {source, {}, {}, {}, {}}, plugin_profile(cpp, plugin_sdk_directory()),
      mapping_cache_directory().parent_path() / "plugins", reload);
  report_load(out, id, result);
}

void Plugins::load_installed(CppEngine& cpp, std::ostream& out, std::ostream& errors) {
  try {
    for (const auto& manifest : installed_plugin_manifests()) {
      try {
        const auto package = read_plugin_package(manifest);
        if (!package) {
          continue;
        }
        std::vector<fs::path> headers;
        if (!package->include.empty()) {
          for (const auto& entry : fs::recursive_directory_iterator(package->include)) {
            if (fs::is_symlink(entry.symlink_status())) {
              throw std::runtime_error("public plugin includes cannot use symlinks: " +
                                       entry.path().string());
            }
            if (fs::is_regular_file(entry.symlink_status())) {
              const auto relative = entry.path().lexically_relative(package->include);
              if (public_headers_.contains(relative)) {
                throw std::runtime_error("duplicate public plugin header: " + relative.string());
              }
              headers.push_back(relative);
            }
          }
        }
        const auto result =
            plugins_.load_package(package->id, plugin_units(*package, cpp), package->include,
                                  mapping_cache_directory().parent_path() / "plugins", false);
        try {
          cpp.add_plugin_api(package->include, plugins_.public_symbols(package->id));
        } catch (...) {
          plugins_.discard(package->id);
          throw;
        }
        public_headers_.insert(headers.begin(), headers.end());
        installed_.emplace(package->id, InstalledPackage{manifest, !package->include.empty()});
        report_load(out, package->id, result);
      } catch (const std::exception& error) {
        errors << "startup plugin " << manifest.string() << ": " << error.what() << '\n';
      }
    }
  } catch (const std::exception& error) {
    errors << "cannot read installed plugins: " << error.what() << '\n';
  }
}

void Plugins::load_attached(Controller& controller, const clangtracer::v1::Session& session,
                            const CppEngine& cpp, std::ostream& errors) {
  if (session.executable().empty()) {
    return;
  }
  try {
    for (const auto& manifest : installed_plugin_manifests()) {
      try {
        const auto package = read_plugin_package(manifest, PluginComponent::Agent);
        if (!package || package->executable != session.executable()) {
          continue;
        }
        using Request = clangtracer::v1::AgentPluginRequest;
        Request request;
        request.set_session_id(session.id());
        request.set_plugin_id(package->id);
        request.set_expected_executable(package->executable);
        const auto files = agent_package_files(*package);
        if (files.size() == 1 && package->compile_commands.empty() &&
            package->extra_flags.empty()) {
          request.set_operation(Request::LOAD);
          const auto source = package->sources.front().string();
          request.set_source(read_source(source));
          offer_agent_object(request, source, cpp);
        } else {
          request.set_operation(Request::LOAD_PACKAGE);
          auto& bundle = *request.mutable_package();
          bundle.set_compile_commands(package->compile_commands.string());
          bundle.set_translation_unit(package->translation_unit.string());
          for (const auto& flag : package->extra_flags) {
            bundle.add_extra_flags(flag);
          }
          for (const auto& source : package->sources) {
            bundle.add_sources(source.lexically_relative(package->directory).string());
          }
          std::uintmax_t size = bundle.ByteSizeLong();
          for (const auto& path : files) {
            const auto bytes = fs::file_size(path);
            if (bytes > tracer::kMaxAgentPluginSource ||
                size > tracer::kMaxAgentPluginSource - bytes) {
              throw std::runtime_error("agent plugin package exceeds 4 MiB");
            }
            size += bytes;
            auto& file = *bundle.add_files();
            file.set_path(path.lexically_relative(package->directory).string());
            file.set_contents(read_source(path.string()));
          }
          if (bundle.ByteSizeLong() > tracer::kMaxAgentPluginSource) {
            throw std::runtime_error("agent plugin package exceeds 4 MiB");
          }
        }
        controller.agent_plugin(request);
      } catch (const std::exception& error) {
        errors << "attach plugin " << manifest.string() << ": " << error.what() << '\n';
      }
    }
  } catch (const std::exception& error) {
    errors << "cannot read agent plugin packages: " << error.what() << '\n';
  }
}

void Plugins::load_enabled(const CppEngine &cpp, std::ostream &out,
                           std::ostream &errors) {
  try {
    const auto directory = enabled_plugin_directory(false);
    if (directory.empty())
      return;
    std::vector<fs::path> records;
    for (const auto &entry : fs::directory_iterator(directory))
      if (entry.path().extension() == ".plugin")
        records.push_back(entry.path());
    std::sort(records.begin(), records.end());
    for (const auto &record : records) {
      auto label = record.string();
      try {
        struct stat info{};
        if (::lstat(record.c_str(), &info) || !S_ISREG(info.st_mode) ||
            info.st_uid != ::geteuid() || (info.st_mode & 0022))
          throw std::runtime_error("expected a regular, owner-controlled record");
        std::ifstream input(record, std::ios::binary);
        std::string id;
        if (!std::getline(input, id) || plugin_record(directory, id) != record)
          throw std::runtime_error("invalid enabled plugin record");
        label = id;
        const std::string path{std::istreambuf_iterator<char>(input), {}};
        if (!fs::path(path).is_absolute() || path.find('\0') != path.npos)
          throw std::runtime_error("expected an absolute plugin source path");
        load(id, path, cpp, out);
      } catch (const std::exception &error) {
        errors << "startup plugin " << label << ": " << error.what() << '\n';
      }
    }
  } catch (const std::exception &error) {
    errors << "cannot read enabled plugins: " << error.what() << '\n';
  }
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
  if (command.kind == K::plugin_load) {
    load(command.option("--as"), fs::absolute(command.arguments.at(0)), cpp, out);
    return;
  }
  if (command.kind == K::plugin_reload) {
    const auto &id = command.arguments.at(0);
    if (const auto installed = installed_.find(id); installed != installed_.end()) {
      const auto package = read_plugin_package(installed->second.manifest);
      if (!package || package->id != id) {
        throw std::runtime_error("installed plugin manifest changed its identity");
      }
      if (installed->second.has_include || !package->include.empty()) {
        throw std::runtime_error("restart the REPL to reload a plugin with public C++ headers");
      }
      const auto result =
          plugins_.load_package(id, plugin_units(*package, cpp), {},
                                mapping_cache_directory().parent_path() / "plugins", true);
      report_load(out, id, result);
    } else {
      load(id, plugins_.info(id).source, cpp, out, true);
    }
    return;
  }
  if (command.kind == K::plugin_enable) {
    const auto id = command.option("--as");
    const auto source = fs::absolute(command.arguments.at(0));
    const auto record = plugin_record(enabled_plugin_directory(true), id);
    const auto active = plugins_.list();
    const auto found = std::find_if(active.begin(), active.end(), [&](const auto &plugin) {
      return plugin.id == id;
    });
    if (found == active.end())
      load(id, source, cpp, out);
    else if (found->source != source)
      throw std::invalid_argument("plugin already loaded from " + found->source.string());
    save_enabled_plugin(record, id, source);
    out << id << " enabled at startup\n";
    return;
  }
  if (command.kind == K::plugin_disable) {
    const auto &id = command.arguments.at(0);
    const auto directory = enabled_plugin_directory(false);
    if (directory.empty() || !fs::remove(plugin_record(directory, id)))
      throw std::invalid_argument("plugin is not enabled: " + id);
    out << id << " disabled at startup\n";
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
        {source, request.source(), {}, {}, {}}, plugin_profile(cpp, sdk),
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

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

#include <unistd.h>

#include "agent_plugin_protocol.hpp"
#include "callback_source.hpp"
#include "context_scheduler.hpp"
#include "native_plugins.hpp"
#include "repl/compiler_profile.hpp"
#include "runtime_paths.hpp"
#include "tracer.pb.h"

namespace {
namespace fs = std::filesystem;
using tracer::AgentPluginOperation;

const pid_t agent_pid = ::getpid();
std::shared_mutex plugin_mutex;
std::atomic<bool> has_observers{false};
std::map<std::string, tracer::NativePlugins, std::less<>> sessions;

fs::path cache_directory() {
  if (const auto *xdg = std::getenv("XDG_CACHE_HOME");
      xdg && fs::path(xdg).is_absolute())
    return fs::path(xdg) / "clang-tracer/agent/plugins";
  if (const auto *home = std::getenv("HOME");
      home && fs::path(home).is_absolute())
    return fs::path(home) / ".cache/clang-tracer/agent/plugins";
  return fs::temp_directory_path() /
         ("clang-tracer-agent-" + std::to_string(::geteuid())) / "plugins";
}

template <std::size_t N> bool terminated(const char (&text)[N]) {
  return std::memchr(text, '\0', N) != nullptr;
}

bool absolute_path(const std::string &path) {
  return path.find('\0') == std::string::npos && fs::path(path).is_absolute();
}

fs::path bundle_path(const std::string& path, const fs::path& root) {
  const fs::path relative(path);
  if (relative.empty() || relative.is_absolute() || path.find('\0') != path.npos ||
      relative.lexically_normal() != relative || *relative.begin() == ".." ||
      *relative.begin() == ".") {
    throw std::invalid_argument("invalid agent package file path");
  }
  return root / relative;
}

std::vector<std::string> target_profile() {
  return tracer::native_plugin_compiler_arguments(tracer::runtime::clang_resource_dir(),
                                                   tracer::runtime::jit_arguments());
}

tracer::NativePlugins::LoadResult load_package(const tracer::AgentPluginCall& request,
                                               const char* source, bool reload) {
  clangtracer::v1::AgentPluginPackage package;
  if (!source || !package.ParseFromArray(source, static_cast<int>(request.source_size)) ||
      package.sources().empty()) {
    throw std::invalid_argument("invalid agent plugin package");
  }
  if ((!package.compile_commands().empty() && !absolute_path(package.compile_commands())) ||
      (!package.translation_unit().empty() && !absolute_path(package.translation_unit()))) {
    throw std::invalid_argument("agent package compiler paths must be absolute");
  }

  const fs::path root = "/__clang_tracer_plugin__/package";
  const auto sdk = root / "clang_tracer_repl_plugin.h";
  std::map<fs::path, std::string_view> contents;
  std::vector<tracer::NativePluginFile> files;
  for (const auto& file : package.files()) {
    auto path = bundle_path(file.path(), root);
    if (path == sdk || path == root / "clang_tracer_repl_plugin.hpp" ||
        path == root / "clang_tracer_agent_extension.hpp" ||
        !contents.emplace(path, file.contents()).second) {
      throw std::invalid_argument("duplicate or reserved agent package file");
    }
    files.push_back({std::move(path), file.contents()});
  }
  const std::vector<std::string> extra(package.extra_flags().begin(), package.extra_flags().end());
  const auto flags = tracer_repl::resolve_plugin_flag_paths(extra, root);
  std::vector<tracer::NativePluginUnit> units;
  for (const auto& source_path : package.sources()) {
    const auto path = bundle_path(source_path, root);
    const auto file = contents.find(path);
    if (file == contents.end()) {
      throw std::invalid_argument("agent package translation unit is missing");
    }
    auto profile = target_profile();
    if (!package.compile_commands().empty()) {
      using tracer_repl::DatabaseSelection;
      const bool select = !package.translation_unit().empty();
      const auto configuration = tracer_repl::compilation_database_profile(
          package.compile_commands(), select ? package.translation_unit() : path.string(),
          select ? DatabaseSelection::Exact : DatabaseSelection::ExactOrUnique);
      profile.insert(profile.end(), configuration.arguments.begin(), configuration.arguments.end());
    }
    profile.insert(profile.begin(), {"-I", root.string(), "-I", (root / "include").string()});
    profile.insert(profile.end(), flags.begin(), flags.end());
    units.push_back({{path, file->second, tracer::kEmbeddedPluginHeader, sdk,
                      tracer::kEmbeddedCppPluginHeader, files, tracer::kEmbeddedAgentExtensionHeader},
                     std::move(profile)});
  }
  auto& plugins = sessions.try_emplace(request.session_id, true).first->second;
  return plugins.load_package(request.plugin_id, units, {}, cache_directory(), reload);
}

int agent_plugin(tracer::AgentPluginCall *request, const char *source,
                 const char *offer_bytes, std::size_t offer_size) noexcept {
  if (!request || request->version != 1)
    return -1;
  try {
    if (agent_pid != ::getpid())
      throw std::runtime_error(
          "plugin agent was inherited from another process");
    if (!terminated(request->session_id) || !request->session_id[0] ||
        !terminated(request->plugin_id) || !terminated(request->command) ||
        request->source_size > tracer::kMaxAgentPluginSource)
      throw std::invalid_argument("invalid agent plugin request");

    tracer::context_scheduler::ScopedPthreadBypass compiler_threads;
    std::lock_guard lock(plugin_mutex);
    struct RefreshObservers {
      ~RefreshObservers() {
        has_observers.store(std::any_of(sessions.begin(), sessions.end(), [](const auto& session) {
          return session.second.has_observers();
        }), std::memory_order_release);
      }
    } refresh;
    switch (request->operation) {
    case AgentPluginOperation::Load:
    case AgentPluginOperation::Reload: {
      if (!source || !request->source_size)
        throw std::invalid_argument("agent plugin source is required");
      auto profile = target_profile();
      auto &plugins = sessions.try_emplace(request->session_id, true).first->second;
      const bool reload = request->operation == AgentPluginOperation::Reload;
      const std::string_view contents(source, request->source_size);
      const auto cache = cache_directory();
      std::optional<tracer::NativePlugins::LoadResult> offered;
      clangtracer::v1::AgentPluginObject offer;
      if (offer_bytes && offer_size &&
          offer_size <= tracer::kMaxAgentPluginOffer &&
          offer.ParseFromArray(offer_bytes, static_cast<int>(offer_size)) &&
          !offer.object().empty() && absolute_path(offer.source_path()) &&
          absolute_path(offer.sdk_directory())) {
        // Mirror the REPL's driver arguments and file names, using the
        // target's own profile, headers and embedded SDK to check the key.
        auto offer_profile = profile;
        offer_profile.insert(offer_profile.begin(),
                             {"-I", offer.sdk_directory()});
        offered = plugins.load_offered(
            request->plugin_id,
            {offer.source_path(), contents, tracer::kEmbeddedPluginHeader,
             fs::path(offer.sdk_directory()) / "clang_tracer_repl_plugin.h",
             tracer::kEmbeddedCppPluginHeader, {}, tracer::kEmbeddedAgentExtensionHeader},
            offer_profile, cache, reload,
            {offer.fingerprint(), offer.checksum(), offer.object()});
      }
      const auto result =
          offered ? *offered
                  : plugins.load(request->plugin_id,
                                 {"/__clang_tracer_plugin__/source.cpp",
                                  contents,
                                  tracer::kEmbeddedPluginHeader,
                                  {},
                                  tracer::kEmbeddedCppPluginHeader, {}, tracer::kEmbeddedAgentExtensionHeader},
                                 profile, cache, reload);
      request->generation = result.generation;
      request->cache_hit = result.cache_hit;
      return result.from_repl ? tracer::kAgentPluginReusedReplObject : 0;
    }
    case AgentPluginOperation::LoadPackage:
    case AgentPluginOperation::ReloadPackage: {
      const auto result = load_package(*request, source,
                                       request->operation == AgentPluginOperation::ReloadPackage);
      request->generation = result.generation;
      request->cache_hit = result.cache_hit;
      return 0;
    }
    case AgentPluginOperation::Run: {
      const auto session = sessions.find(request->session_id);
      if (session == sessions.end())
        throw std::invalid_argument("no agent plugins loaded for this session");
      request->value =
          session->second.run(request->plugin_id, request->command);
      return 0;
    }
    case AgentPluginOperation::Clear:
      sessions.erase(request->session_id);
      return 0;
    }
    throw std::invalid_argument("unknown agent plugin operation");
  } catch (const std::exception &error) {
    std::snprintf(request->error, sizeof(request->error), "%s", error.what());
    return -1;
  }
}

} // namespace

bool tracer::agent_has_observers() noexcept {
  return has_observers.load(std::memory_order_acquire) && agent_pid == ::getpid();
}

bool tracer::agent_has_observers(std::string_view id) {
  if (!agent_has_observers())
    return false;
  std::shared_lock lock(plugin_mutex);
  const auto found = sessions.find(id);
  return found != sessions.end() && found->second.has_observers();
}

void tracer::agent_notify_return(const std::string& id, const agent::ReturnSample& sample,
                                 void* context, PluginEventPublisher publish) noexcept {
  // A hot probe never waits while the injector compiles or replaces a generation.
  std::shared_lock lock(plugin_mutex, std::try_to_lock);
  if (lock.owns_lock()) {
    const auto found = sessions.find(id);
    if (found != sessions.end())
      found->second.notify_return(sample, context, publish);
  }
}

extern "C" int tracer_agent_plugin_v1(tracer::AgentPluginCall *request,
                                      const char *source) noexcept {
  return agent_plugin(request, source, nullptr, 0);
}

extern "C" int tracer_agent_plugin_v2(tracer::AgentPluginCall *request,
                                      const char *source, const char *offer,
                                      std::size_t offer_size) noexcept {
  return agent_plugin(request, source, offer, offer_size);
}

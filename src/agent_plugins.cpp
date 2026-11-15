#include "agent_plugin_protocol.hpp"

#include "callback_source.hpp"
#include "context_scheduler.hpp"
#include "native_plugins.hpp"
#include "runtime_paths.hpp"
#include "tracer.pb.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using tracer::AgentPluginOperation;

const pid_t agent_pid = ::getpid();
std::mutex plugin_mutex;
std::map<std::string, tracer::NativePlugins> sessions;

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
    switch (request->operation) {
    case AgentPluginOperation::Load:
    case AgentPluginOperation::Reload: {
      if (!source || !request->source_size)
        throw std::invalid_argument("agent plugin source is required");
      auto profile = tracer::runtime::jit_arguments();
      profile.insert(profile.begin(),
                     {"-std=c++23", "-O0", "-fPIC", "-resource-dir",
                      tracer::runtime::clang_resource_dir()});
      auto &plugins = sessions.try_emplace(request->session_id).first->second;
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
        std::swap(offer_profile[1], offer_profile[2]);
        offer_profile.insert(offer_profile.begin(),
                             {"-I", offer.sdk_directory()});
        offered = plugins.load_offered(
            request->plugin_id,
            {offer.source_path(), contents, tracer::kEmbeddedPluginHeader,
             fs::path(offer.sdk_directory()) / "clang_tracer_repl_plugin.h",
             tracer::kEmbeddedCppPluginHeader},
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
                                  tracer::kEmbeddedCppPluginHeader},
                                 profile, cache, reload);
      request->generation = result.generation;
      request->cache_hit = result.cache_hit;
      return result.from_repl ? tracer::kAgentPluginReusedReplObject : 0;
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

extern "C" int tracer_agent_plugin_v1(tracer::AgentPluginCall *request,
                                      const char *source) noexcept {
  return agent_plugin(request, source, nullptr, 0);
}

extern "C" int tracer_agent_plugin_v2(tracer::AgentPluginCall *request,
                                      const char *source, const char *offer,
                                      std::size_t offer_size) noexcept {
  return agent_plugin(request, source, offer, offer_size);
}

#pragma once

#include <string>

#define CLANG_TRACER_AGENT_EXTENSION_HOST
#include "repl/clang_tracer_agent_extension.hpp"
#undef CLANG_TRACER_AGENT_EXTENSION_HOST

namespace tracer {
using PluginEventPublisher =
    bool (*)(void*, const std::string&, agent::LatencyEvent) noexcept;

bool agent_has_observers() noexcept;
void agent_notify_return(const std::string& session, const agent::ReturnSample& sample,
                         void* context, PluginEventPublisher publish) noexcept;
} // namespace tracer

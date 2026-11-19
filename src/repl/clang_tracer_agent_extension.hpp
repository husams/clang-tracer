#pragma once

#include <cstdint>

namespace tracer::agent {

enum class Status : std::uint32_t { ok, denied, unsupported };

struct ReturnSample {
  std::uint64_t probe_id;
  std::uint64_t elapsed_ns;
};

struct LatencyEvent {
  std::uint64_t probe_id;
  std::uint64_t elapsed_ns;
};

namespace detail { struct Host; }

class EventSink {
public:
  bool try_emit(LatencyEvent event) noexcept { return emit_(context_, event); }

private:
  friend struct detail::Host;
  using Emit = bool (*)(void*, LatencyEvent) noexcept;
  EventSink(void* context, Emit emit) noexcept : context_(context), emit_(emit) {}
  void* context_;
  Emit emit_;
};

namespace detail {
using Observer = void (*)(const ReturnSample&, EventSink&) noexcept;
template <typename Implementation> struct Adapter;
}

class Attachment {
public:
  template <auto Method>
  Status observe_returns() noexcept;

private:
  friend struct detail::Host;
  explicit Attachment(detail::Observer& observer) noexcept : observer_(observer) {}
  detail::Observer& observer_;
};

namespace detail {
struct Host {
  static Attachment attachment(Observer& observer) noexcept { return Attachment(observer); }
  static EventSink event_sink(void* context, EventSink::Emit emit) noexcept {
    return EventSink(context, emit);
  }
};

using Attach = Status (*)(Attachment&) noexcept;
} // namespace detail
} // namespace tracer::agent

#ifndef CLANG_TRACER_AGENT_EXTENSION_HOST

#include "clang_tracer_repl_plugin.hpp"

namespace tracer::agent {
namespace detail {
template <typename Implementation>
struct Adapter : repl::PluginInterface<Implementation> {
  template <auto Method>
  static void observe(const ReturnSample& sample, EventSink& sink) noexcept {
    static_assert(noexcept((this_instance().*Method)(sample, sink)),
                  "agent observers must be noexcept");
    (this_instance().*Method)(sample, sink);
  }

  static Status attach(Attachment& attachment) noexcept {
    static_assert(noexcept(this_instance().on_attach(attachment)),
                  "agent on_attach must be noexcept");
    return this_instance().on_attach(attachment);
  }

private:
  static Implementation& this_instance() noexcept {
    return repl::PluginInterface<Implementation>::instance();
  }
};
} // namespace detail

template <auto Method>
Status Attachment::observe_returns() noexcept {
  if (observer_)
    return Status::denied;
  observer_ = &detail::Adapter<repl::Plugin>::template observe<Method>;
  return Status::ok;
}

namespace detail {
template <typename Implementation>
Status attach_extension(Attachment& attachment) noexcept {
  return Adapter<Implementation>::attach(attachment);
}
} // namespace detail
} // namespace tracer::agent

extern "C" [[gnu::visibility("default")]] tracer::agent::Status
tracer_agent_extension_v1(tracer::agent::Attachment& attachment) noexcept {
  return tracer::agent::detail::attach_extension<tracer::repl::Plugin>(attachment);
}

#endif

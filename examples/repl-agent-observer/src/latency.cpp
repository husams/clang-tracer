#include "clang_tracer_agent_extension.hpp"

#include <atomic>

namespace agent = tracer::agent;

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  std::atomic<std::uint64_t> completed_{0};

public:
  agent::Status on_attach(agent::Attachment& attachment) noexcept {
    return attachment.observe_returns<&Plugin::on_return>();
  }

  void on_return(const agent::ReturnSample& sample, agent::EventSink& events) noexcept {
    completed_.fetch_add(1, std::memory_order_relaxed);
    (void)events.try_emit({sample.probe_id, sample.elapsed_ns});
  }

  std::uint64_t completed() const noexcept {
    return completed_.load(std::memory_order_relaxed);
  }

  static constexpr Command commands[] = {
      command<&Plugin::completed>("completed", "Count observed probe returns."),
  };
};

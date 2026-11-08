#pragma once

#include <cstddef>
#include <memory>

#include "context_stream.hpp"
#include "tracer/compat.hpp"
#include "tracer/protocol.hpp"

namespace tracer::context_delivery {

// One exporter is shared by every probe in the injected producer session.
// Hook callers hand it already serialized ContextBatch bytes; this API only
// copies into preallocated process storage and never constructs protobufs.
class ContextAgentDelivery final {
public:
  static std::shared_ptr<ContextAgentDelivery> acquire(const Request &request);
  ~ContextAgentDelivery();
  ContextAgentDelivery(const ContextAgentDelivery &) = delete;
  ContextAgentDelivery &operator=(const ContextAgentDelivery &) = delete;

  bool try_publish_capture(::tracer::compat::span<const std::byte> event_record,
                           const std::shared_ptr<const void>& schema_lease) noexcept;
  ContextHookRing &hook_ring() noexcept;
  bool ready() const noexcept;
  bool paused() const noexcept;
  ExportCounters counters() const noexcept;

private:
  explicit ContextAgentDelivery(const Request &request);
  std::shared_ptr<ContextJournal> journal_;
  std::unique_ptr<ContextExportClient> client_;
};

} // namespace tracer::context_delivery

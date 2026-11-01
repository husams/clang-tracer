#pragma once

#include <functional>
#include <memory>

#include <grpcpp/grpcpp.h>

#include "context_receiver.hpp"
#include "context_stream.hpp"

namespace tracer::context_delivery {

// Receiver-backed gRPC service adapter. It creates a fresh schema receiver for
// each Export epoch while sharing the bounded descriptor/decoded-state ledger.
class ContextCollector final {
public:
  using DurableStoreFactory = ContextExportService::DurableStoreFactory;
  // The envelope owns the exact event bytes and every descriptor used by the
  // event, so sinks may retain it without holding receiver/JIT leases.
  using EventSink = std::function<bool(const clangtracer::context::v1::ExportOpen&,
                                       const clangtracer::context::v1::ContextEventEnvelope&,
                                       std::shared_ptr<const context_engine::DecodedEvent>)>;

  explicit ContextCollector(std::shared_ptr<ContextCollectorStore> durable_store = {},
                            StreamLimits limits = {}, EventSink sink = {},
                            DurableStoreFactory durable_store_factory = {});
  ContextCollector(const ContextCollector&) = delete;
  ContextCollector& operator=(const ContextCollector&) = delete;

  grpc::Service* service() noexcept { return &service_; }

  std::shared_ptr<context_engine::ReceiverLedger> receiver_ledger() const noexcept {
    return receiver_ledger_;
  }

private:
  std::shared_ptr<context_engine::ReceiverLedger> receiver_ledger_;
  ContextExportService service_;
};

} // namespace tracer::context_delivery

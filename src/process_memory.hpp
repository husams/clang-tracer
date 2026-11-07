#pragma once

#include <chrono>

#include "tracer.grpc.pb.h"

namespace tracer {
class Session;

namespace memory {
grpc::Status list_mappings(grpc::ServerContext &context, Session &session,
                           std::chrono::seconds timeout,
                           clangtracer::v1::ListMemoryMappingsResponse &response);
grpc::Status read(grpc::ServerContext &context, Session &session,
                  std::chrono::seconds timeout,
                  const clangtracer::v1::ReadMemoryRequest &request,
                  clangtracer::v1::ReadMemoryResponse &response);
grpc::Status scan(grpc::ServerContext &context, Session &session,
                  std::chrono::seconds timeout,
                  const clangtracer::v1::ScanMemoryRequest &request,
                  clangtracer::v1::ScanMemoryResponse &response);
} // namespace memory
} // namespace tracer

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "tracer.grpc.pb.h"

namespace tracer {
class Session;

namespace memory {
std::vector<std::pair<std::uint64_t, std::uint64_t>>
readonly_image_data(Session &session, const std::string &image, std::chrono::seconds timeout);
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
grpc::Status apply_patch(grpc::ServerContext &context, Session &session,
                         std::chrono::seconds timeout,
                         const clangtracer::v1::ApplyMemoryPatchRequest &request,
                         clangtracer::v1::MemoryPatch &response);
grpc::Status start_page_monitor(grpc::ServerContext &context, Session &session,
                                std::chrono::seconds timeout,
                                const clangtracer::v1::StartPageMonitorRequest &request,
                                clangtracer::v1::PageMonitorResult &response);
grpc::Status start_instruction_trace(
    grpc::ServerContext &context, Session &session, std::chrono::seconds timeout,
    const clangtracer::v1::StartInstructionTraceRequest &request,
    clangtracer::v1::InstructionTrace &response);
grpc::Status preview_entry(grpc::ServerContext &context, Session &session,
                           std::chrono::seconds timeout,
                           const clangtracer::v1::PreviewFunctionEntryRequest &request,
                           clangtracer::v1::PreviewFunctionEntryResponse &response);
} // namespace memory
} // namespace tracer

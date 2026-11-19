#pragma once

#include "live_update.hpp"
#include "tracer.pb.h"

namespace tracer::live {

struct AgentRequest;
struct JitRequest;

JitRequest decode_jit(const clangtracer::v1::LiveJitInput &input);
struct EntryImage {
  std::uintptr_t address, mapping_begin;
  std::size_t mapping_size;
};

EntryImage describe_entry(JitImage &image, std::string_view symbol);
std::uint64_t retain_image(std::shared_ptr<JitImage> image, std::uint64_t limit);
std::uint64_t retained_image_count();
void execute_extension(AgentRequest &request);

} // namespace tracer::live

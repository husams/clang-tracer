#pragma once

#include "tracer.pb.h"

namespace tracer::live {

struct AgentRequest;
struct JitRequest;

JitRequest decode_jit(const clangtracer::v1::LiveJitInput &input);
void execute_extension(AgentRequest &request);

} // namespace tracer::live

#pragma once
#include <string_view>

#include "tracer.pb.h"
#include "tracer/protocol.hpp"

namespace tracer {
void begin_native_stack_session(std::string_view session_id);
void capture_native_stack(const TraceContext &context, unsigned depth,
                          bool application_only) noexcept;
void release_native_stack(const TraceContext &context) noexcept;
void serialize_native_stack(const StackTrace &stack,
                            clangtracer::v1::StackTrace &output);
} // namespace tracer

extern "C" void tracer_stack_enter(tracer::TraceContext *, void *) noexcept;
extern "C" void tracer_stack_leave(tracer::TraceContext *) noexcept;

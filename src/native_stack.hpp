#pragma once
#include "tracer/compat.hpp"
#include <array>
#include <string_view>

#include "tracer/protocol.hpp"

#if defined(__x86_64__)
#include "tracer.pb.h"
#endif

namespace tracer {
#if defined(__x86_64__)
void begin_native_stack_session(std::string_view session_id);
void capture_native_stack(const TraceContext &context, unsigned depth,
                          bool application_only) noexcept;
void release_native_stack(const TraceContext &context) noexcept;
void serialize_native_stack(const StackTrace &stack,
                            clangtracer::v1::StackTrace &output);
#endif
struct NativeCodeStack {
  unsigned count = 0;
  StackStatus status = StackStatus::Partial;
};
#if defined(__aarch64__)
using NativeCodeRegisters = std::array<std::uint64_t, 33>;
inline std::uint64_t strip_return_signature(std::uint64_t pc) noexcept {
  // XPACLRI is a HINT on machines without pointer authentication.
  asm volatile("mov x16, x30\n"
               "mov x30, %0\n"
               "hint #7\n"
               "mov %0, x30\n"
               "mov x30, x16" : "+r"(pc) : : "x16", "x30");
  return pc;
}
#else
using NativeCodeRegisters = std::array<std::uint64_t, 17>;
#endif
NativeCodeStack capture_native_code_stack(const NativeCodeRegisters &registers,
    ::tracer::compat::span<std::uint64_t> pcs, ::tracer::compat::span<std::uint64_t> stack_pointers) noexcept;
} // namespace tracer

#if defined(__x86_64__)
extern "C" void tracer_stack_enter(tracer::TraceContext *, void *) noexcept;
extern "C" void tracer_stack_leave(tracer::TraceContext *) noexcept;
#endif

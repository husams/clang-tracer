#include <cstring>

#include "live_mock_context.hpp"

char* mock_status(const LiveMockContext& ctx) {
  constexpr char reply[] = "mock ready";
  auto* out = static_cast<char*>(ctx.target_allocate(sizeof(reply)));
  if (out) {
    std::memcpy(out, reply, sizeof(reply));
  }
  return out;
}

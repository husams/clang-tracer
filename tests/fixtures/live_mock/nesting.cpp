#include <stdexcept>

#include "live_mock_context.hpp"

int mock_nesting(const LiveMockContext& ctx, int depth) {
  if (depth < 0) {
    throw std::runtime_error("mock failure");
  }
  return ctx.original(depth) + 10;
}

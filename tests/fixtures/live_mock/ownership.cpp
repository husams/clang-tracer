#include <cstring>

#include "application.hpp"
#include "live_mock_context.hpp"

Quote* make_test_quote(const LiveMockContext& ctx, Pricer& self, int product) {
  if (!ctx.is_test_product(self, product)) {
    return ctx.original(self, product);
  }
  return ctx.quote_create(product, 125);
}

// Each snapshot provides only the typed context for its selected callable.

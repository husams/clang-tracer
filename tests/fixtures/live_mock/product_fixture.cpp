#include <iomanip>
#include <sstream>

#include "application.hpp"
#include "live_mock_context.hpp"

std::optional<Product> load_fixture(const LiveMockContext& ctx, ProductStore&, int id) {
  std::istringstream rows{std::string(ctx.fixture_text())};
  Product product{};
  while (rows >> product.id >> std::quoted(product.name) >> product.price_cents) {
    if (product.id == id) {
      return product;
    }
  }
  return std::nullopt;
}

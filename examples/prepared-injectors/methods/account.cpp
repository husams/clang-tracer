#include "account.hpp"

#include <numeric>
#include <utility>

namespace prepared_demo {
__attribute__((noinline)) Receipt Account::apply(const std::string &label,
                                                 std::vector<int> parts,
                                                 Adjustment adjustment,
                                                 std::unique_ptr<int> units,
                                                 const std::shared_ptr<int> &factor,
                                                 const std::weak_ptr<int> &observer,
                                                 Choice choice) {
  last_adjustment = adjustment;
  balance += std::accumulate(parts.begin(), parts.end(), adjustment.delta);
  if (units && factor && !observer.expired() && choice.index() == 2)
    balance += *units * *factor;
  return {label, std::move(parts), balance};
}
} // namespace prepared_demo

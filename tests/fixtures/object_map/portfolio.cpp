#include "portfolio.hpp"

#include <cstdio>
#include <unistd.h>

namespace demo {
__attribute__((noinline)) void replace_risk(RiskMap &risk) {
  auto old = risk.extract("AAPL");
  risk.emplace("AAPL", Risk{{70}});
}

__attribute__((noinline)) void Portfolio::rebalance() {
  auto &before = risk.at("AAPL").exposure.limit;
  std::printf("before replace limit_addr=%p limit=%d\n", static_cast<void *>(&before), before);
  before = 60;
  replace_risk(risk);
  auto &after = risk.at("AAPL").exposure.limit;
  std::printf("after replace limit_addr=%p limit=%d\n", static_cast<void *>(&after), after);
  after = 80;
  std::printf("after write limit_addr=%p limit=%d\n", static_cast<void *>(&after), after);
}
}

int main() {
  demo::Portfolio portfolio{{{"AAPL", {{50}}}, {"MSFT", {{100}}}, {"AMD", {{200}}}}};
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  while (::access("rebalance.now", F_OK))
    ::usleep(1000);
  portfolio.rebalance();
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return portfolio.risk.at("AAPL").exposure.limit != 80 ||
         portfolio.risk.at("MSFT").exposure.limit != 100;
}

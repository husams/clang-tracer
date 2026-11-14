#include "portfolio.hpp"

#include <cstdio>
#include <unistd.h>

namespace demo {
__attribute__((noinline)) void replace_risk(RiskMap &risk) {
#ifdef TRACER_TEST_HASH_MAP
  risk.rehash(risk.bucket_count() * 3);
#else
  auto old = risk.extract(selected_key);
  risk.emplace(selected_key, Risk{{limit_base + 70}});
#endif
}

__attribute__((noinline)) void Portfolio::rebalance() {
  auto &before = risk.at(selected_key).exposure.limit;
  std::printf("before replace limit_addr=%p limit=%llu\n", static_cast<void *>(&before),
              static_cast<unsigned long long>(before));
  before = limit_base + 60;
  replace_risk(risk);
  auto &after = risk.at(selected_key).exposure.limit;
  std::printf("after replace limit_addr=%p limit=%llu\n", static_cast<void *>(&after),
              static_cast<unsigned long long>(after));
  after = limit_base + 80;
  std::printf("after write limit_addr=%p limit=%llu\n", static_cast<void *>(&after),
              static_cast<unsigned long long>(after));
}
}

int main() {
  demo::Portfolio portfolio{{{demo::selected_key, {{demo::limit_base + 50}}},
                             {demo::other_key, {{100}}}, {demo::spare_key, {{200}}}}};
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  while (::access("rebalance.now", F_OK))
    ::usleep(1000);
  portfolio.rebalance();
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return portfolio.risk.at(demo::selected_key).exposure.limit != demo::limit_base + 80 ||
         portfolio.risk.at(demo::other_key).exposure.limit != 100;
}

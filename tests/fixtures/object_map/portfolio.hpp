#pragma once

#include <map>
#include <string>

namespace demo {
struct Risk { struct Exposure { int limit; } exposure; };
using RiskMap = std::map<std::string, Risk>;

struct Portfolio {
  RiskMap risk;
  void rebalance();
};

void replace_risk(RiskMap &risk);
}

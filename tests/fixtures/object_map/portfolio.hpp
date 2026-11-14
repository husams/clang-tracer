#pragma once

#include <map>
#include <string>
#include <cstdint>
#ifdef TRACER_TEST_HASH_MAP
#include <unordered_map>
#endif

namespace demo {
#ifdef TRACER_TEST_WIDE_FIELD
using Limit = std::uint64_t;
inline constexpr Limit limit_base = UINT64_C(1) << 40;
#else
using Limit = int;
inline constexpr Limit limit_base = 0;
#endif
struct Risk { struct Exposure { Limit limit; } exposure; };
#ifdef TRACER_TEST_HASH_MAP
using RiskMap = std::unordered_map<int, Risk>;
inline constexpr int selected_key = -7;
inline constexpr int other_key = 2;
inline constexpr int spare_key = 3;
#else
using RiskMap = std::map<std::string, Risk>;
inline constexpr char selected_key[] = "AAPL";
inline constexpr char other_key[] = "MSFT";
inline constexpr char spare_key[] = "AMD";
#endif

struct Portfolio {
  RiskMap risk;
  void rebalance();
};

void replace_risk(RiskMap &risk);
}

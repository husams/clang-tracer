#pragma once

#include <map>
#include <string>
#include <vector>

namespace shop {

struct Order {
  int id;
  int quantity;
  double unit_price;
};

// A struct by reference, a string, a vector and a map, all in one call.
double price(const Order& order, const std::string& coupon,
             const std::vector<int>& fees,
             const std::map<std::string, int>& stock);

} // namespace shop

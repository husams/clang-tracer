#include "types.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
const auto &event = data.at(0);
auto groups = event.argument_as<std::unordered_map<int, std::vector<int>>>(0);
if (groups != value_examples::Groups{{2, {2, 4}}, {0, {}}} ||
    !event.result_as<bool>())
  throw std::runtime_error("unordered_map value mismatch");
std::cout << "unordered_map[2][1]=" << groups.at(2).at(1)
          << " empty mapped vector size=" << groups.at(0).size() << std::endl;
std::cout << "example passed: unordered_map" << std::endl;

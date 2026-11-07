#include "types.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
const auto &event = data.at(0);
auto ids = event.argument_as<std::set<int>>(0);
if (ids != value_examples::SortedIds{2, 4, 6} || !event.result_as<bool>())
  throw std::runtime_error("set value mismatch");
std::cout << "set.count(4)=" << ids.count(4) << std::endl;
std::cout << "example passed: set" << std::endl;

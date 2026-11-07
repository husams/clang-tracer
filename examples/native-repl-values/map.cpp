#include "types.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
const auto &event = data.at(0);
auto counts = event.argument_as<std::map<std::string, int>>(0);
if (counts != value_examples::Counts{{"apples", 2}, {"pears", 4}} ||
    !event.result_as<bool>())
  throw std::runtime_error("map value mismatch");
std::cout << "map[apples]=" << counts.at("apples") << std::endl;
std::cout << "example passed: map" << std::endl;

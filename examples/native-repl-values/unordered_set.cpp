#include "types.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
const auto &event = data.at(0);
auto labels = event.argument_as<std::unordered_set<std::string>>(0);
if (labels != value_examples::Labels{"red", "blue"} || !event.result_as<bool>())
  throw std::runtime_error("unordered_set value mismatch");
std::cout << "unordered_set.count(red)=" << labels.count("red") << std::endl;
std::cout << "example passed: unordered_set" << std::endl;

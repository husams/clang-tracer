#include "types.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
bool saw_present = false, saw_absent = false;
for (const auto &event : data) {
  bool enabled = event.argument_as<bool>(0);
  int count = event.argument_as<int>(1);
  double ratio = event.argument_as<double>(2);
  auto text = event.argument_as<std::string>(3);
  auto serial = event.argument_as<std::optional<int>>(4);
  auto returned = event.result_as<std::string>();
  if (!enabled || count != 3 || ratio != 1.25 || text != "hello" ||
      returned != "hello-returned" || (serial && *serial != 0))
    throw std::runtime_error("simple value mismatch");
  saw_present = saw_present || serial.has_value();
  saw_absent = saw_absent || !serial.has_value();
  std::cout << std::boolalpha << "bool=" << enabled << " int=" << count
            << " double=" << ratio << " string=" << text
            << " optional=" << (serial ? std::to_string(*serial) : "none")
            << " returned=" << returned << std::endl;
}
if (!saw_present || !saw_absent)
  throw std::runtime_error("expected both optional{0} and nullopt");
std::cout << "example passed: simple" << std::endl;

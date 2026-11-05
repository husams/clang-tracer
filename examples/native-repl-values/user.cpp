// REPL source: loaded with %cpp load, without a main() function.
#include "mappings.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
const auto &event = data.at(0);
auto address = event.argument_as<value_examples::Address>(0);
auto before = event.argument_as<value_examples::Account>(1);
auto after = event.result_as<value_examples::Account>();
if (address.city != "Paris" || before.id != 7 || after.id != 8 ||
    before.address.city != "London" || after.address.city != "Paris")
  throw std::runtime_error("user-defined value mismatch");
std::cout << "struct Address: " << address.city << ", " << address.postal_code
          << std::endl;
std::cout << "class Account before: " << before.describe() << std::endl;
std::cout << "class Account returned: " << after.describe() << std::endl;

// Inspect fields directly without reconstructing a user-defined C++ type.
auto captured = event.argument_snapshot(1);
std::cout << "field inspection: " << captured.field("owner").as<std::string>()
          << ", " << captured.field("address").field("city").as<std::string>()
          << std::endl;
std::cout << "example passed: user" << std::endl;

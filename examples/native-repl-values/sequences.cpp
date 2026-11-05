#include "mappings.hpp"
#include <iostream>
#include <stdexcept>

auto data = tracer.batch("demo");
const auto &event = data.at(0);
auto coordinates = event.argument_as<std::array<int, 3>>(0);
auto addresses = event.argument_as<std::vector<value_examples::Address>>(1);
auto numbers = event.argument_as<std::list<int>>(2);
auto messages = event.argument_as<std::deque<std::string>>(3);
auto matrix = event.argument_as<std::vector<std::vector<int>>>(4);
if (coordinates != value_examples::Coordinates{2, 4, 6} ||
    addresses.size() != 2 || addresses.at(0).city != "Paris" ||
    addresses.at(1).postal_code != 150 ||
    numbers != value_examples::Numbers{3, 5, 7} ||
    messages != value_examples::Messages{"queued", "done"} ||
    matrix != value_examples::Matrix{{1, 2}, {}, {3}} ||
    !event.result_as<bool>())
  throw std::runtime_error("sequence value mismatch");
std::cout << "array[2]=" << coordinates.at(2)
          << " vector<Address>[0].city=" << addresses.at(0).city
          << " list.front=" << numbers.front()
          << " deque.front=" << messages.front()
          << " nested vector[0][1]=" << matrix.at(0).at(1)
          << " empty inner vector size=" << matrix.at(1).size() << std::endl;
std::cout << "example passed: sequences" << std::endl;

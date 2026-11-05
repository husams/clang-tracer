#include "event_ranges.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace tracer_repl;
namespace {
void require(bool ok) {
  if (!ok)
    throw std::runtime_error("range adaptor check failed");
}
template <class Range> std::vector<int> values(const Range &range) {
  std::vector<int> result;
  for (const auto &value : range)
    result.push_back(value);
  return result;
}
} // namespace
int main() {
  try {
    const std::vector<int> input{1, 2, 3, 4, 5, 6};
    require(values(input | take(2)) == std::vector<int>({1, 2}));
    require(values(input | take(20)) == input);
    require(values(input | take(0)).empty());
    require(values(input | drop(2) | take(2)) == std::vector<int>({3, 4}));
    require(values(input | drop(20)).empty());
    require(values(input | drop(0)) == input);
    require(values(input | filter([](int n) { return n % 2 == 0; }) |
                   take(2)) == std::vector<int>({2, 4}));
    require(values(input | take_while([](int n) { return n < 4; })) ==
            std::vector<int>({1, 2, 3}));
    require(values(input | take_while([](int) { return false; })).empty());
    require(values(input | take_while([](int) { return true; })) == input);
    require(values(input | filter([](int) { return false; })).empty());
    require(!(std::vector<int>{} | first()));
    require((input | first()).value() == 1);
    require((input | drop(3) | first()).value() == 4);
    auto owned = std::vector<int>{7, 8, 9} | take(2);
    require(values(owned) == std::vector<int>({7, 8}));
    require(values(owned) == std::vector<int>({7, 8}));
    auto copied = owned;
    require(values(copied) == values(owned));
    unsigned calls = 0;
    auto lazy = input | filter([&](int) {
                  ++calls;
                  return true;
                }) |
                take(2);
    require(calls == 0);
    require(values(lazy) == std::vector<int>({1, 2}) && calls == 2);
    calls = 0;
    require(values(lazy | take(0)).empty() && calls == 0);
    auto prefix = input | take_while([&](int n) {
                    ++calls;
                    return n < 3;
                  });
    require(values(prefix) == std::vector<int>({1, 2}) && calls == 3);
    for (bool taking : {false, true}) {
      bool rejected = false;
      try {
        if (taking)
          (void)take(-1);
        else
          (void)drop(-1);
      } catch (const std::invalid_argument &) {
        rejected = true;
      }
      require(rejected);
    }
    require(
        values(std::vector<int>{} | filter([](int) { return true; }) | take(3))
            .empty());
    require(values(input | transform([](int n) { return n * 10; }) | take(2)) ==
            std::vector<int>({10, 20}));
    require(values(input | drop_while([](int n) { return n < 4; })) ==
            std::vector<int>({4, 5, 6}));
    require(values(input | drop_while([](int) { return true; })).empty());
    require(values(input | drop_while([](int) { return false; })) == input);
    require(values(input | stride(2)) == std::vector<int>({1, 3, 5}));
    require(values(input | stride(20)) == std::vector<int>({1}));
    require(values(input | stride(1)) == input);
    for (int step : {0, -1}) {
      bool rejected = false;
      try {
        (void)stride(step);
      } catch (const std::invalid_argument &) {
        rejected = true;
      }
      require(rejected);
    }
    std::size_t index = 0;
    for (const auto &[i, n] : input | drop(1) | enumerate()) {
      require(i == index++ && n == static_cast<int>(i + 2));
    }
    require(index == 5);
    require((input | drop(1) | take(2) | to_vector()) ==
            std::vector<int>({2, 3}));
    require((input | reverse()) == std::vector<int>({6, 5, 4, 3, 2, 1}));
    require(values(input | reverse() | take(2)) == std::vector<int>({6, 5}));
    require((std::vector<int>{} | reverse()).empty());
    require(
        (input | transform([](int n) { return std::to_string(n); }) | first())
            .value() == "1");
    calls = 0;
    auto mapped = input | transform([&](int n) {
                    ++calls;
                    return n;
                  });
    require(calls == 0 && values(mapped | take(0)).empty() && calls == 0);
    require(values(mapped | take(2)).size() == 2 && calls == 2);
    std::cout << "range adaptor checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

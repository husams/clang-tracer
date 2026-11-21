#include "repl_std_headers.hpp"

namespace stdh {

Record::Record() = default;
std::string Record::label() const { return "rec" + std::to_string(n_); }
int Record::count() const { return static_cast<int>(n_); }

int inspect(const Probe &gadget) { return gadget.total + (gadget.second.value ? 1 : 0); }
Probe make_gadget() { return Probe{"gadget", Wrapper<const char *>{"inner"}}; }

std::string make_label(int n) { return "label" + std::to_string(n); }

std::vector<Person> make_people(int n) {
  std::vector<Person> people;
  for (int i = 0; i < n; ++i)
    people.push_back({i + 1, i * 1.5});
  return people;
}

int sum_people(const std::vector<Person> &people) {
  int total = 0;
  for (const auto &person : people)
    total += person.id;
  return total;
}

std::array<int, 4> make_array(int base) { return {base, base + 1, base + 2, base + 3}; }

std::map<int, int, std::greater<int>> make_ordered(int n) {
  std::map<int, int, std::greater<int>> ordered;
  for (int i = 0; i < n; ++i)
    ordered[i] = i * i;
  return ordered;
}

std::vector<std::pair<int, std::string>> make_pairs(int n) {
  std::vector<std::pair<int, std::string>> pairs;
  for (int i = 0; i < n; ++i)
    pairs.emplace_back(i, "p" + std::to_string(i));
  return pairs;
}

double midpoint(const std::uniform_real_distribution<double> &range) {
  return (range.a() + range.b()) / 2;
}

}  // namespace stdh

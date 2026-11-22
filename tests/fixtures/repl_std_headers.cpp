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

std::vector<long> make_longs(int n) {
  std::vector<long> values;
  for (int i = 0; i < n; ++i)
    values.push_back(1000L * (i + 1));
  return values;
}

std::vector<std::size_t> make_sizes(int n) {
  std::vector<std::size_t> values;
  for (int i = 0; i < n; ++i)
    values.push_back(static_cast<std::size_t>(i) + 7);
  return values;
}

std::size_t view_size(std::string_view text) { return text.size(); }

std::chrono::milliseconds make_delay(int n) { return std::chrono::milliseconds(n * 10); }

std::chrono::system_clock::time_point make_stamp(int n) {
  return std::chrono::system_clock::time_point(std::chrono::seconds(n));
}

long call_twice(const std::function<long(long)> &function, long value) {
  return function(function(value));
}

std::vector<Mode> make_modes(int n) {
  return std::vector<Mode>(static_cast<std::size_t>(n), Mode::Fast);
}

std::optional<Level> make_level(int n) {
  return n % 2 ? std::optional<Level>(High) : std::optional<Level>(Low);
}

std::vector<Named> make_named(int n) {
  std::vector<Named> named;
  for (int i = 0; i < n; ++i)
    named.push_back({"n" + std::to_string(i), i});
  return named;
}

std::optional<Named> find_named(int rank) { return Named{"found", rank}; }

long ratio_den(const std::ratio<1, 200> &ratio) { return ratio.den; }

int sum_array(const std::unique_ptr<int[]> &values, int count) {
  int total = 0;
  for (int i = 0; i < count; ++i)
    total += values[static_cast<std::size_t>(i)];
  return total;
}

int grid_sum(const std::array<int[2][3], 1> &grid) {
  int total = 0;
  for (const auto &row : grid[0])
    for (const int cell : row)
      total += cell;
  return total;
}

int apply_all(const std::map<int, int (*)(int)> &handlers, int value) {
  for (const auto &handler : handlers)
    value = handler.second(value);
  return value;
}

}  // namespace stdh

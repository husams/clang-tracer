#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace repl_fixture {
struct Location {
  int code = 13;
  std::string name = "nested";
};
class Record {
public:
  int id = 7;
  std::string label = "heap-backed snapshot label longer than SSO";
  Location location;
  std::vector<int> samples{1, 3, 5};
  std::map<std::string, int> tags{{"", 0}, {"key", 2}};
  std::optional<int> serial{0};
};
bool string_argument(const std::string &);
std::string string_result(const std::string &);
bool mutate_argument(Record &);
Record round_trip(Record);
} // namespace repl_fixture

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace query_fixture {
struct Scope {};
struct Record {
  int id = 0;
  std::string label;
};
struct State : shared_state<Scope> {
  std::vector<std::vector<int>> matrix;
  std::map<std::string, std::vector<Record>> groups;
  std::optional<std::vector<Record>> recent;
};
} // namespace query_fixture

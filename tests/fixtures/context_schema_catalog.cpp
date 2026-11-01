#include <array>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace catalog {
struct Scope {};
enum class Mode { idle = 0, active = 1 };
struct Nested {
  Mode mode = Mode::idle;
  std::array<double, 2> fixed{};
};
struct CatalogState : shared_state<Scope> {
  std::deque<std::list<std::string>> queues;
  std::set<int> ids;
  std::unordered_set<bool> flags;
  std::unordered_map<std::string,
                     std::unordered_map<unsigned, std::optional<Nested>>>
      values;
};
} // namespace catalog

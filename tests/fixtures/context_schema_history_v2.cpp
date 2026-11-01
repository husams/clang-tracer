#include <cstdint>

namespace history {
struct Scope {};
struct State : shared_state<Scope> {
  std::uint64_t retained = 0;
  double changed = 0;
  bool added = false;
};
} // namespace history

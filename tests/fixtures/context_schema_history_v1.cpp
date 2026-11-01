#include <cstdint>
#include <string>

namespace history {
struct Scope {};
struct State : shared_state<Scope> {
  std::uint64_t retained = 0;
  std::string removed;
  std::int32_t changed = 0;
};
} // namespace history

#include <algorithm>
#include <stdexcept>

#include "state.hpp"

extern "C" void fixture_hook_observe(int);

namespace fixture {
bool after(CacheV2& cache) {
  const auto count = cache.entries.size();
  fixture_hook_observe(static_cast<int>(count));
#if defined(FAIL_INVALID_STATE)
  if (count == 1400) {
    cache.max_entries = 0;
    return true;
  }
#elif defined(FAIL_HOOK)
  if (count == 1400) {
#if defined(FIXTURE_REJECT_WITHOUT_THROW)
    return false;
#else
    throw std::runtime_error("second hook failed");
#endif
  }
#endif
  const auto reserve = cache.name == "search" ? 4096 : 256;
  cache.max_entries = std::max(cache.max_entries, count + reserve);
  ++cache.hook_calls;
  return true;
}
} // namespace fixture

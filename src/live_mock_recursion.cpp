#include "live_mock_recursion.hpp"

extern "C" void* clangtracer_live_mock_top_v1(void* value, bool set) noexcept {
  static thread_local void* top;
  if (set) {
    top = value;
  }
  return top;
}

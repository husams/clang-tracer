#include "item.hpp"

namespace HFW {
namespace {
struct Other final : Base {
  int y = 99;
};
}

Prefix::~Prefix() = default;
Base::~Base() = default;

__attribute__((noinline)) Base *HfwManager::createItem() {
  static Child child;
  static Other other;
  switch (next++ % 3) {
  case 0: return &child;
  case 1: return &other;
  default: return nullptr;
  }
}
} // namespace HFW

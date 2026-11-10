#include "item.hpp"

namespace prepared_pointee {
namespace {
HfwItem item{17, 42, "heap-backed HfwItem label", {7, true}};
} // namespace

__attribute__((noinline)) HfwItem *HfwManager::createItem(int id) {
  return id == 0 ? nullptr : &item;
}

} // namespace prepared_pointee

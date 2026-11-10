#include "item.hpp"

namespace prepared_pointee {
namespace {
HfwItem item = [] {
  HfwItem result;
  result.set_base_id(17);
  result.id = 42;
  result.label = "heap-backed HfwItem label";
  result.details = {7, true};
  return result;
}();
} // namespace

__attribute__((noinline)) HfwItem *HfwManager::createItem(int id) {
  return id == 0 ? nullptr : &item;
}

} // namespace prepared_pointee

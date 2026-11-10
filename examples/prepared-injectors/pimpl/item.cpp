#include "item-impl.hpp"

namespace HFW {

Item::Item(int value, ItemImp *implementation)
    : id(value), _imp(implementation) {}

Item::~Item() { delete _imp; }

HfwManager::HfwManager()
    : item_v1_(new Item(101, new ItemV1(1, "live ItemV1", {27, true}, 808))),
      item_v2_(new Item(202, new ItemV2(99))),
      item_null_imp_(new Item(303, nullptr)) {}

HfwManager::~HfwManager() = default;

__attribute__((noinline)) Item *HfwManager::createItem(int kind) {
  if (kind == 0)
    return nullptr;
  if (kind == 2)
    return item_v2_.get();
  if (kind == 3)
    return item_null_imp_.get();
  return item_v1_.get();
}

__attribute__((noinline)) Item &HfwManager::referenceItem(int kind) {
  if (kind == 1)
    return *item_v2_;
  if (kind == 2)
    return *item_null_imp_;
  return *item_v1_;
}

} // namespace HFW

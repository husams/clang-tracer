#include "item-impl.hpp"

namespace HFW {
namespace {
struct ItemV4 final : ItemImp {
  unsigned revision = 4;
  int source = 404;
};
} // namespace

Item::Item(int value, ItemImp *implementation)
    : id(value), _imp(implementation) {}

Item::~Item() { delete _imp; }

HfwManager::HfwManager()
    : item_v1_(new Item(101, new ItemV1(1, "live ItemV1", {27, true}, 808))),
      item_v2_(new Item(202, new ItemV2(2, "live ItemV2", {12, true}, 909))),
      item_v3_(new Item(303, new ItemV3(3, "live ItemV3", {7, 313}, true))),
      item_v4_(new Item(404, new ItemV4)),
      item_null_imp_(new Item(303, nullptr)) {}

HfwManager::~HfwManager() = default;

__attribute__((noinline)) Item *HfwManager::createItem(int kind) {
  if (kind == 0)
    return nullptr;
  if (kind == 2)
    return item_v2_.get();
  if (kind == 3)
    return item_v3_.get();
  if (kind == 4)
    return item_v4_.get();
  if (kind == 5)
    return item_null_imp_.get();
  return item_v1_.get();
}

__attribute__((noinline)) Item &HfwManager::referenceItem(int kind) {
  if (kind == 1)
    return *item_v2_;
  if (kind == 2)
    return *item_v3_;
  if (kind == 3)
    return *item_v4_;
  if (kind == 4)
    return *item_null_imp_;
  return *item_v1_;
}

} // namespace HFW

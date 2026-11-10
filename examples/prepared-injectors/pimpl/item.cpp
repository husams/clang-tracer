#include "item-impl.hpp"

namespace HFW {
namespace {
struct ItemV4 final : ItemImp {
  unsigned revision = 4;
  int source = 404;
};
} // namespace

Item::Item(int value, ItemImp *implementation, ItemImp *other)
    : id(value), _imp(implementation), _other(other),
      stamp_{value + 700}, _stamp(&stamp_) {}

Item::~Item() {
  delete _imp;
  delete _other;
}

__attribute__((noinline)) int Item::compare(const Item &other) const {
  return id - other.id;
}

HfwManager::HfwManager()
    : item_v1_(new Item(101, new ItemV1(1, "live ItemV1", {27, true}, 808),
                        new ItemV2(2, "secondary ItemV2", {12, true}, 909))),
      item_v2_(new Item(202, new ItemV2(2, "live ItemV2", {12, true}, 909),
                        new ItemV3(3, "secondary ItemV3", {7, 313}, true))),
      item_v3_(new Item(303, new ItemV3(3, "live ItemV3", {7, 313}, true),
                        new ItemV1(1, "secondary ItemV1", {27, true}, 808))),
      item_v4_(new Item(404, new ItemV4,
                        new ItemV1(1, "known secondary ItemV1", {27, true}, 808))),
      item_null_imp_(new Item(505, nullptr,
                              new ItemV2(2, "secondary ItemV2", {12, true}, 909))),
      item_null_other_(new Item(606,
                                new ItemV3(3, "primary ItemV3", {7, 313}, true),
                                nullptr)) {}

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
  if (kind == 6)
    return item_null_other_.get();
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
  if (kind == 5)
    return *item_null_other_;
  return *item_v1_;
}

} // namespace HFW

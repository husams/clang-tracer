#pragma once

#include <memory>

namespace HFW {

struct ItemV1;
struct ItemV2;
class ItemImp;

class Item {
public:
  Item(int id, ItemImp *implementation);
  ~Item();
  Item(const Item &) = delete;
  Item &operator=(const Item &) = delete;

  int id;

private:
  ItemImp *_imp;
};

class HfwManager {
public:
  HfwManager();
  ~HfwManager();
  HfwManager(const HfwManager &) = delete;
  HfwManager &operator=(const HfwManager &) = delete;

  Item *createItem(int kind);
  Item &referenceItem(int kind);

private:
  std::unique_ptr<Item> item_v1_;
  std::unique_ptr<Item> item_v2_;
  std::unique_ptr<Item> item_null_imp_;
};

} // namespace HFW

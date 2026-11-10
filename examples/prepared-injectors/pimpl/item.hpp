#pragma once

#include <memory>

namespace HFW {

struct ItemV1;
struct ItemV2;
struct ItemV3;
class ItemImp;

struct ItemStamp {
  int serial;
};

class Item {
public:
  Item(int id, ItemImp *implementation, ItemImp *other);
  ~Item();
  int compare(const Item &other) const;
  Item(const Item &) = delete;
  Item &operator=(const Item &) = delete;

  int id;

private:
  ItemImp *_imp;
  ItemImp *_other;
  ItemStamp stamp_;
  ItemStamp *_stamp;
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
  std::unique_ptr<Item> item_v3_;
  std::unique_ptr<Item> item_v4_;
  std::unique_ptr<Item> item_null_imp_;
  std::unique_ptr<Item> item_null_other_;
};

} // namespace HFW

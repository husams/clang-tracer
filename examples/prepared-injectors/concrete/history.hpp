#pragma once

#include <string>

#include "payload.hpp"

namespace prepared_history {

struct EntryBase {
protected:
  int sequence = 7;
};

class HistoryItem : public EntryBase {
public:
  HistoryItem() = default;
  HistoryItem(const HistoryItem &other);
  HistoryItem(HistoryItem &&other) noexcept;
  ~HistoryItem();

  std::string label = "entry";

private:
  int code = 42;
};

const HistoryItem *via_pointer(const HistoryItem *item, int enabled);
const HistoryItem &via_reference(const HistoryItem &item, int tag);
HistoryItem via_value(HistoryItem item, int tag);

struct OpaqueItem {
  union {
    int integer;
    double real;
  };
};

const OpaqueItem *via_opaque(const OpaqueItem *item, int tag);

} // namespace prepared_history

namespace HFW {

template <typename T = int, UP_SQLObject::Flag Enabled = UP_SQLObject::False>
struct ItemDetails {
  T quantity;
};

class HfwManager {
  enum class ItemKind : int { Widget = 3 };
  template <typename T> struct ItemBox;
  struct Item;
  using ItemResult = Item;

public:
  HfwManager() = default;
  ItemResult createItem();
  ItemBox<int> createBox();

private:
  template <typename T> struct ItemBox {
    T value;
  };

  struct Item {
    int id;
    ItemKind kind;
    ItemDetails<> details;
  };

  int seed = 71;
};

} // namespace HFW

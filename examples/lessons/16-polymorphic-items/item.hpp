// The public handle. Callers see only Item; the data lives in the
// implementation that `impl` points to, chosen by ItemFactory.
#pragma once

#include "item_interface.hpp"

#include <string>

struct ItemFactory;

class Item {
  int id = 0;
  ItemInterface* impl = nullptr;
  friend struct ItemFactory;

public:
  Item() = default;
  Item(const Item&) = delete;
  Item& operator=(const Item&) = delete;
  ~Item() { delete impl; }

  const std::string& description() const { return impl->description(); }
  std::string encode(const std::string& type) const { return impl->encode(type); }
};

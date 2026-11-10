#pragma once

#include "item.hpp"

#include <string>
#include <utility>

namespace HFW {

class ItemImp {
public:
  virtual ~ItemImp() = default;
};

struct ItemDetails {
  int build;
  bool stable;
};

struct ItemV1 final : ItemImp {
  ItemV1(int value, std::string text, ItemDetails state, int hidden)
      : version(value), label(std::move(text)), details(state),
        private_code(hidden) {}

  int version;
  std::string label;
  ItemDetails details;

private:
  int private_code;
};

struct ItemV2 final : ItemImp {
  explicit ItemV2(int value) : marker(value) {}

  int marker;
};

} // namespace HFW

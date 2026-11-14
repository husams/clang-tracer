// Version 1: scalar and string fields only.
#pragma once

#include "item_interface.hpp"

#include <string>

class ItemV1 : public ItemInterface {
protected:
  std::string label;
  int revision = 1;

public:
  ItemV1(std::string label, int revision)
      : label(std::move(label)), revision(revision) {}

  const std::string& description() const override { return label; }

  std::string encode(const std::string& type) const override {
    return type + ":" + label + "#" + std::to_string(revision);
  }
};

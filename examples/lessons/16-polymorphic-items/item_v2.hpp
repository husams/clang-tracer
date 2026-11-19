// Version 2: inherits the version 1 fields and adds a vector and a map.
#pragma once

#include "item_v1.hpp"

#include <map>
#include <string>
#include <vector>

class ItemV2 : public ItemV1 {
  std::vector<std::string> tags;
  std::map<std::string, int> attributes;

public:
  ItemV2(std::string label, int revision, std::vector<std::string> tags,
         std::map<std::string, int> attributes)
      : ItemV1(std::move(label), revision), tags(std::move(tags)),
        attributes(std::move(attributes)) {}

  std::string encode(const std::string& type) const override {
    std::string text = ItemV1::encode(type);
    for (const auto& tag : tags) text += " +" + tag;
    for (const auto& [name, value] : attributes)
      text += " " + name + "=" + std::to_string(value);
    return text;
  }
};

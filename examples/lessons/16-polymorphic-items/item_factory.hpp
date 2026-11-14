// Builds an Item and picks its implementation from the version name.
#pragma once

#include "item.hpp"
#include "item_v1.hpp"
#include "item_v2.hpp"
#include "item_v3.hpp"

#include <string>

struct ItemFactory {
  // Returns an Item the caller owns. An unknown version has no implementation.
  Item* create(const std::string& version) const {
    Item* item = new Item();
    if (version == "v1") {
      item->id = 1;
      item->impl = new ItemV1("basic widget", 4);
    } else if (version == "v2") {
      item->id = 2;
      item->impl = new ItemV2("tagged widget", 7, {"blue", "fragile"},
                              {{"weight", 12}, {"shelf", 3}});
    } else if (version == "v3") {
      item->id = 3;
      item->impl = new ItemV3(
          "assembled widget",
          {{"frame", {40.0, 25.5}, {101, 102}},
           {"cover", {41.0, 26.0}, {201}}},
          {{"north", {5, 8, 13}}, {"south", {21}}},
          {{"created", "by:ana"}, {"resized", "by:ben", "width:40"}});
    }
    return item;
  }
};

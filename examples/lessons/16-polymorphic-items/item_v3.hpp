// Version 3: implements the interface directly and stores nested data:
// records inside a vector, vectors inside a map, and vectors inside a list.
#pragma once

#include "item_interface.hpp"

#include <list>
#include <map>
#include <string>
#include <vector>

struct Dimensions {
  double width = 0;
  double height = 0;
};

struct Part {
  std::string name;
  Dimensions size;
  std::vector<int> batches;
};

class ItemV3 : public ItemInterface {
  std::string title;
  std::vector<Part> parts;
  std::map<std::string, std::vector<int>> stock;   // warehouse -> counts
  std::list<std::vector<std::string>> history;     // one entry per change

public:
  ItemV3(std::string title, std::vector<Part> parts,
         std::map<std::string, std::vector<int>> stock,
         std::list<std::vector<std::string>> history)
      : title(std::move(title)), parts(std::move(parts)),
        stock(std::move(stock)), history(std::move(history)) {}

  const std::string& description() const override { return title; }

  std::string encode(const std::string& type) const override {
    std::string text = type + ":" + title;
    for (const auto& part : parts)
      text += " [" + part.name + " x" + std::to_string(part.batches.size()) + "]";
    text += " warehouses=" + std::to_string(stock.size());
    text += " changes=" + std::to_string(history.size());
    return text;
  }
};

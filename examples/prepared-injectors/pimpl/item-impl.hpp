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

struct ItemV2Counters {
  int attempts;
  bool migrated;
};

struct ItemV3Metadata {
  short region;
  int checksum;
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
  ItemV2(unsigned value, std::string text, ItemV2Counters state, long hidden)
      : generation(value), owner(std::move(text)), counters(state),
        private_tag(hidden) {}

  unsigned generation;
  std::string owner;
  ItemV2Counters counters;

private:
  long private_tag;
};

struct ItemV3 final : ItemImp {
  ItemV3(int value, std::string text, ItemV3Metadata state, bool hidden)
      : schema(value), description(std::move(text)), metadata(state),
        private_trusted(hidden) {}

  int schema;
  std::string description;
  ItemV3Metadata metadata;

private:
  bool private_trusted;
};

} // namespace HFW

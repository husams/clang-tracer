#pragma once
#include <string>
#include <vector>

// Target of the context target-call test: a context hook calls these
// functions, which are defined only in the target process.
namespace target_calls {
struct Item {
  virtual ~Item() = default;
  virtual std::string serialize() const;    // std::string by value
  virtual int weight() const;               // scalar
  virtual const std::string &label() const; // reference into the target
  virtual const Item *next() const;         // pointer into the target
  std::string name;
  int weight_ = 0;
  const Item *next_ = nullptr;
};
struct ItemImpl : Item {
  std::string serialize() const override;
};

void populate(std::vector<Item *> &items); // the probed function
int total(const std::vector<Item *> &items);
bool heavier(const Item &item, int limit);
int failing(int value); // always throws
} // namespace target_calls

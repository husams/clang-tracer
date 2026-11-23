#include "items.hpp"

#include <stdexcept>
#include <unistd.h>

namespace target_calls {
std::string Item::serialize() const { return name; }
int Item::weight() const { return weight_; }
const std::string &Item::label() const { return name; }
const Item *Item::next() const { return next_; }
std::string ItemImpl::serialize() const {
  return name + ":" + std::to_string(weight_);
}

int total(const std::vector<Item *> &items) {
  int sum = 0;
  for (const Item *item : items) sum += item->weight();
  return sum;
}
bool heavier(const Item &item, int limit) { return item.weight() > limit; }
int failing(int value) {
  throw std::runtime_error("target function failed " + std::to_string(value));
}

void populate(std::vector<Item *> &items) {
  static ItemImpl first, second;
  first.name = "alpha";
  first.weight_ = 1;
  first.next_ = &second;
  // Longer than the small-string buffer, so the text lives on the heap of the
  // runtime that built it.
  second.name = "a considerably longer name than the small string buffer";
  second.weight_ = 2;
  second.next_ = &first;
  items.clear();
  items.push_back(&first);
  items.push_back(&second);
}
} // namespace target_calls

int main() {
  for (;;) {
    std::vector<target_calls::Item *> items;
    target_calls::populate(items);
    usleep(100000);
  }
}

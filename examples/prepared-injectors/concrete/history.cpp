#include "history.hpp"

#include <utility>
#include <unistd.h>

namespace prepared_history {
namespace {
int copies, moves, destructions;
}

HistoryItem::HistoryItem(const HistoryItem &other)
    : EntryBase(other), label(other.label), code(other.code) {
  ++copies;
}

HistoryItem::HistoryItem(HistoryItem &&other) noexcept
    : EntryBase(other), label(std::move(other.label)), code(other.code) {
  ++moves;
}

HistoryItem::~HistoryItem() { ++destructions; }

const HistoryItem *via_pointer(const HistoryItem *item, int enabled) {
  const volatile int observed = enabled;
  return observed ? item : nullptr;
}

const HistoryItem &via_reference(const HistoryItem &item, int tag) {
  const volatile int observed = tag;
  (void)observed;
  return item;
}

HistoryItem via_value(HistoryItem item, int tag) {
  item.label += std::to_string(tag);
  return item;
}

const OpaqueItem *via_opaque(const OpaqueItem *item, int tag) {
  const volatile int observed = tag;
  (void)observed;
  return item;
}

} // namespace prepared_history

int main() {
  using namespace prepared_history;
  const HistoryItem item;
  const OpaqueItem opaque{3};
  for (;;) {
    copies = moves = destructions = 0;
    if (via_pointer(nullptr, 1) || via_pointer(&item, 1) != &item ||
        &via_reference(item, 4) != &item || via_opaque(&opaque, 4) != &opaque)
      return 1;
    {
      const auto value = via_value(item, 4);
      if (value.label != "entry4")
        return 2;
    }
    if (copies != 1 || moves != 1 || destructions != 2)
      return 3;
    ::usleep(100000);
  }
}

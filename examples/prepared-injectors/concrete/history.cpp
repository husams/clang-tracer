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

int uses_toolchain_headers(const std::error_code *error, const struct tm *time) {
  return error && time ? 1 : 0;
}

} // namespace prepared_history

HFW::HfwManager::Item HFW::HfwManager::createItem() {
  return Item{seed + 2, ItemKind::Widget, {5}};
}

HFW::HfwManager::ItemBox<int> HFW::HfwManager::createBox() {
  return ItemBox<int>{17};
}
int main() {
  using namespace prepared_history;
  const HistoryItem item;
  const OpaqueItem opaque{3};
  HFW::HfwManager manager;
  const std::error_code toolchain_error;
  const struct tm toolchain_time{};
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
    const auto created = manager.createItem();
    if (created.id != 73 || static_cast<int>(created.kind) != 3 ||
        created.details.quantity != 5)
      return 4;
    const auto box = manager.createBox();
    if (box.value != 17)
      return 5;
    if (uses_toolchain_headers(&toolchain_error, &toolchain_time) != 1)
      return 6;
    ::usleep(100000);
  }
}

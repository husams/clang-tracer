#pragma once

#include <string>

namespace prepared_history {

struct EntryBase {
protected:
  int sequence = 7;
};

class HistoryItem : public EntryBase {
public:
  HistoryItem() = default;
  HistoryItem(const HistoryItem &other);
  HistoryItem(HistoryItem &&other) noexcept;
  ~HistoryItem();

  std::string label = "entry";

private:
  int code = 42;
};

const HistoryItem *via_pointer(const HistoryItem *item, int enabled);
const HistoryItem &via_reference(const HistoryItem &item, int tag);
HistoryItem via_value(HistoryItem item, int tag);

} // namespace prepared_history

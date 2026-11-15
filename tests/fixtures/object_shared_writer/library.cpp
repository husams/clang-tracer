#include "record.hpp"

namespace shared_writer {

__attribute__((noinline)) int adjust(Record &record, int value, bool store) {
  const int adjusted = value + 1;
  if (store)
    record.value = adjusted; // SHARED_HELPER_STORE
  return adjusted;
}

Record::Record(int initial) : value(initial) {}
Record::~Record() = default;

__attribute__((noinline)) void write(Record &record, int value, bool store) {
  (void)adjust(record, value, store);
}

} // namespace shared_writer

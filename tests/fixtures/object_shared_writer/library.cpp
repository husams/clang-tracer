#include "record.hpp"

namespace {
volatile int active_records = 0;
}

namespace shared_writer {

__attribute__((noinline)) int adjust(Record &record, int value, bool store) {
  const int adjusted = value + 1;
  if (store)
    record.value = adjusted; // SHARED_HELPER_STORE
  return adjusted;
}

Record::Record(int initial) : value(initial) { active_records = active_records + 1; }
Record::~Record() { active_records = active_records - 1; }

__attribute__((noinline)) void write(Record &record, int value, bool store) {
  (void)adjust(record, value, store);
}

} // namespace shared_writer

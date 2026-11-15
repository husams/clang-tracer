#pragma once

namespace shared_writer {

struct Record {
  int value;
  explicit Record(int initial);
  ~Record();
};

int adjust(Record &record, int value, bool store);
void write(Record &record, int value, bool store);

} // namespace shared_writer

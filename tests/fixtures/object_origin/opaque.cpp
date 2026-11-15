#include "record.hpp"

extern "C" void opaque_write(origin_test::Tracked &record) {
  record.value = 81;
}

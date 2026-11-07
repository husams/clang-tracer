#include "types.hpp"

#include <unistd.h>

int main() {
  const prepared_concrete::Handler handler;
  const prepared_concrete::Box<int, 5> box{6};
  for (;;) {
    if (prepared_concrete::through_base(&handler, 4) != 23 ||
        prepared_concrete::twice<int>(4) != 8 ||
        prepared_concrete::twice<double>(4.0) != 4.5 ||
        handler.convert<int>(4) != 11 ||
        handler.convert<double>(4.0) != 11.5 ||
        box.apply(4) != 15 ||
        prepared_concrete::Handler::scale(4) != 12 ||
        prepared_concrete::Handler::shift<int>(4) != 6 ||
        prepared_concrete::Handler::Nested::adjust(4) != 13 ||
        prepared_concrete::Handler::Nested::adjust<int>(4) != 14)
      return 1;
    ::usleep(100000);
  }
}

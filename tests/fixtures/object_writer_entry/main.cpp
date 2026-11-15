#include "writer.hpp"

#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>

namespace {
bool wait_for(char expected) {
  char command;
  for (;;) {
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 1) {
      if (command == '\n')
        continue;
      return command == expected;
    }
    if (count == 0 || (errno != EAGAIN && errno != EINTR))
      return false;
    ::usleep(1000);
  }
}
} // namespace

int main() {
  using namespace writer_entry;
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 1;
  std::puts("READY");
  std::fflush(stdout);
  if (!wait_for('g'))
    return 1;
  {
    Cell cell(1);
    poke(cell, 11);
#if defined(TRACE_STORED_WRITER)
    Writer writer;
    select_writer(writer);
#else
    const auto writer = chosen();
#endif
    writer(cell, 12);
    apply(cell, 13, writer);
    if (cell.value != 13)
      return 2;
  }
  {
    Knob knob(1);
    knob.set(11);
    turn(knob, 12);
    turn(knob, 13);
    if (knob.value != 13)
      return 3;
  }
  std::puts("DONE");
  std::fflush(stdout);
  return wait_for('q') ? 0 : 4;
}

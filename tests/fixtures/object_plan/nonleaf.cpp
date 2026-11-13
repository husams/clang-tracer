#include "record.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace demo {
[[gnu::noinline, gnu::noipa]] void checkpoint(const PlanRecord &record) {
  asm volatile("" : : "m"(record.value) : "memory");
}

[[gnu::noinline]] void nonleaf_writer(PlanRecord &record) {
  auto *const alias = &record.value;
  checkpoint(record);
  *alias = 19;
  checkpoint(record);
  *alias = 23;
  checkpoint(record);
}

using Writer = void (*)(PlanRecord &);
Writer volatile selected_writer = nonleaf_writer;
}

int next_command() {
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 1 && command != '\n')
      return command;
    if (!size || (size < 0 && errno != EAGAIN && errno != EINTR))
      return EOF;
    ::usleep(1000);
  }
}

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  if (next_command() != 'g')
    return 2;
  {
    demo::PlanRecord record(17);
#if defined(TRACE_TEST_MULTIPLE_WRITERS)
    demo::write_value(record);
#endif
    demo::selected_writer(record);
    std::printf("WROTE value=%d\n", record.value);
    if (record.value != 23)
      return 3;
  }
  for (int ch; (ch = next_command()) != EOF;)
    if (ch == 'q')
      return 0;
  return 0;
}

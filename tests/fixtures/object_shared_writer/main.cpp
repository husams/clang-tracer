#include "record.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#if defined(SHARED_WRITER_INTERPOSE)
namespace shared_writer {

__attribute__((noinline)) int adjust(Record &record, int value, bool store) {
  const int adjusted = value + 1;
  if (store)
    record.value = adjusted; // EXECUTABLE_HELPER_STORE
  return adjusted;
}

} // namespace shared_writer
#endif

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 90;
  std::puts("READY");
  for (;;) {
    char command;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (!count || (count == 1 && command == 'q'))
      break;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 91;
      ::usleep(10000);
      continue;
    }
    if (command != 'g')
      continue;
    {
      shared_writer::Record record(1);
      for (int value = 1; value <= 6; ++value)
        shared_writer::write(record, value, true);
      std::printf("DONE value=%d\n", record.value);
    }
  }
  return 0;
}

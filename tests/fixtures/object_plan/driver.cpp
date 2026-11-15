#include "record.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <thread>

#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) || ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::optional<demo::PlanRecord> record;
  const auto exercise = [](std::optional<demo::PlanRecord> &item) {
    item.emplace(17);
    demo::write_value(*item);
    std::printf("WROTE object=%p field=%p value=%d tid=%ld\n", static_cast<void *>(&*item),
                static_cast<void *>(&item->value), item->value, ::syscall(SYS_gettid));
    return item->value == 23;
  };
  std::printf("READY offset=%zu\n", offsetof(demo::PlanRecord, value));
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 0)
      return 0;
    if (size < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    if (command == 'g') {
      if (!exercise(record))
        return 4;
    } else if (command == 't') {
      bool success = false;
      std::thread writer([&] {
        std::optional<demo::PlanRecord> local;
        success = exercise(local);
      });
      writer.join();
      if (!success)
        return 4;
    } else if (command == 'q') {
      std::puts("EXIT");
      return 0;
    }
  }
}

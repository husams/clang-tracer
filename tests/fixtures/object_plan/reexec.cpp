#include "record.hpp"

#if defined(__aarch64__)
#include "../object_aarch64/rendezvous.hpp"
#endif

#include <cstdio>
#include <unistd.h>

namespace {
void wait_for(const char *path) {
  while (::access(path, F_OK))
    ::usleep(1000);
}
}

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc > 1) {
    std::printf("REEXEC pid=%ld\n", static_cast<long>(::getpid()));
    wait_for("stop.now");
    return 0;
  }
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#else
  wait_for("write.now");
#endif
  demo::PlanRecord record(17);
  demo::write_value(record);
  std::printf("WROTE value=%d\n", record.value);
  wait_for("exec.now");
  ::execl(argv[0], argv[0], "replaced", static_cast<char *>(nullptr));
  return 2;
}

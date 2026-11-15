#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

extern "C" int startup_result();

namespace {
void mark_preinit() {
  const auto marker = ::open("preinit.now", O_CREAT | O_WRONLY, 0600);
  if (marker < 0 || ::close(marker))
    ::_exit(8);
}

void (*const preinit)() __attribute__((section(".preinit_array"), used)) = mark_preinit;
}

int main(int argc, char **) {
  const auto value = startup_result();
  std::printf("MAIN value=%d\n", value);
  std::fflush(stdout);
  if (argc > 1)
    while (::access("stop.now", F_OK))
      ::usleep(1000);
  return value == 23 ? 0 : 5;
}

#include <tracer/launch.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

__attribute__((noinline)) int startup_call(int input) {
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop" ::: "memory");
  return input + 10;
}

int main(int argc, char **argv) {
  if (argc > 1 && std::strcmp(argv[1], "--early") == 0)
    return 7;
  if (argc > 1 && std::strcmp(argv[1], "--no-ready") == 0)
    for (;;)
      ::usleep(10'000);
  if (!tracer::launch::wait_for_resume())
    return 125;

  const int result = startup_call(7);
  char cwd[4096]{};
  char input[128]{};
  if (!::getcwd(cwd, sizeof(cwd)) || !std::fgets(input, sizeof(input), stdin))
    return 8;
  const char *value = std::getenv("TRACER_LAUNCH_TEST");
  std::printf("result=%d\ncwd=%s\nargs=%s,%s\nenv=%s\nstdin=%s",
              result, cwd, argc > 1 ? argv[1] : "", argc > 2 ? argv[2] : "",
              value ? value : "", input);
  std::fflush(stdout);
  std::fputs("target stderr\n", stderr);
  if (argc > 1 && std::strcmp(argv[1], "--exit-after-start") == 0)
    return 0;
  for (;;)
    ::usleep(10'000);
}

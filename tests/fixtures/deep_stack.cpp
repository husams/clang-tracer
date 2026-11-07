#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>

#include <unistd.h>

[[gnu::noinline, gnu::patchable_function_entry(16)]]
int traced_deep_leaf(int value) {
  asm volatile("" : "+r"(value));
  return value;
}

[[gnu::noinline]] int recursive_entry(int depth) {
  if (!depth)
    return traced_deep_leaf(1);
  int value = recursive_entry(depth - 1);
  asm volatile("" : "+r"(value));
  return value + 1;
}

int main(int argc, char **argv) {
  const int depth = argc > 1 ? std::atoi(argv[1]) : 1200;
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0)
    return 1;
  std::puts("READY");
  std::fflush(stdout);
  for (;;) {
    char trigger;
    const auto count = ::read(STDIN_FILENO, &trigger, 1);
    if (count == 0)
      return 0;
    if (count > 0) {
      if (recursive_entry(depth) != depth + 1)
        return 1;
    } else if (errno == EAGAIN || errno == EINTR) {
      ::usleep(10000);
    } else {
      return 1;
    }
  }
}

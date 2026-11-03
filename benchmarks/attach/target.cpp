#include <cstdio>

#include <dlfcn.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    if (!::dlopen(argv[i], RTLD_NOW | RTLD_LOCAL)) {
      std::fprintf(stderr, "%s\n", ::dlerror());
      return 1;
    }
  }
  if (::write(STDOUT_FILENO, "R", 1) != 1) {
    return 1;
  }
  for (;;) {
    timespec delay{0, 100000000};
    ::nanosleep(&delay, nullptr);
  }
}

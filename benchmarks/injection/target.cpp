#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <signal.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
  // This fixture is owned by the benchmark driver, including on driver failure.
  if (::prctl(PR_SET_PDEATHSIG, SIGTERM) != 0)
    return 1;
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0)
    return 1;
  if (argc != 2)
    return 1;
  void *module = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!module) {
    std::fprintf(stderr, "%s\n", ::dlerror());
    return 1;
  }
  auto function =
      reinterpret_cast<int (*)(int)>(::dlsym(module, "injection_target"));
  if (!function)
    return 1;
  std::printf("READY %d\n", static_cast<int>(::getpid()));
  std::fflush(stdout);
  for (int argument = 1;; argument = argument == 1000 ? 1 : argument + 1) {
    const int result = function(argument);
    if (result != argument * 3 + 7)
      return 2;
    timespec delay{0, 20000000};
    while (::nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
  }
}

#include <cstdio>

__attribute__((constructor)) static void loaded() {
  std::puts("injected-library-constructor");
  std::fflush(stdout);
}

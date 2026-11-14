#include "rendezvous.hpp"
#include "../object_plan/record.hpp"

#include <cstdio>
#include <dlfcn.h>

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  auto *library = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!library)
    return 2;
  auto *writer = reinterpret_cast<void (*)(demo::PlanRecord &)>(
      ::dlsym(library, "arm_shared_store"));
  if (!writer)
    return 3;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  arm_rendezvous();
  int observed;
  {
    demo::PlanRecord object(17);
    writer(object);
    observed = object.value;
  }
  std::printf("DONE value=%d\n", observed);
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return ::dlclose(library) || observed != 23;
}

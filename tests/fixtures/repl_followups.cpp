#include "tracer/launch.hpp"
#include "tracer/override_target.hpp"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string_view>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" int ct_followup_import(int value);
extern "C" int ct_followup_patch();

asm(".text\n"
    ".globl ct_followup_patch\n"
    ".type ct_followup_patch,@function\n"
    "ct_followup_patch:\n"
    "mov $11,%eax\n"
    "ret\n"
    ".size ct_followup_patch, .-ct_followup_patch\n");

extern "C" __attribute__((noinline)) int ct_followup_replacement(int value) {
  return value + 700;
}

namespace {
int original_calls = 0;
struct Receiver {
  int value = 100;
  const int &set(const int &delta) {
    value += delta;
    return value;
  }
  int read() const { return value; }
};
}

TRACER_SCALAR_OVERRIDE_TARGET int followup_observed(int value) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  ++original_calls;
  return 40 + value;
}

int main(int argc, char **argv) {
  if (argc != 4)
    return 1;
  const bool plain = std::string_view(argv[1]) == "plain";
  const std::string command_path = argv[2];
  Receiver receiver;
  using tracer::launch::named_call;
  if (!plain &&
      !tracer::launch::wait_for_resume(named_call("set", std::ref(receiver), &Receiver::set),
                                       named_call("read", std::cref(receiver), &Receiver::read)))
    return 2;
  if (std::getenv(tracer::launch::gate_environment) ||
      std::getenv(tracer::launch::preload_environment))
    return 3;
  const long page = ::sysconf(_SC_PAGESIZE);
  auto *region = static_cast<unsigned char *>(
      ::mmap(nullptr, page * 4, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  if (region == MAP_FAILED || ::mprotect(region + page, page * 2, PROT_READ | PROT_WRITE))
    return 4;
  auto *data = static_cast<volatile unsigned char *>(region + page);
  const auto imported = ::dlsym(RTLD_DEFAULT, "ct_followup_import");
  std::printf("ready pid=%d import=%p replacement=%p data=%p page=%ld receiver=%d\n",
              ::getpid(), imported, reinterpret_cast<void *>(&ct_followup_replacement),
              region + page, page, receiver.value);
  std::fflush(stdout);
  void *loaded = nullptr;
  for (;;) {
    char command = 0;
    if (std::ifstream input(command_path); input >> command) {
      ::unlink(command_path.c_str());
      if (command == 'v') {
        const auto observed = followup_observed(2);
        std::printf("values probe=%d import=%d patch=%d original=%d\n", observed,
                    ct_followup_import(2), ct_followup_patch(), original_calls);
      } else if (command == 'd') {
        loaded = ::dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
        if (!loaded)
          return 5;
        std::puts("loaded");
      } else if (command == 'u') {
        if (!loaded || ::dlclose(loaded))
          return 6;
        loaded = nullptr;
        std::puts("unloaded");
      } else if (command == 'r') {
        const auto value = data[31];
        data[page + 53] = value + 7;
        const auto observed = followup_observed(2);
        std::printf("touched value=%u probe=%d\n", data[page + 53], observed);
      } else if (command == 'f') {
        const auto child = ::fork();
        if (child < 0)
          return 7;
        if (child == 0) {
          for (;;)
            ::sleep(1);
        }
        std::printf("child pid=%d\n", child);
        std::fflush(stdout);
        return 0;
      } else if (command == 'q') {
        break;
      }
      std::fflush(stdout);
    }
    ::usleep(5'000);
  }
  return 0;
}

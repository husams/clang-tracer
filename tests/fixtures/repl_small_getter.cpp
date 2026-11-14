#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define OUT_OF_LINE __attribute__((noipa))
#else
#define OUT_OF_LINE __attribute__((noinline))
#endif

namespace demo {
class Circle {
  unsigned char padding[0x50]{};
  int radius;
public:
  explicit Circle(int value) : radius(value) {}
  OUT_OF_LINE int get_radius() const;
  OUT_OF_LINE int get_roomy() const;
  OUT_OF_LINE int get_branchy() const;
};

int Circle::get_radius() const { return radius; }

int Circle::get_roomy() const {
  asm volatile(".rept 16\nnop\n.endr");
  return radius;
}

int Circle::get_branchy() const {
  asm volatile("jmp 1f\n.rept 80\nnop\n.endr\n1:");
  return radius;
}

OUT_OF_LINE int call_radius(const Circle &circle) { return circle.get_radius() + 7; }
OUT_OF_LINE int other_radius(const Circle &circle) { return circle.get_radius() - 2; }
} // namespace demo

namespace {
volatile sig_atomic_t foreign_trap = 0;

void handle_trap(int) { foreign_trap = 1; }

template <typename Method>
std::uintptr_t method_address(Method method) {
  std::uintptr_t address = 0;
  std::memcpy(&address, &method, sizeof(address));
  return address;
}

bool fill_relay_range(std::uintptr_t caller) {
  const auto page = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE));
  std::uintptr_t floor = 0;
  std::ifstream("/proc/sys/vm/mmap_min_addr") >> floor;
  if (!page || !floor || caller > static_cast<std::uintptr_t>(INT32_MAX) ||
      floor % page)
    return false;
  const auto end = (caller + INT32_MAX + 2 * page) / page * page;
  std::vector<std::pair<std::uintptr_t, std::uintptr_t>> gaps;
  std::ifstream maps("/proc/self/maps");
  std::string row;
  while (std::getline(maps, row)) {
    unsigned long begin = 0, next = 0;
    if (std::sscanf(row.c_str(), "%lx-%lx", &begin, &next) != 2)
      return false;
    if (floor >= end)
      break;
    if (begin > floor)
      gaps.emplace_back(floor, std::min(end, begin));
    floor = std::max(floor, next);
  }
  if (floor < end)
    gaps.emplace_back(floor, end);
  for (const auto &gap : gaps) {
    auto *address = reinterpret_cast<void *>(gap.first);
    auto *mapped = ::mmap(address, gap.second - gap.first, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                          -1, 0);
    if (mapped != address)
      return false;
  }
  return true;
}
} // namespace

int main(int argc, char **argv) {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  const auto caller = reinterpret_cast<std::uintptr_t>(&demo::call_radius);
  if (argc == 2 && std::strcmp(argv[1], "blocked") == 0 && !fill_relay_range(caller))
    return 3;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::printf("READY entry=%#lx caller=%#lx alternate=%#lx roomy=%#lx\n",
              method_address(&demo::Circle::get_radius), caller,
              reinterpret_cast<std::uintptr_t>(&demo::other_radius),
              method_address(&demo::Circle::get_roomy));
  const demo::Circle circle(10);
  for (;;) {
    char command = 0;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 0 || command == 'q')
      break;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 4;
      ::usleep(10000);
      continue;
    }
    if (command != 'g')
      continue;
    foreign_trap = 0;
    if (::signal(SIGTRAP, handle_trap) == SIG_ERR || ::raise(SIGTRAP) != 0)
      return 6;
    for (int i = 0; i < 4; ++i) {
      // Keep each invocation observable even when the compiler infers the
      // out-of-line accessors only read this object.
      asm volatile("" : : "r"(&circle) : "memory");
      if (demo::call_radius(circle) != 17 || demo::other_radius(circle) != 8 ||
          circle.get_roomy() != 10 || circle.get_branchy() != 10)
        return 5;
    }
    std::printf("CALLS direct=17 alternate=8 getter=10 foreign_trap=%d\n",
                static_cast<int>(foreign_trap));
  }
  std::puts("EXIT");
}

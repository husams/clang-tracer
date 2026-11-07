#include <csignal>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t change_requested = 0;
unsigned char large_range[128 * 1024]{};

void request_change(int) { change_requested = 1; }
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  const auto page = ::sysconf(_SC_PAGESIZE);
  const int backing = ::open(argv[1], O_RDONLY);
  if (page <= 0 || backing < 0)
    return 2;
  auto *base = static_cast<unsigned char *>(::mmap(
      nullptr, 6 * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  if (base == MAP_FAILED || ::mprotect(base, 2 * page, PROT_READ | PROT_WRITE) != 0)
    return 3;
  const unsigned char across[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
  std::memcpy(base + page - 4, across, sizeof(across));
  volatile unsigned char *tail = large_range + sizeof(large_range) - sizeof(across);
  for (std::size_t i = 0; i < sizeof(across); ++i)
    tail[i] = across[i];
  const float floating = -1.5f;
  std::memcpy(base + 8, &floating, sizeof(floating));
  if (::mprotect(base + page, page, PROT_READ) != 0 ||
      ::mmap(base + 4 * page, 2 * page, PROT_READ,
             MAP_FIXED | MAP_PRIVATE, backing, 0) == MAP_FAILED)
    return 4;
  std::signal(SIGUSR1, request_change);
  std::printf("memory base=%p page=%ld large=%p\n",
              static_cast<void *>(base), page, static_cast<void *>(large_range));
  std::fflush(stdout);
  unsigned stage = 0;
  for (;;) {
    if (change_requested) {
      change_requested = 0;
      switch (++stage) {
      case 1:
        if (::munmap(base + 3 * page, page) != 0)
          return 5;
        std::puts("hole ready");
        break;
      case 2:
        if (::mprotect(base + 2 * page, page, PROT_READ) != 0)
          return 6;
        std::puts("permissions changed");
        break;
      case 3:
        if (::mmap(base + page, page, PROT_READ,
                   MAP_FIXED | MAP_PRIVATE, backing, 0) == MAP_FAILED)
          return 7;
        std::puts("remapped");
        break;
      }
      std::fflush(stdout);
    }
    ::usleep(10000);
  }
}

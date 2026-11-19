#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "state.hpp"

namespace boundary_test {
__attribute__((noinline, noipa)) void Record::first() {
  value = 17;
  values.push_back(17);
}

__attribute__((noinline, noipa)) void Record::second() {
  value = 23;
  values.back() = 23;
}

} // namespace boundary_test

int main() {
  // Keep this target independent of the controller's Protobuf runtime, as a
  // normal native application is. Loading the agent must register its own RPCs.
  const auto page = ::sysconf(_SC_PAGESIZE);
  char path[] = "boundary-text-XXXXXX";
  const auto fd = ::mkstemp(path);
  if (page <= 0 || fd < 0 || ::ftruncate(fd, 2 * page) != 0) {
    return 2;
  }
  auto* code = static_cast<unsigned char*>(
      ::mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0));
  ::close(fd);
  if (code == MAP_FAILED) {
    return 3;
  }
  constexpr std::uint32_t sleep_number = 0xd2800008U | (SYS_nanosleep << 5);
  constexpr std::array<std::uint32_t, 5> words{sleep_number, 0xaa1f03e1U, 0xd4000001U, 0xd503201fU,
                                               0xd65f03c0U};
  auto* entry = code + page - sizeof(words);
  std::memcpy(entry, words.data(), sizeof(words));
  __builtin___clear_cache(reinterpret_cast<char*>(entry), reinterpret_cast<char*>(code + page));
  if (::mprotect(code, page, PROT_READ | PROT_EXEC) != 0 ||
      ::mprotect(code + page, page, PROT_NONE) != 0 || ::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 ||
      ::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0) {
    return 4;
  }
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  const auto sleep = reinterpret_cast<long (*)(const timespec*)>(entry);
  const timespec delay{0, 50 * 1000 * 1000};
  boundary_test::Record record{0};
  unsigned progress = 0;
  for (;;) {
    sleep(&delay);
    record.first();
    record.second();
    ++progress;
    char command = 0;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count < 0 && errno != EAGAIN && errno != EINTR) {
      return 5;
    }
    if (!count || command == 'q') {
      break;
    }
    if (command == 'g') {
      std::printf("PROGRESS %u\n", progress);
    }
  }
  ::munmap(code, 2 * page);
  ::unlink(path);
  return record.value == 23 ? 0 : 6;
}

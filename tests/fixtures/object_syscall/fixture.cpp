#include <cstdint>
#include <cstdio>

#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

namespace kernel_test {
struct Record {
  int value;
  __attribute__((noinline)) Record();
  __attribute__((noinline)) ~Record();
};

Record::Record() : value(17) {}
Record::~Record() { asm volatile("" ::: "memory"); }

extern "C" __attribute__((noinline)) void userspace_write(Record &record) {
  record.value = 23;
}
}

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY))
    return 1;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  while (::access("write.now", F_OK))
    ::usleep(1000);
  {
    kernel_test::Record record;
    int channel[2];
    if (::pipe(channel))
      return 2;
    const int from_pipe = 41;
    if (::write(channel[1], &from_pipe, sizeof(from_pipe)) != sizeof(from_pipe) ||
        ::read(channel[0], &record.value, sizeof(record.value)) != sizeof(record.value) ||
        record.value != 41)
      return 3;
    auto *file = std::tmpfile();
    if (!file)
      return 4;
    const int from_file = 53;
    const auto fd = ::fileno(file);
    if (::write(fd, &from_file, sizeof(from_file)) != sizeof(from_file) ||
        ::pread(fd, &record.value, sizeof(record.value), 0) != sizeof(record.value) ||
        record.value != 53)
      return 5;
    std::fclose(file);
    const std::uint16_t partial = 7;
    if (::write(channel[1], &partial, sizeof(partial)) != sizeof(partial) ||
        ::read(channel[0], &record.value, sizeof(record.value)) != sizeof(partial) ||
        record.value != 7)
      return 6;
    ::close(channel[0]);
    ::close(channel[1]);
    kernel_test::userspace_write(record);
    if (record.value != 23)
      return 7;
  }
  std::puts("DONE");
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return 0;
}

#include <cstdio>
#include <unistd.h>

namespace startup_test {
struct Record {
  int value;
#if defined(TRACE_TEST_STARTUP_LIBRARY)
  __attribute__((noinline, patchable_function_entry(5))) Record();
  __attribute__((noinline, patchable_function_entry(5))) ~Record();
#else
  __attribute__((noinline)) Record();
  __attribute__((noinline)) ~Record();
#endif
};

Record::Record() : value(17) {}
Record::~Record() { asm volatile("" ::: "memory"); }

extern "C" __attribute__((noinline, patchable_function_entry(5))) void startup_write(Record &record) {
  record.value = 23;
}

int result;

void run_before_main() {
  Record record;
  startup_write(record);
  result = record.value;
}
#if defined(TRACE_TEST_STARTUP_LIBRARY)
extern "C" int startup_result() { return result; }
__attribute__((constructor)) void initialize() {
  if (::access("preinit.now", F_OK))
    ::_exit(9);
  run_before_main();
}
#elif defined(TRACE_TEST_PREINIT)
void (*const preinit)() __attribute__((section(".preinit_array"), used)) = run_before_main;
#else
__attribute__((constructor)) void initialize() { run_before_main(); }
#endif
}

#if !defined(TRACE_TEST_STARTUP_LIBRARY)
int main(int argc, char **) {
  std::printf("MAIN value=%d\n", startup_test::result);
  std::fflush(stdout);
  if (argc > 1)
    while (::access("stop.now", F_OK))
      ::usleep(1000);
  return startup_test::result == 23 ? 0 : 5;
}
#endif

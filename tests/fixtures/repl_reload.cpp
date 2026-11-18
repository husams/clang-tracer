#include <tracer/override_target.hpp>

#include <atomic>
#include <cstdio>
#include <thread>
#include <fcntl.h>
#include <unistd.h>

namespace {
std::atomic<bool> hold_call{false}, release_call{false};
}

__attribute__((noinline)) int observed(int input) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  if (input == 1) {
    std::puts("call held");
    std::fflush(stdout);
    while (!release_call.load(std::memory_order_relaxed))
      ::usleep(10000);
  }
  return input + 10;
}

// Distinct patchable entries for a batch larger than the former 32-probe cap.
#define MANY_PROBE(N) \
  __attribute__((noinline)) int many_probe_##N(int input) { \
    asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory"); \
    return input + N; \
  }
MANY_PROBE(0)
MANY_PROBE(1)
MANY_PROBE(2)
MANY_PROBE(3)
MANY_PROBE(4)
MANY_PROBE(5)
MANY_PROBE(6)
MANY_PROBE(7)
MANY_PROBE(8)
MANY_PROBE(9)
MANY_PROBE(10)
MANY_PROBE(11)
MANY_PROBE(12)
MANY_PROBE(13)
MANY_PROBE(14)
MANY_PROBE(15)
MANY_PROBE(16)
MANY_PROBE(17)
MANY_PROBE(18)
MANY_PROBE(19)
MANY_PROBE(20)
MANY_PROBE(21)
MANY_PROBE(22)
MANY_PROBE(23)
MANY_PROBE(24)
MANY_PROBE(25)
MANY_PROBE(26)
MANY_PROBE(27)
MANY_PROBE(28)
MANY_PROBE(29)
MANY_PROBE(30)
MANY_PROBE(31)
MANY_PROBE(32)
MANY_PROBE(33)
MANY_PROBE(34)
MANY_PROBE(35)
MANY_PROBE(36)
MANY_PROBE(37)
MANY_PROBE(38)
MANY_PROBE(39)
#undef MANY_PROBE

#if defined(TRACER_SCALAR_OVERRIDE_TARGET)
TRACER_SCALAR_OVERRIDE_TARGET
#else
__attribute__((noinline))
#endif
int wide_probe(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10,
           int a11, int a12, int a13, int a14, int a15, int a16, int a17, int a18, int a19, int a20,
           int a21, int a22, int a23, int a24, int a25, int a26, int a27, int a28, int a29, int a30,
           int a31, int a32, int a33, int a34, int a35, int a36, int a37, int a38, int a39) {
  asm volatile(".rept 16\n\tnop\n\t.endr" ::: "memory");
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 +
         a17 + a18 + a19 + a20 + a21 + a22 + a23 + a24 + a25 + a26 + a27 + a28 + a29 + a30 + a31 +
         a32 + a33 + a34 + a35 + a36 + a37 + a38 + a39;
}

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1)
    return 1;
  std::thread([] {
    while (!hold_call.load(std::memory_order_relaxed))
      ::usleep(10000);
    std::printf("held call returned %d\n", observed(1));
    std::fflush(stdout);
  }).detach();
  std::printf("ready entry=%p\n", reinterpret_cast<void *>(&observed));
  std::fflush(stdout);
  for (;;) {
    char command = 0;
    (void)::read(STDIN_FILENO, &command, 1);
    if (command == 'h')
      hold_call.store(true, std::memory_order_relaxed);
    else if (command == 'r')
      release_call.store(true, std::memory_order_relaxed);
    else if (command == 'w') {
      std::printf("wide call returned %d\n",
                  wide_probe(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                             20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
                             38, 39));
      std::fflush(stdout);
    } else if (command == 'm') {
      int total = 0;
      total += many_probe_0(1);
      total += many_probe_1(1);
      total += many_probe_2(1);
      total += many_probe_3(1);
      total += many_probe_4(1);
      total += many_probe_5(1);
      total += many_probe_6(1);
      total += many_probe_7(1);
      total += many_probe_8(1);
      total += many_probe_9(1);
      total += many_probe_10(1);
      total += many_probe_11(1);
      total += many_probe_12(1);
      total += many_probe_13(1);
      total += many_probe_14(1);
      total += many_probe_15(1);
      total += many_probe_16(1);
      total += many_probe_17(1);
      total += many_probe_18(1);
      total += many_probe_19(1);
      total += many_probe_20(1);
      total += many_probe_21(1);
      total += many_probe_22(1);
      total += many_probe_23(1);
      total += many_probe_24(1);
      total += many_probe_25(1);
      total += many_probe_26(1);
      total += many_probe_27(1);
      total += many_probe_28(1);
      total += many_probe_29(1);
      total += many_probe_30(1);
      total += many_probe_31(1);
      total += many_probe_32(1);
      total += many_probe_33(1);
      total += many_probe_34(1);
      total += many_probe_35(1);
      total += many_probe_36(1);
      total += many_probe_37(1);
      total += many_probe_38(1);
      total += many_probe_39(1);
      std::printf("many calls returned %d\n", total);
      std::fflush(stdout);
    } else if (command == 's') {
      std::printf("short call returned %d\n", observed(2));
      std::fflush(stdout);
    }
    ::usleep(10000);
  }
}

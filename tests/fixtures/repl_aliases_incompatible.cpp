// Identical machine code need not have an identical calling convention. Gold's
// --icf=all folds each pair below even though their scalar widths differ.
#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY (-1UL)
#endif

// Leave enough real instructions for an entry detour, so a refusal cannot be
// explained by the function being too short to patch.
#define ENTRY_SPACE __asm__ volatile(".rept 32\n\tnop\n\t.endr")

__attribute__((noinline)) float a_float(float x) { ENTRY_SPACE; return x; }
__attribute__((noinline)) double z_double(double x) { ENTRY_SPACE; return x; }
__attribute__((noinline)) float a_return_float() { ENTRY_SPACE; return 0.0f; }
__attribute__((noinline)) double z_return_double() { ENTRY_SPACE; return 0.0; }

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0)
    return 1;
  // Volatile function pointers preserve real calls even with -O2. In particular,
  // the compiler must not replace the identity calls with their input values.
  float (*volatile narrow)(float) = a_float;
  double (*volatile wide)(double) = z_double;
  float (*volatile narrow_result)() = a_return_float;
  double (*volatile wide_result)() = z_return_double;
  std::printf("ALIAS_FIXTURE pid=%ld\n", static_cast<long>(::getpid()));
  std::printf("ICF_ENTRIES identity_float=%p identity_double=%p return_float=%p return_double=%p\n",
              reinterpret_cast<void *>(narrow), reinterpret_cast<void *>(wide),
              reinterpret_cast<void *>(narrow_result), reinterpret_cast<void *>(wide_result));
  std::fflush(stdout);
  unsigned checks = 0;
  while (true) {
    // Wake regularly so attaching the loader never waits on an indefinitely
    // blocked input syscall. Test requests still trigger each health report.
    pollfd input{STDIN_FILENO, POLLIN, 0};
    if (::poll(&input, 1, 0) <= 0) {
      ::usleep(1000);
      continue;
    }
    if (std::getchar() == EOF)
      return 0;
    const float expected_float = 1.234567f;
    const double expected_double = 1.23456789012345;
    const float actual_float = narrow(expected_float);
    const double actual_double = wide(expected_double);
    const float actual_float_result = narrow_result();
    const double actual_double_result = wide_result();
    if (actual_float != expected_float || actual_double != expected_double ||
        actual_float_result != 0.0f || actual_double_result != 0.0) {
      std::printf("ICF_CORRUPT float=%.9g double=%.17g return_float=%.9g return_double=%.17g\n",
                  actual_float, actual_double, actual_float_result, actual_double_result);
      std::fflush(stdout);
      return 2;
    }
    std::printf("ICF_OK count=%u\n", ++checks);
    std::fflush(stdout);
  }
}

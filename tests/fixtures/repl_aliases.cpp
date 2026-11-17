// Several names for one function: an explicit assembler alias, and (when linked with
// identical-code folding) functions with identical bodies. Callers pass a different argument
// range per name so a test can tell which name reached the shared entry.
#include <cstdio>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY (-1UL)
#endif

namespace alias_demo {

struct Twin {
  int fold_a(int x) const;
  int fold_b(int x) const;
  int aliased(int x) const;  // defined only as an alias of fold_a, see the assembler below
  long wide(long x) const;   // a different signature, for partial-failure rollback
};

struct Folded {
  int one(int x) const;
  int two(int x) const;
  int three(int x) const;
};

__attribute__((noinline)) int Twin::fold_a(int x) const { return x * 3 + 1; }
__attribute__((noinline)) int Twin::fold_b(int x) const { return x * 3 + 1; }
__attribute__((noinline)) long Twin::wide(long x) const { return x * 5 + 2; }

// Identical bodies. With --icf=all the linker folds the three into one entry and keeps a
// symbol and a DWARF subprogram for each.
#define FOLDED_BODY(x) \
  int acc = (x);       \
  for (int i = 0; i < ((x) & 7); ++i) acc += i * 7 + 3; \
  if (acc & 1) acc ^= 0x55; \
  return acc
__attribute__((noinline)) int Folded::one(int x) const { FOLDED_BODY(x); }
__attribute__((noinline)) int Folded::two(int x) const { FOLDED_BODY(x); }
__attribute__((noinline)) int Folded::three(int x) const { FOLDED_BODY(x); }

}  // namespace alias_demo

// _ZNK10alias_demo4Twin7aliasedEi is alias_demo::Twin::aliased(int) const.
__asm__(".globl _ZNK10alias_demo4Twin7aliasedEi\n"
        ".type _ZNK10alias_demo4Twin7aliasedEi,@function\n"
        ".set _ZNK10alias_demo4Twin7aliasedEi,_ZNK10alias_demo4Twin6fold_aEi\n");

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0)
    return 1;
  const alias_demo::Twin twin{};
  const alias_demo::Folded folded{};
  std::printf("ALIAS_FIXTURE pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  for (unsigned iteration = 0;; ++iteration) {
    const int n = static_cast<int>(iteration % 100000);
    volatile int sink = twin.fold_a(n) + twin.fold_b(n) + twin.aliased(n + 1000000) +
                        static_cast<int>(twin.wide(n)) +
                        folded.one(n) + folded.two(n + 2000000) + folded.three(n + 3000000);
    (void)sink;
    usleep(2000);
  }
}

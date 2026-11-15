#include <cstdint>
#include <cstdio>
#include <unistd.h>

namespace union_test {
using Lanes = std::uint64_t __attribute__((vector_size(16)));
struct Pair { Lanes lanes; };
struct Wide { std::uint64_t first, second, third, last; };
union alignas(16) Payload {
  std::uint64_t number;
  double real;
  Pair pair;
  Wide wide;
};
struct Frame {
  std::uint64_t kind;
  Payload payload;
};
struct Cell {
  struct { Frame state; } envelope;
  __attribute__((noinline)) explicit Cell(std::uint64_t kind);
  __attribute__((noinline)) ~Cell();
  __attribute__((noinline)) void wide();
  __attribute__((noinline)) void pair();
  __attribute__((noinline)) void tagged();
  __attribute__((noinline)) void switch_tag();
};

Cell::Cell(std::uint64_t kind) : envelope{{kind, {}}} {
  if (kind == 3) envelope.state.payload.wide = {11, 22, 33, 44};
  else if (kind == 4) envelope.state.payload.pair = Pair{Lanes{1, 2}};
  else envelope.state.payload.number = 7;
  asm volatile("" ::: "memory");
}
Cell::~Cell() { asm volatile("" ::: "memory"); }
void Cell::wide() {
  auto *target = &envelope.state.payload.wide.last;
  *target = 88;
  asm volatile("" ::: "memory");
  *target = 44;
}
void Cell::pair() {
  envelope.state.payload.pair = Pair{Lanes{81, 93}};
  asm volatile("" ::: "memory");
  envelope.state.payload.pair = Pair{Lanes{1, 2}};
}
void Cell::tagged() {
  auto *target = &envelope.state.payload.number;
  *target = 0xfedcba9876543210ULL;
  asm volatile("" ::: "memory");
  *target = 7;
}
void Cell::switch_tag() {
  auto &state = envelope.state;
  state.kind = 0x100000002ULL;
  asm volatile("" ::: "memory");
  state.payload.real = 2.5;
  asm volatile("" ::: "memory");
  state.payload.real = 3.5;
}
}

int main(int argc, char **argv) {
  if (argc != 2) return 1;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  while (::access("go", F_OK)) ::usleep(1000);
  const auto test = argv[1][0];
  union_test::Cell cell(test == 'w' ? 3 : test == 'p' ? 4 : test == 'u' ? 19 : 0x100000001ULL);
  if (test == 'w') cell.wide();
  else if (test == 'p') cell.pair();
  else if (test == 's') cell.switch_tag();
  else cell.tagged();
  const auto &state = cell.envelope.state;
  if (test == 'w' ? state.payload.wide.last != 44 :
      test == 'p' ? state.payload.pair.lanes[0] != 1 || state.payload.pair.lanes[1] != 2 :
      test == 's' ? state.kind != 0x100000002ULL || state.payload.real != 3.5 :
                    state.payload.number != 7) return 2;
  std::puts("DONE");
  while (::access("stop", F_OK)) ::usleep(1000);
}

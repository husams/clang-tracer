#include "state.hpp"

#include <cstdio>
#include <unistd.h>

#if defined(TRACE_ARM_TEST_RENDEZVOUS)
#include "rendezvous.hpp"
#endif

int main() {
  using namespace tagged_test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(TRACE_ARM_TEST_RENDEZVOUS)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  State integer(Kind::integer);
  State real(Kind::real);
  State unregistered(Kind::unregistered);
  write_bits(integer, 23);
  write_bits(real, 0x40200000);
  write_bits(unregistered, 0x01020304);
  std::printf("DONE integer=%d real=%.1f raw=%08x\n", integer.payload.integer,
              real.payload.real, unregistered.payload.raw);
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return integer.payload.integer != 23 || real.payload.real != 2.5F ||
         unregistered.payload.raw != 0x01020304;
}

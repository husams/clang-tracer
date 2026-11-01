#include <cstdio>
#include <unistd.h>

struct UnsupportedType {};

class UnsupportedFixture {
public:
  __attribute__((noinline)) UnsupportedType unsupported(UnsupportedType value) { return value; }
};

class X {
public:
  __attribute__((noinline)) int f(int value) { return value + 1; }
  __attribute__((noinline)) int f(long value) { return static_cast<int>(value) + 2; }
};

class VirtualOnly {
public:
  virtual __attribute__((noinline)) int f(int value) { return value + 3; }
};

class VolatileOnly {
public:
  __attribute__((noinline)) int v(int value) volatile { return value + 4; }
};

class UnsignedOnly {
public:
  __attribute__((noinline)) int u(unsigned long value) { return static_cast<int>(value) + 5; }
};

class HeaderReceiver {
public:
  int bias = 7;
  __attribute__((noinline)) int read(int value) const { return value + bias; }
};

int main() {
  UnsupportedFixture unsupported;
  X overloads;
  VirtualOnly virtual_only;
  volatile VolatileOnly volatile_only;
  UnsignedOnly unsigned_only;
  HeaderReceiver header_receiver;
  std::printf("CPP_NAME_FIXTURE pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  for (;;) {
    (void)unsupported.unsupported({});
    volatile int result = overloads.f(1) + overloads.f(2L) + virtual_only.f(3) +
                          volatile_only.v(4) + unsigned_only.u(5) + header_receiver.read(6);
    (void)result;
    ::usleep(50000);
  }
}

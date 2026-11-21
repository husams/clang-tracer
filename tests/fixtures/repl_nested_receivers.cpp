// Methods of classes nested in classes, observed through DWARF alone and with real headers.
#include "repl_nested_receivers.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#define NOINLINE __attribute__((noinline))

namespace nested_receivers {
NOINLINE int Outer::Nested::f(int x) const { return v + x; }
NOINLINE int Outer::Sibling::g(int x) const { return w * x; }
NOINLINE int Outer2::Mid::Deep::f(int x) const { return v + x; }
NOINLINE int Plain::get() const { return a + b; }
}  // namespace nested_receivers

int main() {
  std::cout << "NESTED_FIXTURE pid=" << getpid() << std::endl;
  for (;;) {
    nested_receivers::Outer outer;
    nested_receivers::Outer::Sibling sibling;
    nested_receivers::Outer2::Mid::Deep deep;
    nested_receivers::Plain plain;
    int sink = outer.n.f(7) + sibling.g(2) + deep.f(1) + plain.get();
    asm volatile("" : : "r"(sink));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

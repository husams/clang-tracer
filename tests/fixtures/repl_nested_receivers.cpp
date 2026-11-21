// Methods of classes nested in classes, observed through DWARF alone and with real headers.
#include "repl_nested_partial.hpp"
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
struct Partial::Nested { int n = 9; int partial(int x) const; };
NOINLINE int Partial::Nested::partial(int x) const { return n + x; }
NOINLINE int UnionScope::Nested::simple(int x) const { return n + x; }
NOINLINE int OwnerCase::Nested::owner(const OwnerCase *o) const { return n + o->v * 3; }
NOINLINE int SiblingCase::A::sibling(const B *b) const { return n + b->s; }
NOINLINE int OpaqueOwner::Nested::owner(const OpaqueOwner *o) const { return n + o->values[1]; }
NOINLINE int DeepSibling::A::sibling(const B::Deep *d) const { return n + d->n; }
NOINLINE int Plain::get() const { return a + b; }
}  // namespace nested_receivers

int main() {
  std::cout << "NESTED_FIXTURE pid=" << getpid() << std::endl;
  for (;;) {
    nested_receivers::Outer outer;
    nested_receivers::Outer::Sibling sibling;
    nested_receivers::Outer2::Mid::Deep deep;
    nested_receivers::Plain plain;
    nested_receivers::OpaqueOwner opaque;
    nested_receivers::OpaqueOwner::Nested opaque_nested;
    nested_receivers::DeepSibling::A deep_a;
    nested_receivers::DeepSibling::B::Deep deep_b;
    nested_receivers::Partial::Nested partial;
    nested_receivers::UnionScope::Nested unioned;
    nested_receivers::OwnerCase owner;
    nested_receivers::OwnerCase::Nested owned;
    nested_receivers::SiblingCase::A a;
    nested_receivers::SiblingCase::B b;
    int sink = outer.n.f(7) + sibling.g(2) + deep.f(1) + plain.get() +
               partial.partial(1) + unioned.simple(1) + owned.owner(&owner) + a.sibling(&b) +
               opaque_nested.owner(&opaque) + deep_a.sibling(&deep_b);
    asm volatile("" : : "r"(sink));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

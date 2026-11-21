// Functions whose parameter or result is a pointer to a type in an anonymous namespace. Each
// translation unit and each library defines its own `Anon` with a different layout.
#pragma once

#define ANON_NOINLINE __attribute__((noinline))

namespace anonp {
struct Plain { int a = 1; };
int take_plain(Plain *p);  // control: a named type

int run_unit_a(int pass);   // anon_unit_a.cpp: struct Anon { int a, b; }
int run_unit_b(int pass);   // anon_unit_b.cpp: struct Anon { long x, y, z; }
int run_local(int pass);    // function-local pointer control
const char *unit_a_address();
const char *unit_b_address();
}  // namespace anonp

namespace anonl {
int run_library_one(int pass);  // libanon_one.so: struct Anon { int a, b; }
int run_library_two(int pass);  // libanon_two.so: struct Anon { long x, y, z; }
}  // namespace anonl

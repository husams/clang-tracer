#include "anon_pointer.hpp"

namespace anonp {
namespace { struct Anon { int a; int b; }; }
static Anon g_a{3, 4};
ANON_NOINLINE int take_plain(Plain *p) { return p->a; }
ANON_NOINLINE int take_a(Anon *p) { return p->a * 10 + p->b; }
ANON_NOINLINE Anon *make_a(int pass) { g_a.a = 3 + pass; return &g_a; }
// By value the callback would need the original type's ABI, so this one stays unsupported.
ANON_NOINLINE int take_value(Anon v) { return v.a; }
int run_unit_a(int pass) {
  Plain p;
  return take_plain(&p) + take_a(make_a(pass)) + take_value(g_a);
}
const char *unit_a_address() { return reinterpret_cast<const char *>(&g_a); }
}  // namespace anonp

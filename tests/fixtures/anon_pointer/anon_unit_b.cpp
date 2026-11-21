#include "anon_pointer.hpp"

namespace anonp {
namespace { struct Anon { long x; long y; long z; }; }
static Anon g_b{5, 6, 7};
ANON_NOINLINE int take_b(Anon *p) { return static_cast<int>(p->x * 100 + p->y * 10 + p->z); }
ANON_NOINLINE Anon *make_b(int pass) { g_b.x = 5 + pass; return &g_b; }
int run_unit_b(int pass) { return take_b(make_b(pass)); }
const char *unit_b_address() { return reinterpret_cast<const char *>(&g_b); }
}  // namespace anonp

#include "receiver.hpp"
namespace unnamed {
Plain::Plain(int x) { value = x; }
Plain::~Plain() { value = -1; }
int Plain::ordinary(int x) { return value + x; }
int Plain::constant(int x) const { return value + x + 1; }
int Plain::statik(int x) { return x + 3; }
Root::Root(int x) { root = x; }
Root::~Root() { root = -2; }
Mid::Mid(int x) : Root(x + 10) { value = x; }
Mid::~Mid() { value = -3; }
Leaf::Leaf(int x) : Root(x + 20), Mid(x + 1) {}
Leaf::~Leaf() {}
int run_once(int pass) {
  Plain p(pass);
  Leaf l(pass);
  return p.ordinary(7) + p.constant(7) + Plain::statik(7);
}
}  // namespace unnamed

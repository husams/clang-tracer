#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>
#include <vector>
#include <unistd.h>

namespace structural_test {
struct Entry {
  unsigned id;
  struct { int quantity; } state;
};
using Entries = std::vector<Entry>;
struct Board { Entries entries; };

__attribute__((noinline)) void grow(Entries &items) {
  items.resize(4, {40, {4}});
}
__attribute__((noinline)) void move(Entries &items) {
  std::rotate(items.begin(), items.begin() + 1, items.end());
}
__attribute__((noinline)) void swap(Entries &items, Entries &peer) {
  items.swap(peer);
}
__attribute__((noinline)) void insert(Entries &items) {
  items.push_back({12, {70}});
}
__attribute__((noinline)) void erase(Entries &items) {
  items.pop_back();
}
__attribute__((noinline)) void destroy(Entries &items) {
  items.~Entries();
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}
__attribute__((noinline)) void exercise(Board &board, Entries &peer) {
  auto &items = board.entries;
  grow(items);
  move(items);
  swap(items, peer);
  insert(items);
  items.back().state.quantity = 71;
  erase(items);
  insert(items);
  destroy(items);
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}
} // namespace structural_test

int main() {
  using namespace structural_test;
  alignas(Board) unsigned char storage[sizeof(Board)];
  auto *board = new (storage) Board{{{12, {100}}, {13, {20}}}};
  Entries peer{{19, {5}}};
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  while (::access("go", F_OK))
    ::usleep(1000);
  exercise(*board, peer);
  if (peer.size() != 4 || peer.back().id != 12 || peer.back().state.quantity != 100)
    std::abort();
  std::puts("DONE");
  while (::access("stop", F_OK))
    ::usleep(1000);
}

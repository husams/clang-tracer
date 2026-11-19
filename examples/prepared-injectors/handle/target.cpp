#include "item-impl.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <string>
#include <sys/prctl.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY (~0UL)
#endif
#include <unistd.h>

namespace {
HFW::HfwItemImpl implementation;
#ifdef HANDLE_DEEP
HFW::HfwItemDetail detail;
int counter = 42;
std::string text_value = "text pointee";
HFW::HfwNode ring[3] = {{1, &ring[1]}, {2, &ring[2]}, {3, &ring[0]}};
int *dangling = nullptr;
HFW::HfwNode self_link{9, &self_link};
#endif
#ifdef HANDLE_STALE
// Mapped memory that looks like an object whose members hold garbage.
alignas(8) unsigned char stale_storage[sizeof(HFW::HfwItemImpl)];
alignas(8) std::uint64_t stale_members[8];
alignas(16) unsigned char stale_vbase[sizeof(HFW::HfwVirtualImpl)];
alignas(8) std::uint64_t stale_big[4];
#endif
#ifdef HANDLE_MAP
HFW::HfwMapImpl map_value{{{1, 2}}, 4};
#endif
#ifdef HANDLE_LIMITS
// A chain longer than any --follow-depth the tracer accepts.
constexpr int chain_length = 64;
HFW::HfwLink chain_nodes[chain_length];
struct ChainInit {
  ChainInit() {
    for (int i = 0; i < chain_length; ++i) {
      chain_nodes[i].id = i + 1;
      chain_nodes[i].next = i + 1 < chain_length ? &chain_nodes[i + 1] : nullptr;
    }
  }
} chain_init;
HFW::HfwVectorImpl vector_value{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
HFW::HfwStringsImpl strings_value{"a", "b", "c", "d"};
HFW::HfwWideImpl wide_value{std::vector<int>(5000, 7)};
#endif
#ifdef HANDLE_SCALARS
bool flag_value = true;
signed char signed_value[3] = {static_cast<signed char>(-3), 'a', 0};
unsigned char byte_value = 200;
HFW::HfwMode mode_value = HFW::high;
std::string long_text(5000, 'z');
#endif
#ifdef HANDLE_WIDGET
HFW::Widget widget;
HFW::Gadget gadget;
HFW::PolyWidget poly;
#endif
#ifdef HANDLE_REFERENCE
HFW::HfwItem item{7, implementation};
#else
HFW::HfwItem item{7};
#endif
}

__attribute__((noinline)) HFW::HfwItem &HFW::HfwManager::getItem(int id) {
#if defined(HANDLE_UNIQUE)
  if (!item._impl)
    item._impl = std::make_unique<HFW::HfwItemImpl>();
  item._impl->revision = id;
  item._impl->label = "implementation label";
#ifdef HANDLE_DEEP
  item._impl->_detail = &detail;
  detail.back = item._impl.get();
#endif
#elif defined(HANDLE_SHARED)
  if (!item._impl)
    item._impl = std::make_shared<HFW::HfwItemImpl>();
  item._impl->revision = id;
  item._impl->label = "implementation label";
  item._weak = item._impl;
#elif defined(HANDLE_REFERENCE)
  implementation.revision = id;
#else
  implementation.revision = id;
  item._impl = &implementation;
#endif
#ifdef HANDLE_MAP
  item._mapped = &map_value;
#endif
#ifdef HANDLE_LIMITS
  item._chain = &chain_nodes[0];
  item._vec = &vector_value;
  item._strings = &strings_value;
  item._wide = &wide_value;
#endif
#ifdef HANDLE_SCALARS
  item._flag = &flag_value;
  item._signed = signed_value;
  item._byte = &byte_value;
  item._mode = &mode_value;
  item._long = long_text.c_str();
  item._first = "first";
  item._second = "second";
  item._third = "third";
#endif
#ifdef HANDLE_DEEP
  item._note = "note text";
  item._counter = &counter;
  item._text = &text_value;
  item._dangling = dangling;
#ifdef HANDLE_UNIQUE
  item._nested = {5, item._impl.get()};
#else
  item._nested = {5, &implementation};
#endif
  item._head = &ring[0];
  item._loop = &self_link;
#ifdef HANDLE_UNIQUE
  tracked_ = item._impl.get();
#else
  tracked_ = &implementation;
#endif
#endif
  return item;
}

#ifndef HANDLE_UNIQUE
__attribute__((noinline)) HFW::HfwItem HFW::HfwManager::makeItem(int id) {
  return getItem(id);
}
#endif

__attribute__((noinline)) int HFW::HfwManager::inspectItem(const HFW::HfwItem &) {
  return 1;
}

#ifdef HANDLE_STALE
__attribute__((noinline)) int HFW::HfwManager::load(HFW::HfwItem &) { return 1; }
__attribute__((noinline)) int HFW::HfwManager::loadStale() {
  HFW::HfwItem local{3};
  local._impl = reinterpret_cast<HFW::HfwItemImpl *>(stale_storage);
  local._stale = reinterpret_cast<HFW::HfwStaleImpl *>(stale_members);
  local._vbase = reinterpret_cast<HFW::HfwVirtualImpl *>(stale_vbase);
  local._big = reinterpret_cast<HFW::HfwBigVector *>(stale_big);
  return load(local);
}
#endif
#ifdef HANDLE_RECURSE
__attribute__((noinline)) int HFW::HfwManager::recurse(const HFW::HfwItem &item, int depth) {
  return depth ? recurse(item, depth - 1) + 1 : 0;
}
#endif
#ifdef HANDLE_WIDGET
__attribute__((noinline)) int HFW::HfwManager::widgetId(const HFW::Widget &value) {
  return value.id;
}
__attribute__((noinline)) int HFW::HfwManager::gadgetId(const HFW::Gadget &value) {
  return value.id;
}
__attribute__((noinline)) int HFW::HfwManager::polyId(const HFW::PolyWidget &value) {
  return value.id;
}
#endif

#ifdef HANDLE_THREAD
// The traced function runs on a thread with a 64 KiB stack.
void *small_stack(void *) {
  HFW::HfwManager manager;
  for (;;) {
    manager.getItem(827);
    ::usleep(100000);
  }
  return nullptr;
}
#endif

#ifdef HANDLE_BUDGET
__attribute__((noinline)) int HFW::HfwManager::budget(const HFW::HfwBig &, const HFW::HfwBig &,
                                                      const HFW::HfwBig &, const HFW::HfwBig &) {
  return 4;
}
__attribute__((noinline)) HFW::HfwBig HFW::HfwManager::makeBig(const HFW::HfwBig &, const HFW::HfwBig &,
                                                               const HFW::HfwBig &,
                                                               const HFW::HfwBig &d) {
  return d;
}
#endif

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0)
    return 1;
  implementation.label = "implementation label";
#ifdef HANDLE_DEEP
  detail.depth = 3;
  detail.back = &implementation;
  implementation._detail = &detail;
  // A pointer into memory that is not mapped: the page below the lowest
  // address a process may map, so nothing can ever appear there.
  dangling = reinterpret_cast<int *>(0x2000);
#endif
#ifdef HANDLE_BUDGET
  HFW::HfwBig big[4];
  for (auto &entry : big) {
    entry.text.assign(3000, 'x');
    entry.impl = &implementation;
  }
  implementation.label.assign(3000, 'y');
#endif
#ifdef HANDLE_STALE
  std::memset(stale_storage, 0xA5, sizeof(stale_storage));
  // vector: unmapped data of 1000 ints; unique_ptr and shared_ptr: unmapped pointees.
  stale_members[0] = 0x2000;
  stale_members[1] = 0x2000 + 4000;
  stale_members[2] = 0x2000 + 4000;
  stale_members[3] = 0x3000;
  stale_members[4] = 0x3000;
  stale_members[5] = 0x4000;
  stale_members[6] = 7;
  std::memset(stale_vbase, 0xA5, sizeof(stale_vbase));
  // A vector far larger than any real one, in unmapped memory.
  stale_big[0] = 0x2000;
  stale_big[1] = 0x2000 + 4 * 100000;
  stale_big[2] = 0x2000 + 4 * 100000;
  stale_big[3] = 9;
#endif
  std::puts("ready");
  std::fflush(stdout);
  HFW::HfwManager manager;
#ifdef HANDLE_THREAD
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, 64 * 1024);
  pthread_t worker;
  pthread_create(&worker, &attributes, small_stack, nullptr);
#endif
  for (;;) {
#ifndef HANDLE_THREAD
    manager.inspectItem(manager.getItem(827));
#endif
#ifdef HANDLE_STALE
    manager.loadStale();
#endif
#ifdef HANDLE_RECURSE
    manager.recurse(manager.getItem(827), 200);
#endif
#ifdef HANDLE_WIDGET
    manager.widgetId(widget);
    manager.gadgetId(gadget);
    manager.polyId(poly);
#endif
#ifdef HANDLE_BUDGET
    manager.budget(big[0], big[1], big[2], big[3]);
    manager.makeBig(big[0], big[1], big[2], big[3]);
#endif
#if !defined(HANDLE_UNIQUE) && !defined(HANDLE_THREAD)
    manager.makeItem(827);
#endif
    ::usleep(100000);
  }
}

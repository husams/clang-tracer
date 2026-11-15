#include "rendezvous.hpp"

#include <cstdio>
#include <memory>

namespace pointee_test {
struct Node {
  struct State { int quantity; } state;
};

struct Link {
#if defined(TRACE_TEST_UNIQUE_POINTEE)
  std::unique_ptr<Node> node;
#elif defined(TRACE_TEST_SHARED_POINTEE)
  std::shared_ptr<Node> node;
#else
  Node *node;
#endif

  Node *get() const {
#if defined(TRACE_TEST_UNIQUE_POINTEE) || defined(TRACE_TEST_SHARED_POINTEE)
    return node.get();
#else
    return node;
#endif
  }
};

struct Owner {
  struct Routes { Link *head; } routes;
  void select(Link *);
  void write(Link &);
};

__attribute__((noinline, noipa)) void Owner::select(Link *link) {
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
  routes.head = link;
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}

__attribute__((noinline, noipa)) void Owner::write(Link &replacement) {
  auto *stale = routes.head->get();
  stale->state.quantity = 11;
  select(&replacement);
  auto *current = routes.head->get();
  stale->state.quantity = 99;
  current->state.quantity = 21;
  select(&replacement);
  current->state.quantity = 22;
  select(nullptr);
  current->state.quantity = 23;
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}
}

int main() {
  using pointee_test::Node;
#if defined(TRACE_TEST_UNIQUE_POINTEE)
  pointee_test::Link original{std::make_unique<Node>(Node{{10}})},
                     replacement{std::make_unique<Node>(Node{{20}})};
#elif defined(TRACE_TEST_SHARED_POINTEE)
  pointee_test::Link original{std::make_shared<Node>(Node{{10}})},
                     replacement{std::make_shared<Node>(Node{{20}})};
#else
  Node first_node{{10}}, second_node{{20}};
  pointee_test::Link original{&first_node}, replacement{&second_node};
#endif
  const auto &first = *original.get();
  const auto &second = *replacement.get();
  pointee_test::Owner owner{{&original}};
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  owner.write(replacement);
  std::printf("DONE first=%p:%d second=%p:%d\n", static_cast<const void *>(&first.state.quantity),
              first.state.quantity, static_cast<const void *>(&second.state.quantity), second.state.quantity);
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return first.state.quantity != 99 || second.state.quantity != 23;
}

#include "state.hpp"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unistd.h>

namespace snapshot_test {
Primary::~Primary() = default;
Secondary::~Secondary() = default;
MoveBomb::MoveBomb(int error) { throw error; }
MoveBomb::MoveBomb(MoveBomb &&) { throw 7; }
MoveBomb &MoveBomb::operator=(MoveBomb &&) { throw 7; }

struct Opaque { int value; };
struct Hidden final : Primary, Secondary { int hidden = 19; };

State::State(Secondary &child) : child_(&child), alias_(&child) {
  for (int i = 0; i != 400; ++i)
    items_.push_back(std::make_unique<int>(i));
}
void State::rehash_only() {
  const auto previous = hash_.bucket_count();
  hash_.rehash(previous * 3 + 1);
  if (hash_.bucket_count() == previous)
    std::abort();
}
void State::update_registered() { std::any_cast<Registered &>(extension_).value = 18; }
void State::mutate() {
  *items_[350] = 700;
  large_[4200] = 45;
  unselected_[39000] = 8;
  nested_[1][0] = 15;
  map_.erase(7);
  map_.at(9) = 21;
  map_.emplace(12, 30);
  note_.emplace<1>(5);
  extension_ = Opaque{32};
  std::thread child_writer([this] { static_cast<Known &>(*child_).visible = 17; });
  child_writer.join();
}
void State::make_valueless() {
  try {
    note_.emplace<2>(7);
  } catch (int error) {
    if (error != 7 || !note_.valueless_by_exception())
      std::abort();
  }
}
void State::replace_child() {
  static Hidden fallback;
  child_ = &fallback;
  alias_ = &fallback;
}
} // namespace snapshot_test

int main() {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  snapshot_test::Known known;
  if (static_cast<void *>(&known) == static_cast<void *>(static_cast<snapshot_test::Secondary *>(&known)))
    return 2;
  snapshot_test::State state(known);
  snapshot_test::Known another;
  snapshot_test::State other(another);
  std::puts("READY");
  while (::access("mutate.now", F_OK) != 0)
    ::usleep(1000);
  other.rehash_only();
  state.rehash_only();
  state.update_registered();
  state.mutate();
  state.make_valueless();
  state.replace_child();
  std::puts("DONE");
  while (::access("stop.now", F_OK) != 0)
    ::usleep(1000);
}

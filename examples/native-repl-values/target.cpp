#include "types.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>
#include <unistd.h>

namespace value_examples {

// O0, noinline, and the small volatile locals leave room for the native detour.
__attribute__((visibility("default"), noinline)) Account
user_types(Address address, Account account) {
  volatile int padding_a = 0, padding_b = 0;
  account.id += 1 + padding_a + padding_b;
  account.address = address;
  return account;
}

__attribute__((visibility("default"), noinline)) std::string
simple_types(bool enabled, int count, double ratio, const std::string &text,
             const std::optional<int> &serial) {
  volatile int padding_a = 0, padding_b = 0;
  (void)padding_a;
  (void)padding_b;
  return enabled && count == 3 && ratio == 1.25 && serial.value_or(0) == 0
             ? text + "-returned"
             : "unexpected";
}

__attribute__((visibility("default"), noinline)) bool
sequence_types(const Coordinates &coordinates, const Addresses &addresses,
               const Numbers &numbers, const Messages &messages,
               const Matrix &matrix) {
  volatile int padding_a = 0, padding_b = 0;
  return coordinates.at(2) == 6 && addresses.at(0).city == "Paris" &&
         numbers.front() == 3 && messages.front() == "queued" &&
         matrix.at(1).empty() && padding_a == padding_b;
}

__attribute__((visibility("default"), noinline)) bool
map_types(const Counts &counts) {
  volatile int padding_a = 0, padding_b = 0;
  return counts.at("apples") == 2 && padding_a == padding_b;
}
__attribute__((visibility("default"), noinline)) bool
unordered_map_types(const Groups &groups) {
  volatile int padding_a = 0, padding_b = 0;
  return groups.at(2).at(1) == 4 && groups.at(0).empty() &&
         padding_a == padding_b;
}
__attribute__((visibility("default"), noinline)) bool
set_types(const SortedIds &ids) {
  volatile int padding_a = 0, padding_b = 0;
  return ids.count(4) && padding_a == padding_b;
}
__attribute__((visibility("default"), noinline)) bool
unordered_set_types(const Labels &labels) {
  volatile int padding_a = 0, padding_b = 0;
  return labels.count("red") && padding_a == padding_b;
}
} // namespace value_examples

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
} // namespace

int main() {
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  std::printf("Typed values target PID=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  const value_examples::Coordinates coordinates{2, 4, 6};
  const value_examples::Addresses addresses{{"Paris", 75001}, {"Oslo", 150}};
  const value_examples::Numbers numbers{3, 5, 7};
  const value_examples::Messages messages{"queued", "done"};
  const value_examples::Matrix matrix{{1, 2}, {}, {3}};
  const value_examples::Counts counts{{"apples", 2}, {"pears", 4}};
  const value_examples::Groups groups{{2, {2, 4}}, {0, {}}};
  const value_examples::SortedIds ids{2, 4, 6};
  const value_examples::Labels labels{"red", "blue"};
  unsigned iteration = 0;
  while (running) {
    auto account = value_examples::user_types({"Paris", 75001}, {});
    (void)account;
    const std::optional<int> serial =
        (++iteration % 2) ? std::optional<int>{0} : std::nullopt;
    auto text = value_examples::simple_types(true, 3, 1.25, "hello", serial);
    (void)text;
    volatile bool sequences = value_examples::sequence_types(
        coordinates, addresses, numbers, messages, matrix);
    volatile bool map = value_examples::map_types(counts);
    volatile bool unordered_map = value_examples::unordered_map_types(groups);
    volatile bool set = value_examples::set_types(ids);
    volatile bool unordered_set = value_examples::unordered_set_types(labels);
    (void)sequences;
    (void)map;
    (void)unordered_map;
    (void)set;
    (void)unordered_set;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

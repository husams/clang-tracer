#include "objects.hpp"

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <poll.h>
#include <string>
#include <unistd.h>

namespace lesson {

Counter::Counter(int initial) : value(initial) {}
Counter::~Counter() = default;
int Counter::add(int amount) { return value += amount; }

Base::Base(int initial) : base(initial) {}
Base::~Base() = default;
int Base::score(int amount) const { return base + amount; }

Derived::Derived(int initial, int extra) : Base(initial), bonus(extra) {}
Derived::~Derived() = default;
int Derived::score(int amount) const { return base + bonus + amount; }

int through_base(const Base& object, int amount) {
  return object.score(amount);
}

}  // namespace lesson

int main() {
  std::setvbuf(stdin, nullptr, _IONBF, 0);
  std::cout << "PID=" << getpid() << '\n'
            << "Enter: default values   a: larger arguments   b: qualified base call   q: quit"
            << std::endl;
  pollfd terminal{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Give the tracer attachment opportunities while the terminal is idle.
    const int ready = poll(&terminal, 1, 0);
    if (ready < 0 && errno != EINTR) {
      std::perror("poll");
      return 1;
    }
    if (ready <= 0) {
      usleep(20'000);
      continue;
    }
    std::string input;
    if (!(terminal.revents & POLLIN) || !std::getline(std::cin, input))
      break;
    if (input == "q")
      break;
    if (!input.empty() && input != "a" && input != "b") {
      std::cout << "Use Enter, a, b, or q." << std::endl;
      continue;
    }
    lesson::Counter counter(10);
    lesson::Derived derived(7, 5);
    if (input == "b") {
      std::cout << "qualified base score=" << derived.lesson::Base::score(3)
                << std::endl;
      continue;
    }
    const int sum = counter.add(input == "a" ? 7 : 2);
    const int score = lesson::through_base(derived, input == "a" ? 8 : 3);
    std::cout << "sum=" << sum << " score=" << score << std::endl;
  }
}

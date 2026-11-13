#include <cerrno>
#include <cstdio>
#include <new>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#if defined(TRACE_ARM_TEST_RENDEZVOUS)
#include "rendezvous.hpp"
#endif

namespace life {
class Record {
  int value_;

public:
  Record();
  explicit Record(int value);
  Record(const Record &source);
  Record(Record &&source);
  virtual ~Record();
  void write(int value);
};

Record::Record() : Record(7) { value_ = 8; }

Record::Record(int value) : value_(value) {
  if (value < 0)
    throw std::runtime_error("fixture constructor rejected value");
}

Record::Record(const Record &source) : value_(source.value_) {}
Record::Record(Record &&source) : value_(source.value_) {}
Record::~Record() = default;
void Record::write(int value) { value_ = value; }

class Base {
  int value_ = 5;

public:
  Base();
  virtual ~Base();
  void write(int value);
};

Base::Base() = default;
Base::~Base() = default;
void Base::write(int value) { value_ = value; }

class Derived final : public Base {
public:
  Derived();
  ~Derived() override;
};

Derived::Derived() = default;
Derived::~Derived() = default;

void variants() {
  try {
    Record failed(-1);
  } catch (const std::runtime_error &) {
    std::puts("caught");
  }
  Record stack;
  stack.write(10);
  Record copied(stack);
  copied.write(20);
  Record moved(std::move(copied));
  moved.write(30);
  alignas(Record) unsigned char storage[sizeof(Record)];
  for (int initial : {40, 50}) {
    auto *placed = new (storage) Record(initial);
    placed->write(initial + 1);
    placed->~Record();
  }
  auto *heap = new Record(60);
  heap->write(61);
  delete heap;
}
} // namespace life

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(TRACE_ARM_TEST_RENDEZVOUS)
  arm_rendezvous();
#endif
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 0)
      return 0;
    if (size < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    switch (command) {
    case 'g':
      life::variants();
      std::puts("variants done");
      break;
    case 'b': {
      life::Derived value;
      value.write(11);
      std::puts("alias done");
      break;
    }
    case 'q':
      return 0;
    }
  }
}

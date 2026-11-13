#include <cerrno>
#include <cstdio>
#include <new>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#if defined(__x86_64__)
#include <signal.h>
#include <ucontext.h>

extern "C" char object_watch_divide_pc;
static volatile sig_atomic_t divide_faults = 0;

static void repair_divide(int sig, siginfo_t *info, void *context) {
  auto &registers = static_cast<ucontext_t *>(context)->uc_mcontext.gregs;
  if (sig != SIGFPE || info->si_code != FPE_INTDIV || info->si_addr != &object_watch_divide_pc ||
      registers[REG_RIP] != reinterpret_cast<greg_t>(&object_watch_divide_pc))
    _exit(4);
  divide_faults = 1;
  registers[REG_RSI] = 4;
}
#endif

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
  void adjust(int amount);
#if defined(__x86_64__)
  void divide(int divisor);
  void scale();
#endif
};

Record::Record() : Record(7) {
  value_ = 8;
  asm volatile("nop; nop; nop; nop; nop");
}

Record::Record(int value) : value_(value) {
  if (value < 0)
    throw std::runtime_error("fixture constructor rejected value");
  asm volatile("nop; nop; nop; nop; nop");
}

// Leave room for separate entry and return redirects with Clang and GCC.
Record::Record(const Record &source) : value_(source.value_) {
  asm volatile("nop; nop; nop; nop; nop");
}
Record::Record(Record &&source) : value_(source.value_) {
  asm volatile("nop; nop; nop; nop; nop");
}
Record::~Record() { asm volatile("nop; nop; nop; nop; nop"); }
void Record::write(int value) { value_ = value; }
void Record::adjust(int amount) { value_ -= amount; }

#if defined(__x86_64__)
void Record::divide(int divisor) {
  int quotient = value_;
  int remainder;
  asm volatile("cdq\n"
               ".globl object_watch_divide_pc\n"
               "object_watch_divide_pc:\n"
               "idivl %%esi\n"
               : "+a"(quotient), "=&d"(remainder) : "S"(divisor) : "cc");
  value_ = remainder;
}

void Record::scale() { value_ = static_cast<int>(value_ * 1.5); }

bool divide() {
  Record value(9);
  value.divide(0);
  return divide_faults == 1;
}

void scale() {
  Record value(9);
  value.scale();
}
#endif

class Base {
  int value_ = 5;

public:
  Base();
  virtual ~Base();
  void write(int value);
};

Base::Base() { asm volatile("nop; nop; nop; nop; nop"); }
Base::~Base() { asm volatile("nop; nop; nop; nop; nop"); }
void Base::write(int value) { value_ = value; }

class Derived final : public Base {
public:
  Derived();
  ~Derived() override;
};

Derived::Derived() { asm volatile("nop; nop; nop; nop; nop"); }
Derived::~Derived() { asm volatile("nop; nop; nop; nop; nop"); }
struct Implicit : Base { int tag = 0; };

struct TemplateBase {
  int value;
  TemplateBase(int);
  ~TemplateBase();
};
TemplateBase::TemplateBase(int initial) : value(initial) {}
TemplateBase::~TemplateBase() { asm volatile("nop; nop; nop; nop; nop" : : "r"(this) : "memory"); }
template <class T> struct Typed : TemplateBase {
  T item;
  explicit Typed(T initial) : TemplateBase(3), item(initial) {}
};
void update_template(TemplateBase &object, int value) { object.value = value; }

void template_base() {
  Typed<int> derived(2);
  update_template(derived, 13);
  TemplateBase complete(5);
  update_template(complete, 15);
}

struct Widget {
  int value;
  Widget(int);
  ~Widget();
};
Widget::Widget(int initial) : value(initial) {}
Widget::~Widget() { asm volatile("" : : "r"(this) : "memory"); }
struct Plain : Widget { int extra; };
void poke(Widget &widget, int value) { widget.value = value; }

void aggregate() {
  Plain value{{3}, 30};
  poke(value, 13);
}

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

void reuse() {
  alignas(Record) unsigned char storage[sizeof(Record)];
  auto *prior = new (storage) Record(40);
  prior->write(41);
  auto *next = new (storage) Record(50);
  next->write(51);
  next->Record::~Record();
}

void alias() {
  Derived value;
  value.write(11);
  Implicit implicit;
  implicit.write(14);
  Base complete;
  complete.write(12);
}

void capacity() {
  Record first(80);
  Record skipped(90);
  skipped.write(91);
  first.write(81);
}

void arithmetic() {
  Record value(9);
  value.adjust(2);
}
} // namespace life

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
#if defined(__x86_64__)
  struct sigaction action{};
  action.sa_sigaction = repair_divide;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGFPE, &action, nullptr))
    return 5;
#endif
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
    case 'r':
      life::reuse();
      std::puts("reuse done");
      break;
    case 'b':
      life::alias();
      std::puts("alias done");
      break;
    case 'c':
      life::capacity();
      std::puts("capacity done");
      break;
    case 'a':
      life::arithmetic();
      std::puts("arithmetic done");
      break;
#if defined(__x86_64__)
    case 'd':
      if (!life::divide())
        return 6;
      std::puts("division done");
      break;
    case 's':
      life::scale();
      std::puts("scale done");
      break;
#endif
    case 'p':
      life::aggregate();
      std::puts("aggregate done");
      break;
    case 't':
      life::template_base();
      std::puts("template done");
      break;
    case 'q':
      return 0;
    }
  }
}

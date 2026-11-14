#include "record.hpp"

#if defined(__aarch64__)
#include "../object_aarch64/rendezvous.hpp"
#endif

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ucontext.h>

#include <fcntl.h>
#include <unistd.h>

extern "C" char ct_async_spin, ct_async_resume;

namespace {
std::atomic<std::uintptr_t> interrupted_pc;
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
volatile std::sig_atomic_t redirect;

void on_signal(int, siginfo_t *, void *context) {
#if defined(__aarch64__)
  auto &pc = static_cast<ucontext_t *>(context)->uc_mcontext.pc;
#else
  auto &pc = static_cast<ucontext_t *>(context)->uc_mcontext.gregs[REG_RIP];
#endif
  interrupted_pc.store(static_cast<std::uintptr_t>(pc), std::memory_order_relaxed);
  if (redirect)
    pc = reinterpret_cast<std::uintptr_t>(&ct_async_resume);
}

int next_command() {
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 1 && command != '\n')
      return command;
    if (!size || (size < 0 && errno != EAGAIN && errno != EINTR))
      return EOF;
    ::usleep(1000);
  }
}
}

extern "C" __attribute__((noinline, noipa))
void async_write(demo::PlanRecord &record, unsigned long count) {
  asm volatile(".globl ct_async_spin\n"
               "ct_async_spin:\n"
#if defined(__aarch64__)
               "subs %0, %0, #1\n"
               "b.ne ct_async_spin\n"
#else
               "subq $1, %0\n"
               "jnz ct_async_spin\n"
#endif
               ".globl ct_async_resume\n"
               "ct_async_resume:\n" : "+r"(count) : : "cc");
  record.value = 23;
}

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  struct sigaction handler{};
  handler.sa_flags = SA_SIGINFO;
  handler.sa_sigaction = on_signal;
  ::sigemptyset(&handler.sa_mask);
  if (::sigaction(SIGUSR1, &handler, nullptr))
    return 3;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#endif
  const auto command = next_command();
  if (command != 'g' && command != 'r')
    return 4;
  redirect = command == 'r';
  demo::PlanRecord record(17);
  std::printf("SPINNING begin=%p end=%p\n", static_cast<void *>(&ct_async_spin),
              static_cast<void *>(&ct_async_resume));
  async_write(record, redirect ? 3000000000UL : 600000000UL);
  const auto pc = interrupted_pc.load(std::memory_order_relaxed);
  std::printf("DONE pc=%#lx value=%d\n", static_cast<unsigned long>(pc), record.value);
  if (record.value != 23 || !pc)
    return 5;
  return next_command() == 'q' ? 0 : 6;
}

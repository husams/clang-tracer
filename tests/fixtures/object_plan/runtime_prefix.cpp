#include "record.hpp"

#include <cerrno>
#include <cstdio>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

extern "C" {
void *trace_runtime_pointer_page;
void *trace_runtime_guard_page;
void trace_runtime_prefix_load();
void trace_runtime_body_load();
void trace_static_body_load();
}

namespace demo {
[[gnu::naked, gnu::noinline]] int nonleaf_dispatch(PlanRecord &) {
  asm volatile(
      "push %rbx\n"
      "mov %rdi, %rbx\n"
      "mov trace_runtime_pointer_page(%rip), %rax\n"
      ".globl trace_runtime_prefix_load\n"
#if defined(TRACE_TEST_RUNTIME_MEMORY)
      "nop\n"
      "trace_runtime_prefix_load: call *(%rax)\n"
#else
      "trace_runtime_prefix_load: mov (%rax), %rax\n"
      "nop\n"
      "call *%rax\n"
#endif
      "mov (%rbx), %eax\n"
      "pop %rbx\n"
      "ret\n");
}

[[gnu::naked, gnu::noinline]] void held_writer(PlanRecord &) {
  asm volatile(
      "mov trace_runtime_guard_page(%rip), %rax\n"
      ".globl trace_runtime_body_load\n"
      "trace_runtime_body_load: mov (%rax), %eax\n"
      "mov %eax, (%rdi)\n"
      "ret\n");
}

[[gnu::naked, gnu::noinline]] void held_static_writer(PlanRecord &, const int *) {
  asm volatile(
      ".globl trace_static_body_load\n"
      "trace_static_body_load: mov (%rsi), %eax\n"
      "mov %eax, (%rdi)\n"
      "ret\n");
}
}

namespace {
int wake_pipe[2];
long page_size;

void fault(int, siginfo_t *info, void *context) {
  const auto pc = static_cast<ucontext_t *>(context)->uc_mcontext.gregs[REG_RIP];
  const bool prefix = info->si_addr == trace_runtime_pointer_page &&
                      pc == reinterpret_cast<greg_t>(&trace_runtime_prefix_load);
  const bool body = info->si_addr == trace_runtime_guard_page &&
      (pc == reinterpret_cast<greg_t>(&trace_runtime_body_load) ||
       pc == reinterpret_cast<greg_t>(&trace_static_body_load));
  if (!prefix && !body)
    ::_exit(6);
  constexpr char prefix_message[] = "PREFIX_FAULT\n", body_message[] = "BODY_FAULT\n";
  const auto size = prefix ? sizeof(prefix_message) - 1 : sizeof(body_message) - 1;
  if (::write(STDOUT_FILENO, prefix ? prefix_message : body_message, size) != size)
    ::_exit(7);
  char byte;
  if (::read(wake_pipe[0], &byte, 1) != 1 ||
      ::syscall(SYS_mprotect, info->si_addr, page_size, PROT_READ) != 0)
    ::_exit(8);
}

char read_command() {
  for (;;) {
    char command = 0;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count > 0)
      return command;
    if (count == 0 || (errno != EINTR && errno != EAGAIN))
      return 'q';
    ::usleep(1000);
  }
}
}

int main() {
  page_size = ::sysconf(_SC_PAGESIZE);
  if (page_size <= 0 || ::pipe(wake_pipe) || ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  trace_runtime_pointer_page = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  trace_runtime_guard_page = ::mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (trace_runtime_pointer_page == MAP_FAILED || trace_runtime_guard_page == MAP_FAILED)
    return 3;
  *static_cast<int *>(trace_runtime_guard_page) = 23;
  struct sigaction action{};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = fault;
  if (::sigaction(SIGSEGV, &action, nullptr))
    return 4;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  for (char command; (command = read_command()) != 'q';) {
    if (command != 'g' && command != 'h' && command != 's')
      continue;
    if (::mprotect(trace_runtime_pointer_page, page_size, PROT_READ | PROT_WRITE))
      return 4;
    *static_cast<decltype(&demo::write_value) *>(trace_runtime_pointer_page) =
        command == 'g' ? &demo::write_value : &demo::held_writer;
    if (::mprotect(command == 'g' ? trace_runtime_pointer_page : trace_runtime_guard_page,
                   page_size, PROT_NONE))
      return 4;
    demo::PlanRecord record(17);
    std::thread release([] {
      for (char command; (command = read_command()) != 'q';)
        if (command == 'r') {
          if (::write(wake_pipe[1], &command, 1) != 1)
            ::_exit(9);
          return;
        }
      ::_exit(10);
    });
    int result;
    if (command == 's') {
      demo::held_static_writer(record, static_cast<const int *>(trace_runtime_guard_page));
      result = record.value;
    } else {
      result = demo::nonleaf_dispatch(record);
    }
    release.join();
    std::printf("WROTE value=%d\n", result);
    if (result != 23)
      return 5;
  }
}

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

asm(R"(
.text
.globl verified_entry
.hidden verified_entry
.type verified_entry,@function
verified_entry:
  .cfi_startproc
  push %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  mov %rsp,%rbp
  .cfi_def_cfa_register %rbp
  .rept 10
  nop
  .endr
  mov $7,%eax
  pop %rbp
  .cfi_def_cfa %rsp,8
  ret
  .cfi_endproc
.size verified_entry, .-verified_entry

.globl indirect_entry
.hidden indirect_entry
.type indirect_entry,@function
indirect_entry:
  .cfi_startproc
  jmp *%rdi
  .rept 16
  nop
  .endr
  ret
  .cfi_endproc
.size indirect_entry, .-indirect_entry

.globl direct_entry
.hidden direct_entry
.type direct_entry,@function
direct_entry:
  .cfi_startproc
  test %edi,%edi
  je 1f
  mov $1,%eax
  ret
1:
  mov $2,%eax
  ret
  .cfi_endproc
.size direct_entry, .-direct_entry
)");

extern "C" void verified_entry(), indirect_entry(), direct_entry();

__attribute__((noinline)) int named_entry(int input) {
  volatile int value = input + 9;
  return value;
}

namespace {
volatile std::sig_atomic_t changed = 0;
void request_change(int) { changed = 1; }
}

int main(int argc, char **) {
  std::signal(SIGUSR1, request_change);
  std::printf("entries good=%p indirect=%p direct=%p named=%p\n",
              reinterpret_cast<void *>(&verified_entry),
              reinterpret_cast<void *>(&indirect_entry),
              reinterpret_cast<void *>(&direct_entry),
              reinterpret_cast<void *>(&named_entry));
  std::fflush(stdout);
  volatile int result = named_entry(argc);
  (void)result;
  unsigned stage = 0;
  for (;;) {
    if (changed) {
      changed = 0;
      const auto page_size = ::sysconf(_SC_PAGESIZE);
      if (page_size <= 0)
        return 1;
      if (++stage == 1) {
        if (::mmap(nullptr, page_size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
          return 2;
        std::puts("mappings changed");
      } else if (stage == 2) {
        const auto address = reinterpret_cast<std::uintptr_t>(&verified_entry);
        auto *page = reinterpret_cast<void *>(address - address % page_size);
        if (::mprotect(page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
          return 3;
        *reinterpret_cast<volatile unsigned char *>(address) = 0x90;
        if (::mprotect(page, page_size, PROT_READ | PROT_EXEC) != 0)
          return 4;
        std::puts("bytes changed");
      }
      std::fflush(stdout);
    }
    ::usleep(10000);
  }
}

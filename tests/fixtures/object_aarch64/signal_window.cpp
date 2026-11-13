#include "rendezvous.hpp"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ucontext.h>
#if defined(__aarch64__)
#include <asm/sigcontext.h>
#include <sys/prctl.h>
#endif

namespace signal_test {
struct Record {
  int value;
  Record();
  ~Record();
};

__attribute__((noinline, noipa)) Record::Record() : value(17) {}
__attribute__((noinline, noipa)) Record::~Record() { asm volatile("nop" : : "r"(this) : "memory"); }

extern "C" int signal_window_write(Record &, int);
extern "C" void signal_window_successor();
extern "C" void signal_window_return();
extern "C" int signal_window_dispatch(Record &, int);
extern "C" void signal_dispatch_leaf();
extern "C" void signal_dispatch_continuation();
extern "C" int signal_window_tail(Record &, int);
extern "C" void signal_tail_continuation();
asm(".pushsection .text\n"
    ".globl signal_window_tail, signal_tail_continuation\n"
    ".type signal_window_tail, %function\n"
#if defined(__aarch64__)
    "signal_window_tail: str x30, [sp, #-16]!\n"
    "bl signal_tail_relay\n"
    "signal_tail_continuation: ldr x30, [sp], #16\n"
#else
    "signal_window_tail: subq $8, %rsp\n"
    "call signal_tail_relay\n"
    "signal_tail_continuation: addq $8, %rsp\n"
#endif
    "ret\n"
    ".size signal_window_tail, .-signal_window_tail\n"
    ".type signal_tail_relay, %function\n"
#if defined(__aarch64__)
    "signal_tail_relay: b signal_dispatch_leaf\n"
#else
    "signal_tail_relay: jmp signal_dispatch_leaf\n"
#endif
    ".size signal_tail_relay, .-signal_tail_relay\n"
    ".popsection\n");
#if defined(__aarch64__)
extern "C" int signal_window_sve(Record &, int);
extern "C" void signal_sve_store(), signal_sve_successor();
extern "C" int signal_window_sme(Record &, int);
extern "C" void signal_sme_successor();
asm(".pushsection .text\n"
    ".arch_extension sme2\n"
    ".globl signal_window_sve, signal_sve_store, signal_sve_successor\n"
    ".type signal_window_sve, %function\n"
    "signal_window_sve: mov z0.b, #42\n"
    "ptrue p0.b\n"
    "signal_sve_store: str w1, [x0]\n"
    "signal_sve_successor: lastb w0, p0, z0.b\n"
    "ret\n"
    ".size signal_window_sve, .-signal_window_sve\n"
    ".globl signal_window_sme, signal_sme_successor\n"
    ".type signal_window_sme, %function\n"
    "signal_window_sme: smstart\n"
    "zero {za}\n"
    "zero {zt0}\n"
    "mov z0.b, #42\n"
    "ptrue p0.b\n"
    "str w1, [x0]\n"
    "signal_sme_successor: lastb w0, p0, z0.b\n"
    "smstop\n"
    "ret\n"
    ".size signal_window_sme, .-signal_window_sme\n"
    ".popsection\n");
#endif

asm(".pushsection .text\n"
    ".globl signal_window_dispatch, signal_dispatch_continuation\n"
    ".type signal_window_dispatch, %function\n"
#if defined(__aarch64__)
    "signal_window_dispatch: str x30, [sp, #-16]!\n"
    "bl signal_dispatch_leaf\n"
    "signal_dispatch_continuation: ldr x30, [sp], #16\n"
#else
    "signal_window_dispatch: subq $8, %rsp\n"
    "call signal_dispatch_leaf\n"
    "signal_dispatch_continuation: addq $8, %rsp\n"
#endif
    "ret\n"
    ".size signal_window_dispatch, .-signal_window_dispatch\n"
    ".globl signal_dispatch_leaf\n"
    ".type signal_dispatch_leaf, %function\n"
    "signal_dispatch_leaf:\n"
#if defined(__aarch64__)
    "str w1, [x0]\n"
    "add w0, w1, #1\n"
#else
    "movl %esi, (%rdi)\n"
    "leal 1(%rsi), %eax\n"
#endif
    "ret\n"
    ".size signal_dispatch_leaf, .-signal_dispatch_leaf\n"
    ".popsection\n");

asm(".pushsection .text\n"
    ".globl signal_window_write, signal_window_successor, signal_window_return\n"
    ".type signal_window_write, %function\n"
    "signal_window_write:\n"
#if defined(__aarch64__)
    "str w1, [x0]\n"
    "signal_window_successor: add w0, w1, #1\n"
#else
    ".rept 5\n nop\n .endr\n"
    "movl %esi, (%rdi)\n"
    "signal_window_successor: leal 1(%rsi), %eax\n"
#endif
    "signal_window_return: ret\n"
    ".size signal_window_write, .-signal_window_write\n"
    ".popsection\n");
}

namespace {
bool after_store;
#if !defined(__aarch64__)
bool at_return;
#endif
bool at_dispatch;
bool at_tail;
#if defined(__aarch64__)
unsigned sve_length;
bool streaming;
volatile sig_atomic_t sve_pc, sve_records;
std::uintptr_t sve_interrupted;

bool edit_sve(mcontext_t &machine) {
  bool fp_ok = false, sve_ok = false, matrix_ok = !streaming, zt_ok = !streaming;
  for (unsigned offset = 0; offset + 16 <= sizeof(machine.__reserved);) {
    auto *record = machine.__reserved + offset;
    const auto &head = *reinterpret_cast<const _aarch64_ctx *>(record);
    if (head.size < 16 || head.size > sizeof(machine.__reserved) - offset)
      break;
    if (head.magic == FPSIMD_MAGIC && head.size == sizeof(fpsimd_context)) {
      auto &fp = *reinterpret_cast<fpsimd_context *>(record);
      auto *lane = reinterpret_cast<unsigned char *>(&fp.vregs[0]) + 15;
      fp_ok = *lane == 42;
      if (sve_length == 16)
        *lane = 44;
    }
    if (head.magic == SVE_MAGIC && head.size >= SVE_SIG_CONTEXT_SIZE(sve_vq_from_vl(sve_length))) {
      auto &sve = *reinterpret_cast<sve_context *>(record);
      auto *lane = record + SVE_SIG_REGS_OFFSET + sve_length - 1;
      sve_ok = sve.vl == sve_length && sve.flags == (streaming ? SVE_SIG_FLAG_SM : 0) && *lane == 42;
      *lane = 44;
    }
    if (streaming && head.magic == ZA_MAGIC && head.size == ZA_SIG_CONTEXT_SIZE(sve_vq_from_vl(sve_length))) {
      auto &za = *reinterpret_cast<za_context *>(record);
      matrix_ok = za.vl == sve_length && record[ZA_SIG_REGS_OFFSET] == 0;
    }
    if (streaming && head.magic == ZT_MAGIC && head.size == ZT_SIG_CONTEXT_SIZE(1)) {
      auto &zt = *reinterpret_cast<zt_context *>(record);
      zt_ok = zt.nregs == 1 && *(record + sizeof(zt)) == 0;
    }
    offset += head.size;
  }
  return fp_ok && sve_ok && matrix_ok && zt_ok;
}
#endif
volatile sig_atomic_t correct_pc;
void handle_signal(int, siginfo_t *, void *raw) {
  auto &machine = static_cast<ucontext_t *>(raw)->uc_mcontext;
#if defined(__aarch64__)
  const auto pc = machine.pc;
  machine.regs[1] = 43;
  const auto continuation = machine.regs[30];
  if (sve_length) {
    sve_interrupted = pc;
    sve_pc = pc == reinterpret_cast<std::uintptr_t>(streaming ? &signal_test::signal_sme_successor :
                                                               &signal_test::signal_sve_successor);
    sve_records = edit_sve(machine);
    correct_pc = sve_pc && sve_records;
    return;
  }
#else
  const auto pc = machine.gregs[REG_RIP];
  if (at_return) {
    machine.gregs[REG_RAX] = 44;
    correct_pc = static_cast<std::uintptr_t>(pc) ==
        reinterpret_cast<std::uintptr_t>(&signal_test::signal_window_return);
    return;
  }
  machine.gregs[REG_RSI] = 43;
  const auto continuation = at_dispatch ? *reinterpret_cast<const std::uintptr_t *>(machine.gregs[REG_RSP]) : 0;
#endif
  if (at_dispatch) {
    const auto caller = reinterpret_cast<std::uintptr_t>(
        at_tail ? &signal_test::signal_tail_continuation : &signal_test::signal_dispatch_continuation);
    correct_pc = static_cast<std::uintptr_t>(pc) ==
                     reinterpret_cast<std::uintptr_t>(&signal_test::signal_dispatch_leaf) &&
                 continuation == caller;
    return;
  }
  const auto expected = after_store ? reinterpret_cast<std::uintptr_t>(&signal_test::signal_window_successor) :
      reinterpret_cast<std::uintptr_t>(&signal_test::signal_window_write);
  correct_pc = static_cast<std::uintptr_t>(pc) == expected;
}
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  after_store = !std::strcmp(argv[1], "after");
#if defined(__aarch64__)
  streaming = !std::strcmp(argv[1], "sme16");
  if (streaming || !std::strncmp(argv[1], "sve", 3)) {
    sve_length = !std::strcmp(argv[1], "sve64") ? 64 : 16;
    if (::prctl(streaming ? PR_SME_SET_VL : PR_SVE_SET_VL, sve_length, 0, 0, 0) !=
        static_cast<int>(sve_length))
      return 3;
    after_store = true;
  }
#endif
#if !defined(__aarch64__)
  at_return = !std::strcmp(argv[1], "return");
  after_store |= at_return;
#endif
  at_dispatch = !std::strcmp(argv[1], "dispatch");
  at_tail = !std::strcmp(argv[1], "tail");
  at_dispatch |= at_tail;
  struct sigaction action{};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = handle_signal;
  if (::sigaction(SIGUSR1, &action, nullptr))
    return 2;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
#if defined(__aarch64__)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
    ::usleep(1000);
#endif
  if (at_tail) {
    {
      signal_test::Record warmup;
      if (signal_test::signal_window_tail(warmup, 23) != 24)
        return 4;
    }
    std::puts("WARMED");
    while (::access("tail.now", F_OK))
      ::usleep(1000);
  }
  signal_test::Record record;
#if defined(__aarch64__)
  const auto result = streaming ? signal_test::signal_window_sme(record, 23) :
      sve_length ? signal_test::signal_window_sve(record, 23) :
      at_tail ? signal_test::signal_window_tail(record, 23) :
      at_dispatch ? signal_test::signal_window_dispatch(record, 23) : signal_test::signal_window_write(record, 23);
#else
  const auto result = at_tail ? signal_test::signal_window_tail(record, 23) :
      at_dispatch ? signal_test::signal_window_dispatch(record, 23) : signal_test::signal_window_write(record, 23);
#endif
  std::printf("DONE pc_ok=%d value=%d result=%d\n", int(correct_pc), record.value, result);
#if defined(__aarch64__)
  if (sve_length && !correct_pc)
    std::printf("SVE pc=%d records=%d interrupted=%p\n", int(sve_pc), int(sve_records),
                reinterpret_cast<void *>(sve_interrupted));
#endif
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return !correct_pc || record.value != (after_store ? 23 : 43) || result != 44;
}

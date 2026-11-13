#include "record.hpp"

#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>

#if defined(TRACE_TEST_RUNTIME_DSO)
#include <dlfcn.h>
#include <thread>
#endif

namespace demo {
#if defined(TRACE_TEST_MULTI_INDIRECT)
constexpr int expected = 19;
#else
constexpr int expected = 23;
#endif

[[gnu::noinline]] void indirect_writer(PlanRecord &record) {
  asm volatile("nop\nnop\nnop\nnop\nnop" ::: "memory");
  auto *aliased = &record.value;
  *aliased = expected;
}

#if defined(TRACE_TEST_MULTI_INDIRECT)
[[gnu::noinline]] void indirect_again(PlanRecord &record) {
  asm volatile("nop\nnop\nnop\nnop\nnop" ::: "memory");
  record.value = 23;
}
#endif

using Writer = void (*)(PlanRecord &);
Writer volatile selected_writer = indirect_writer;

#if defined(TRACE_TEST_RUNTIME_TAIL)
extern "C" void trace_tail_relay(PlanRecord &);
asm(".pushsection .text\n"
    ".type trace_tail_relay, %function\n"
    "trace_tail_relay:\n"
#if defined(__aarch64__)
    "mov x16, #23\n"
    "b trace_tail_value\n"
#else
    "movl $23, -8(%rsp)\n"
    "jmp trace_tail_value\n"
#endif
    ".size trace_tail_relay, .-trace_tail_relay\n"
    ".type trace_tail_value, %function\n"
    "trace_tail_value:\n"
    ".cfi_startproc\n"
#if defined(__aarch64__)
    "str w16, [x0]\n"
#else
    "movl -8(%rsp), %eax\n"
    "movl %eax, (%rdi)\n"
#endif
    "ret\n"
    ".cfi_endproc\n"
    ".size trace_tail_value, .-trace_tail_value\n"
    ".popsection\n");
#endif

#if defined(TRACE_TEST_RUNTIME_NESTED) || defined(TRACE_TEST_RUNTIME_DISCOVERED_NONLEAF)
[[gnu::noinline]] void nested_relay(PlanRecord &record) {
  write_value(record);
  asm volatile("" ::: "memory");
}
#endif
#if defined(TRACE_TEST_RUNTIME_DISCOVERED_NONLEAF)
[[gnu::noinline]] void discovered_relay(PlanRecord &record) {
  static Writer volatile selected_relay = nested_relay;
  const auto relay = selected_relay;
  asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop" ::: "memory");
  relay(record);
  asm volatile("" ::: "memory");
}
#endif

[[gnu::noinline]] int nonleaf_dispatch(PlanRecord &record) {
#if defined(TRACE_TEST_RUNTIME_TAIL)
  trace_tail_relay(record);
#elif defined(TRACE_TEST_RUNTIME_NESTED)
  nested_relay(record);
#elif defined(TRACE_TEST_RUNTIME_DIRECT)
  write_value(record);
#else
  const auto writer = selected_writer;
  writer(record);
#endif
  return record.value;
}
} // namespace demo

#if defined(TRACE_TEST_RUNTIME_DSO)
#if defined(TRACE_TEST_RUNTIME_SELECTED_DSO) || defined(TRACE_TEST_RUNTIME_DISCOVERED_DSO)
constexpr int runtime_argc = 2;
#else
constexpr int runtime_argc = 3;
#endif
int main(int argc, char **argv) {
  if (argc != runtime_argc)
    return 5;
#else
int main() {
#endif
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
#if defined(TRACE_TEST_RUNTIME_SELECTED_DSO)
  void *library = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!library)
    return 6;
  const auto writer = reinterpret_cast<demo::Writer>(::dlsym(library, "plugin_nested"));
  if (!writer)
    return 7;
  demo::selected_writer = writer;
#endif
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
#if defined(TRACE_TEST_RUNTIME_DISCOVERED_DSO)
  demo::Writer previous_writer = nullptr;
  void *discovered_library = nullptr;
#endif
  for (;;) {
    char command;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 0 || (size == 1 && command == 'q')) {
#if defined(TRACE_TEST_RUNTIME_SELECTED_DSO) || defined(TRACE_TEST_RUNTIME_DISCOVERED_DSO)
#if defined(TRACE_TEST_RUNTIME_SELECTED_DSO)
      if (::dlclose(library))
        return 8;
#else
      if (discovered_library && ::dlclose(discovered_library))
        return 8;
      const auto writer = previous_writer;
#endif
      Dl_info resident{};
      for (unsigned attempt = 0; ::dladdr(reinterpret_cast<void *>(writer), &resident); ++attempt) {
        if (attempt == 1000)
          return 9;
        ::usleep(1000);
      }
#endif
      return 0;
    }
    if (size < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    if (command == 'g') {
      demo::PlanRecord record(17);
#if defined(TRACE_TEST_RUNTIME_DSO) && !defined(TRACE_TEST_RUNTIME_SELECTED_DSO)
#if defined(TRACE_TEST_RUNTIME_DISCOVERED_DSO)
#if defined(TRACE_TEST_RUNTIME_NAMESPACE)
      discovered_library = ::dlmopen(LM_ID_NEWLM, argv[1], RTLD_NOW | RTLD_LOCAL);
#else
      discovered_library = ::dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
      if (!discovered_library)
        return 6;
#if defined(TRACE_TEST_RUNTIME_NAMESPACE)
      Lmid_t namespace_id = LM_ID_BASE;
      if (::dlinfo(discovered_library, RTLD_DI_LMID, &namespace_id) || namespace_id == LM_ID_BASE)
        return 10;
#endif
      const auto writer = reinterpret_cast<demo::Writer>(::dlsym(discovered_library, "plugin_nested"));
      if (!writer)
        return 7;
      demo::selected_writer = writer;
      int result = 0;
      std::thread worker([&] { result = demo::nonleaf_dispatch(record); });
      worker.join();
      demo::selected_writer = demo::indirect_writer;
      previous_writer = writer;
#else
      const char *modules[] = {argv[1], argv[2], argv[1]};
      const int values[] = {23, 19, 23};
      for (unsigned index = 0; index < 3; ++index) {
#if defined(TRACE_TEST_RUNTIME_NAMESPACE)
        void *library = ::dlmopen(LM_ID_NEWLM, modules[index], RTLD_NOW | RTLD_LOCAL);
#else
        void *library = ::dlopen(modules[index], RTLD_NOW | RTLD_LOCAL);
#endif
        if (!library)
          return 6;
#if defined(TRACE_TEST_RUNTIME_NAMESPACE)
        Lmid_t namespace_id = LM_ID_BASE;
        if (::dlinfo(library, RTLD_DI_LMID, &namespace_id) || namespace_id == LM_ID_BASE)
          return 10;
#endif
        const auto writer = reinterpret_cast<demo::Writer>(::dlsym(library, "plugin_write"));
        if (!writer)
          return 7;
        demo::selected_writer = writer;
        int result = 0;
        std::thread worker([&] { result = demo::nonleaf_dispatch(record); });
        worker.join();
        demo::selected_writer = demo::indirect_writer;
        if (result != values[index] || ::dlclose(library))
          return 8;
        Dl_info resident{};
        if (::dladdr(reinterpret_cast<void *>(writer), &resident))
          return 9;
      }
      const auto result = record.value;
#endif
#else
#if defined(TRACE_TEST_MULTI_INDIRECT)
      demo::selected_writer = demo::indirect_again;
      if (demo::nonleaf_dispatch(record) != 23)
        return 4;
      demo::selected_writer = demo::indirect_writer;
#endif
#if defined(TRACE_TEST_RUNTIME_DISCOVERED_NONLEAF)
      demo::selected_writer = demo::discovered_relay;
#endif
      const auto result = demo::nonleaf_dispatch(record);
#endif
      std::printf("WROTE value=%d\n", result);
      if (result != demo::expected)
        return 4;
    }
  }
}

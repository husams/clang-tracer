#pragma once

#include <cstddef>
#include <cstdint>

struct dl_phdr_info;

struct SharedStackState {
  volatile std::uint64_t *calls;
  volatile std::uint64_t *failures;
  volatile int *errno_address;
};

class __attribute__((visibility("default"))) SharedStackB {
public:
  __attribute__((noinline)) void run(SharedStackState *state) const;
};

extern "C" __attribute__((visibility("default"), noinline)) void
shared_stack_enter(SharedStackState *state);

extern "C" __attribute__((visibility("default"), noinline)) int
shared_stack_runtime_callback(dl_phdr_info *info, std::size_t size,
                              void *context);

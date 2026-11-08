#pragma once

#define CLANG_TRACER_OVERRIDE_SECTION "clang_tracer_override_targets"

// Place this on the definition of a free function built with Clang and DWARF.
// It declares the ordinary SysV scalar ABI, including its register preservation
// rules. Do not combine it with other calling-convention or register attributes.
#if defined(__clang__) && defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#define TRACER_SCALAR_OVERRIDE_TARGET                                         \
  __attribute__((sysv_abi, noinline, used, section(CLANG_TRACER_OVERRIDE_SECTION)))
#endif

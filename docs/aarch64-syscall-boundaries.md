# AArch64 syscall boundary audit

Follow-up to [issue #124](https://github.com/husams/clang-tracer/issues/124).

The native AArch64 syscall stub is eight bytes: `SVC #0; BRK #0`.
The admitted PC must leave room for both instructions and the eight-byte
saved-byte read in executable memory. The x86-64 workaround in #117 does not
establish AArch64 reachability.

## Entry paths

The current production paths preserve this invariant:

| Entry | Admission and preservation |
| --- | --- |
| `Process::attach`, leader or promoted worker | `attach_impl` requires a readable executable mapping containing the original four-byte SVC and the complete eight-byte stub after it. The SVC is stepped only after this check; the resulting PC must be exactly its successor. A sleep whose successor is the final four-byte instruction is rejected. |
| `Process::attach_at_application_point` | The caller supplies a verified executable rendezvous range. Admission requires eight bytes remaining in that range at the stopped PC. `NativeWatch::start` derives the range from the object plan and compares its live bytes with the verified ELF before invoking the agent loader. |
| `Process::adopt_executable_entry` | `verify_launch_entry` verifies eight file-backed bytes in both the executable ELF segment and its readable executable mapping. This applies to both AT_ENTRY and the glibc loader-ready checkpoint. Early loader notifications are stepped before adoption; adoption occurs back at the verified entry. |
| Patch-only attachment | `attach_for_patch` can stop at arbitrary PCs, but `remote_syscall` and `remote_call` reject this mode before changing code or registers. AArch64 `NativeWatch::stop` and method-watch stop use this mode. |

`remote_allocate` calls mmap; `remote_map_file` calls openat, mmap and close
(including its cleanup); `remote_free` calls munmap. Remote-call setup uses
the same allocator for its scratch stack and call page, then invokes mprotect.
Detach frees those allocations through the same syscall path. Each syscall
restores the original PC, general and extended registers, syscall number and
signal mask. Remote calls also restore their saved PC. A failed restoration
poisons the process and prevents further injection. Code patching and peer
freezing do not advance the admitted control thread.

No production path identified by this audit admits a syscall-capable AArch64
attachment with only four executable bytes remaining. The syscall stub and
admission checks therefore stay unchanged. Native execution did expose a
separate method-watch initialization defect: the ARM snapshot compiler used
the generated receiver expression without its generated storage declarations.
Prepending `receiver_storage` fixes that error and lets the two-probe teardown
regression reach the stop operation.

## Native regression coverage

On an AArch64 Linux build, run:

```sh
ctest --test-dir BUILD --output-on-failure -R native_aarch64
```

`native_aarch64_syscall_boundary` uses file-backed executable text followed by
a PROT_NONE page. Both sleep and rendezvous paths exercise a PC eight bytes
before the boundary and repeatedly reject the corresponding four-byte case.
For sleep, the rejected site is the SVC whose successor would leave only four
bytes; admission rejects it before taking that step. The ptrace wrapper
confirms the target reached the selected SVC or rendezvous PC.
It checks mmap, munmap, file mapping, call-page setup, a remote call, syscall
error recovery, complete text restoration, general/FP registers, syscall
number, signal mask, successful detach and target progress. It also checks
that patch-only attachment cannot allocate remotely.

With `TRACER_BUILD_REPL=ON`, `native_aarch64_two_method_stop` also prepares one
method-snapshot watch with exactly two methods, matching #98's watch shape.
Eight start, SQLite-record and stop cycles alternate the normal snapshot
budget with a small budget. Every cycle restores both method probes and all
live redirect bytes, keeps the same session attached and confirms target
progress. The fixture sleeps at the same admitted executable boundary and
uses a vector to exercise ordinary C++ container snapshots.

These tests require an AArch64 Linux kernel executing on ARM hardware;
x86-64 tests and instruction-set emulation cannot substitute for that run.

## Verification on 2026-11-18

The two focused tests passed on Linux 6.8.0-90-generic, AArch64, GCC 13.3.0
and LLVM 22.1.8. The kernel ran in an Apple Virtualization.framework VM on an
ARM Mac (Lima `vz`, `aarch64`; CPU vendor Apple). This executes native ARM
instructions under hardware virtualization. It is not an x86-64 or emulated
AArch64 result. The CPU exposes FP/SIMD but not SVE/SME, so the latter register
sets were not exercised.

Two mutation checks independently weakened the sleep and rendezvous admission
conditions to permit four bytes. Each caused the new boundary test to fail
with `attachment admitted an incomplete syscall stub at the boundary`.
The original conditions were restored and the boundary test passed again.
Removing the receiver-storage fix also reproduced the method-watch
initialization failure with an undeclared `snapshot_receiver`; restoring it
let all eight stop cycles pass.

The base revision `abb467ca` could not build this AArch64 configuration:
glibc's `elf.h` omitted three SME note constants, an x86-only collection helper
was compiled on ARM, module enumeration referenced the x86 CFI admission
module, and the ARM server omitted private LLVM include paths. The small
platform build repairs in this change allow native validation. They retain
the existing syscall admission conditions and stub placement.

All eight relevant native ARM tests passed: the two new tests plus startup,
syscall-output, template, startup-code-cache, preinit-code-cache and
loader-DSO-code-cache. The startup tests launch a child, detach their original
controller, then inspect it from a second controller; they require ptrace
permission for that inspection. The isolated test VM temporarily used
`ptrace_scope=0` for this run and restored its original setting afterward.
At the default `ptrace_scope=1`, those later reads report a build-ID mismatch
because access is denied. The new fixture explicitly permits its tracing
controller, and the focused tests also run at `ptrace_scope=1`.

For x86-64, GCC 8.5 preprocessing of the three changed implementation files
compiled on that architecture (`injector.cpp`, `object_code_cache.cpp`,
`object_watch_plan.cpp`) produces
byte-identical output to the base revision with line markers removed. The
AArch64 build repairs do not change x86-64 executable logic. There is no
dependency on the separate alias or constructor/destructor feature work in
issues #118 and #119.

The GCC 8.5 tracer also passed eight two-method snapshot-watch cycles against
the private application in `~/banking-fixture`: 96 SQLite-recorded events,
both method probes' live bytes restored each time, the same attached session,
continued application progress, and unchanged application/library ELF hashes.
The application's C++23/GCC 15 header profile was supplied explicitly during
preparation; the tracer itself was built by GNU GCC 8.5.0. No Banking source
or generated private adapters are included here.

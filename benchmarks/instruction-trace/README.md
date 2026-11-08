# Bounded instruction tracing pilot

**Decision for P4 A2.** Keep Gum Stalker as an experimental backend. It was
faster on the standalone fixture and passed live attachment with an existing
native observer. The pilot has a source and relinking bundle. Automatic
loader selection, a handled signal, guard-page recovery and warmed probe
changes passed on both glibc profiles. A maintained backend still needs
broader validation.
Native single-step remains the fixture reference.

## Reproduce

Use Linux x86-64 with a C++17 compiler, Python 3.8 or newer, GNU binutils,
and permission to trace a child you own. Obtain the
`frida-gum-devkit-17.26.1-linux-x86_64.tar.xz` release asset. Verify
SHA-256 `4e7247a5f990b15d319c997836af2f6fea38fccb493dc7f6b4b6142009df040e`
before extracting it. [Frida 17.26.1 release](https://github.com/frida/frida/releases/tag/17.26.1)

```sh
benchmarks/instruction-trace/build.sh /tmp/instruction-pilot /path/to/gum-devkit
python3 benchmarks/instruction-trace/compare.py /tmp/instruction-pilot --cpu 3
```

Choose a CPU from the current affinity mask, or omit `--cpu`. Set `CC` and
`CXX` to select a toolchain. For the GCC 8.5 SDK, set `PILOT_LDFLAGS` to
`-Wl,--dynamic-linker,LOADER,-rpath,LIBRARY_PATH` to link the SDK loader and
runtime directories into the fixture. The runner can also invoke a loader
directly with `--loader PATH --library-path PATHS` for comparison. Build
with `-` in place of the devkit path and run with `--native-only` to run
the reference without Gum. The pilot has a standalone build.

## Method

Each trial runs baseline, native, Gum and post-teardown phases on the same
thread and instruction addresses in one owned PIE child. The x86-64 leaf
workload exercises both conditional branch outcomes, a direct call, an
indirect call, returns and a loop. Each instruction has an ELF label. The
runner derives an expected PC sequence from those labels and checks every
record independently of either backend. The signal case sends SIGUSR1
to the running thread through `tgkill` inside the recorded range.

The native backend steps from a fixture-owned trap, observes original PCs,
forwards the fixture's SIGUSR1 and detaches at the exit or budget boundary.
Stalker follows the same thread and records original PCs through internal
callouts inserted only in the fixture's 65-byte range. It requests unfollow
on the first excess instruction and drains its translation context before
releasing capture storage. Both report truncation explicitly. Each phase
has a five-second watchdog, with an additional step budget for native
capture. A failing phase terminates and reaps its owned child. [Linux ptrace
manual](https://man7.org/linux/man-pages/man2/ptrace.2.html) · [Stalker
design](https://frida.re/docs/stalker/) · [Pinned x86 implementation](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/backend-x86/gumstalker-x86.c)

## Results on linux-dev

Rocky Linux 9.8 x86-64, kernel 5.14.0-687.17.1.el9_8, Intel i9-7900X under
KVM. Built with Clang 21.1.8 and the GCC 8.5 SDK. The timed fixture used the
SDK glibc 2.23 ELF interpreter and SDK libraries. Nine trials pinned to
CPU 3 each executed 256 loop iterations and recorded 1,929 instructions
per backend.

| Phase | Median workload and trace time | Median request-to-reply time |
| --- | ---: | ---: |
| Baseline | 0.758 µs | 0.085 ms |
| Native ptrace | 23.664 ms | 23.704 ms |
| Gum Stalker | 1.200 ms | 5.073 ms |
| Baseline after Gum unload | 0.742 µs | 0.833 ms |

Workload time includes the native stepping window or Gum follow and unfollow.
Request-to-reply also includes IPC, native attach or Gum initialization and
garbage collection, and Gum unload in the final baseline. Gum took about
one twentieth of the native workload time and one fifth of its request time
on this fixture.

Both matched the 32-instruction plain trace and the 37-instruction signal
trace exactly. SIGUSR1 ran once in both modes. A seven-record cap returned
the same prefix and reported truncation while the workload and its signal
completed. The exact 32-record cap reported a complete trace. The final
baseline ran with the same return value and signal count, followed by a
clean child exit. A stalled test backend exercised the watchdog and the
child was reaped in five seconds. The standalone native-only run also passed.

The host glibc 2.34 run passed. In an earlier glibc 2.23 run with the loader
invoked directly against the normally linked executable, Gum reported
`Unable to locate the libc` once after the baseline and native phases.
The SDK-interpreter build passed all nine timed trials. A separate
direct-loader run of that SDK-interpreter build passed 20 trials.
These are distinct launch configurations. [Gum libc discovery](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/backend-linux/gumprocess-linux.c#L1316)

## Live attachment with a native probe

Build the pilot as above with Gum, and supply the paths to a built native
REPL, server, and agent. Keep debug symbols on `live-target` so the REPL can
resolve the fixture's C++ function signatures. The pilot also verifies the
ELF metadata of a stripped plugin after moving it to a staging directory.

```sh
mkdir -p /tmp/instruction-stage
strip --strip-unneeded -o /tmp/instruction-stage/gum-backend.so \
  /tmp/instruction-pilot/gum-backend.so
python3 benchmarks/instruction-trace/live_compare.py \
  "$REPL" "$SERVER" "$AGENT" /tmp/instruction-pilot/live-target \
  /tmp/instruction-stage/gum-backend.so
```

The runner starts a C++ target and waits for its worker to come up. It then
attaches with `clang-tracer-repl --attach`, installs a Clang JIT PRE and POST
observer, and installs a temporary control observer. The control observer
loads the plugin through `dlopen` and follows the existing worker. The
instruction range is the same 65-byte leaf used by the standalone pilot,
reached through the actively probed caller on that worker.

After replacing the private glibc import, three runs on SDK glibc 2.23 and
four on Rocky Linux 9.8 glibc 2.34 each matched 56 instruction PCs and a
seven-PC prefix with explicit truncation.
Each verified six native PRE and POST pairs before, during, and after Gum,
including typed arguments and unchanged return values. The native detour
bytes remained intact through both Gum unloads. The plugin disappeared
from `/proc/PID/maps` after each capture, and REPL detach restored the
original entry bytes. The standalone comparison smoke cases and three
focused native REPL CTests passed in the same SDK environment.

The default Gum trust threshold captured the expected PCs but could time
out waiting for the worker to unfollow. The live pilot sets the threshold
to `-1` so already translated blocks continue checking for unfollow at
dispatch boundaries. On timeout, the pilot keeps the plugin and capture
storage resident. Gum temporarily authorizes its own tracing helper on
Linux, so the control hook restores this fixture's explicit authorization
after start and stop. The host had `kernel.yama.ptrace_scope=0`.
[Pinned x86 source](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/backend-x86/gumstalker-x86.c),
[Pinned Linux source](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/backend-linux/gumprocess-linux.c#L792)

Run the same REPL, server, agent, and relocated plugin against both
fixtures. Build the SDK fixture with its SDK interpreter and runtime
paths. Omit `PILOT_LDFLAGS` to build the host fixture with the host
interpreter. Invoke each fixture directly using the runner above.
The runner checks that the agent and fixture report the same glibc
version and records whether libdl was already mapped before attach.

The injector resolves loader functions from the target's mapped glibc
and verifies their live entry bytes. Before loading an SDK-built agent,
it loads any missing glibc companion libraries required by the agent
from beside the target's libc. The agent then uses those copies rather
than libraries from an incompatible SDK runtime. Both fixtures can
start without libdl. glibc 2.34 provides the public loader functions in
libc, while the legacy SDK can bootstrap through its mapped libc when
libdl is absent. [glibc 2.34 release notes](https://sourceware.org/pipermail/libc-announce/2021/000032.html)

Launch the fixture as an executable with its linked interpreter. An
explicit loader command makes `/proc/PID/exe` identify the loader, which
fails native attach admission.
The stock GCC 11.5 debug build attached and ran the native observer, then
failed to resolve the control observer's signature. A fresh host-linked
Clang 21.1.8 build passed the same live run.

### Signals, faults and probe changes

The live runner also follows the worker while it warms an unprobed
`pilot_observed(int)`, installs a native observer, reloads the observer
with a distinct generation marker, and removes it. It changes probes
between worker calls while keeping the same Gum capture active. It checks
the entry bytes after each change, the PRE and POST call IDs and generation
markers, and the absence of native events after removal. Gum still records
the work after the original entry bytes have been restored.

While followed, the worker sends itself one `SIGUSR1` with `tgkill` from
inside the recorded leaf. It also reads a `PROT_NONE` guard page from a
two-instruction leaf. The application handler checks the fault address and
`SEGV_ACCERR`, restores read access and returns so the faulting load retries.
The fixture compares signal counts and return values before the capture,
during it, and after detach. The combined capture matched all 151
instruction offsets with one signal, one recovered fault and ten native
PRE and POST pairs across the live trial. [Linux sigaction](https://man7.org/linux/man-pages/man2/sigaction.2.html)

The initial test exposed a native agent issue. Clang's temporary PCH output
registered LLVM's process-wide signal handlers and replaced the fixture's
`SIGUSR1` handler. The PCH cache already owns a private generation path,
so it now writes there directly and publishes only validated artifacts.
[Clang output handling](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/clang/lib/Frontend/CompilerInstance.cpp#L804),
[LLVM temporary-file registration](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/llvm/lib/Support/Path.cpp#L1325)

## Packaging and distribution

The native-only fixture added no third-party library. The Gum devkit download
was 12,791,284 bytes and its static archive was 87,510,768 bytes. The SDK-linked
Gum module was 23,109,304 bytes before stripping and 10,319,976 bytes after
stripping. The native executable was 27,248 bytes after stripping.
The original SDK GNU linker 2.30 emitted 564 unsupported GNU property
warnings from the devkit objects. The first source relink emitted 298
from the retained objects. The pilot now uses host GNU BFD 2.35.2 with the
SDK Clang driver and SDK sysroot for the plugin link. The original archive
and rebuilt libraries link with zero warnings. Linker warnings fail the
build, and the devkit's object notes are preserved in the link inputs.
[Clang linker selection](https://clang.llvm.org/doxygen/ToolChain_8cpp_source.html),
[GNU ld warning handling](https://sourceware.org/binutils/docs/ld.html)

The updated live SDK module was 10,322,360 bytes after stripping. It exposed five
pilot symbols and had full RELRO, immediate binding and a nonexecutable
stack. Its dynamic section had no search path or text relocations. Its
runtime dependencies were libc, libdl, libgcc_s, libm, libpthread and
libresolv. The newest public glibc symbol version was `GLIBC_2.17`. The
devkit's `gum.c.o` originally introduced a weak
`__libc_dlopen_mode@GLIBC_PRIVATE` import. The pilot links that reference to
a hidden wrapper that pins libdl through public `dlopen` and balances its
reference during Gum shutdown. Both the build and live runner reject
`GLIBC_PRIVATE` in the plugin's ELF versions. The updated staged plugin
passed that check and the previous plugin failed it.
[Pinned Gum initialization](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/gum.c#L538)

The [license audit](GUM_LICENSE_AUDIT.md) identifies 819 archive members
and 282 members linked into the original pilot, including LGPL GLib code.
The relinking bundle contains the license texts, original source notices,
pinned source trees, and 149 retained Gum, Capstone and liblzma objects.
It builds the plugin from replacement GLib, GObject, libffi and PCRE2
libraries.

From the repository root on Linux x86-64, use the verified SDK compiler
and Meson to assemble the bundle and produce an archive.

```sh
CC="$SDK/bin/clang" CXX="$SDK/bin/clang++" \
  benchmarks/instruction-trace/package-relink.sh "$GUM_DEVKIT_TARBALL" \
  gum-stalker-pilot-linux-x86_64
tar -cJf gum-stalker-pilot-linux-x86_64.tar.xz gum-stalker-pilot-linux-x86_64
```

See [Relink the Gum Stalker pilot](RELINK.md) for recipient instructions.

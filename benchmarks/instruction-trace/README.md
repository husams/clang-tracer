# Bounded instruction tracing pilot

**Decision for P4 A2:** Hold a maintained instruction-tracing backend. Keep
Gum Stalker as the candidate for an integration experiment and native
single-step as a fixture reference. Gum was faster on the measured workload.
The pilot covers an owned, cooperative fixture. A maintained backend would
also need live attachment and coexistence with the existing injector and
observers, broader signal and fault cases, and a redistribution inventory.

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

## Packaging and distribution

The native-only fixture added no third-party library. The Gum devkit download
was 12,791,284 bytes and its static archive was 87,510,768 bytes. The SDK-linked
Gum module was 23,109,304 bytes before stripping and 10,319,976 bytes after
stripping. The native executable was 27,248 bytes after stripping. The module
required glibc symbols through `GLIBC_2.17`. The SDK GNU linker 2.30 emitted
564 unsupported GNU property warnings while reading the devkit objects.
The tested runtime still passed the fixture.

Gum's wxWindows Library Licence 3.1 includes an exception permitting binary
distribution of works based on the library under the distributor's own terms.
The devkit also bundles third-party code and contained no separate license
inventory in its file list. Shipping the module requires checking the exact
bundled dependencies and carrying their applicable notices and terms.
[Gum COPYING](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/COPYING)

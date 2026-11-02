# GCC 8.5 runtime compatibility validation — 2026-11-02

The compatibility implementation is in branch `c++17`, based on
`e9856259e2ecb538e098f99c30c50c1a88320afb`. Its managed worktree is
`/Users/husam/.codex/worktrees/gcc85-runtime/clang-tracer`.
The primary checkout, installed baseline, and original application
binaries were preserved.

## Build and application validation

On `linux-dev`, the full native build uses SDK
`/opt/toolchain/x86_64-2.6.32-v4.0.137`: Clang 21.1.8, the GCC 8.5 standard
library and glibc 2.23. LLVM/Clang stay inside the agent and use the SDK's
prebuilt libraries. The tracer remains C++20 internally; this does not require
recompiling the C++17 target. gRPC 1.46.7 and its bundled dependencies,
protobuf 3.19.5, and SQLite 3.34.1 were built against this runtime.

The remote source, build and install directories are respectively
`/tmp/tracer-gcc85-build-20261102/source`, `sdk-build`, and `install`.
The installed executable is `install/bin/clang-tracer-server`, with the agent
and sealed standard-library image under `install/lib64/`. These binaries retain
absolute scratch dependency and SDK runtime paths; this install is not a
relocatable distribution.

| Validation | Result |
| --- | --- |
| Full SDK native CTest suite | 21/21 passed, 192.47 seconds |
| macOS modern-library portable and Clang-analysis suite | 6/6 passed, 37.44 seconds |
| Unchanged GCC 8.5 application, build-tree server/agent | Attach and all three supported signatures passed |
| Same application, installed SDK server/agent | Attach and all three supported signatures passed |
| Original application and installed baseline SHA-256 checks | Unchanged |
| Script checks | Shell syntax, dependency-source preflight and dirty-tree regression checks |

The original application is
`linux-dev:/tmp/tracer-gcc85-cxx17.Zsojgj/old-runtime`, SHA-256
`331d285b8290e3713ba36c9e0b823953d424ae2b9c8e3bc2a2e546d95f35c749`.
It retains its original GCC 8.5 C++17 binary, loader and runtime selection.
`add(int, int)`, `Calculator::multiply(int, int) const`, and
`inspect(const Packet&)` each produced three correctly paired PRE/POST calls
with the expected results, no reported trace drops and restored entry bytes.
The application and server stopped cleanly afterward.

`by_value(Packet)` remains rejected because the target's DWARF lacks the required
pass-by-value ABI evidence. This is an explicit existing signature limitation,
not a successful compatibility test. Fully stripped busy-main attach on legacy
libc also remains rejected when the exact `main` boundary cannot be proven;
the legacy regression verifies that rejection. Idle attach remains available.
This work does not add RIP-relative entry instruction relocation or establish
support for a Linux 2.6.32 kernel. The validation host runs Linux 5.14.

The implementation and rebuild recipe are in
[gcc85-compatibility.md](gcc85-compatibility.md). The script captures dependency
pins, validates downloaded SQLite content, and rejects unrelated modifications
to pinned dependency sources. The dependency builds and final tracer build
were performed in the existing scratch layout; the script consolidates that
recipe rather than claiming a second clean download/build run.

## Benchmark comparison and limits

The baseline is the current installed `/usr/local/bin/clang-tracer-server` and
`/usr/local/lib64/libclang_tracer_agent.so`; the candidate is the SDK build above.
Both trace the same immutable GCC 8.5-built C++17 `libcalculator.so` and the same
workload source. The executable is linked once to the modern host runtime for
the baseline and once to the original SDK runtime for the candidate. The
installed baseline cannot load into the old-runtime target, so this comparison
measures the complete supported profiles, including runtime, compiler/build,
LLVM and dependency effects. It does not isolate compatibility-helper cost.

The benchmark runs nine paired rounds, alternates build order, rotates modes,
and starts a fresh target/server for each trial. The target and its threads are
pinned to CPU 6; the server is pinned to CPU 7. The host is a KVM guest exposing
eight Intel i9-7900X CPUs. An existing unrelated performance campaign remained
running; its coordinator has affinity 0–7, so shared-host interference remains
possible despite pinning these trials. No build or full test suite ran during
the measured campaign.

Each trial warms up 100 calls. Untraced batches contain 5,000,000 calls;
instrumented batches contain 1,500 calls. Measurement encloses only the target
loop; server startup, attach, callback compilation and probe installation are
recorded separately. Times are batch means per call summarized across rounds,
not individual-call latency percentiles. The tiny untraced function makes
application-wide percentage slowdowns unrepresentative: use absolute added
time multiplied by the rate of traced calls.

Modes have distinct work:

- `off`: no tracer attached.
- `trace`: ordinary empty PRE/POST callbacks with two trace records per call.
- `stack`: the same trace records with application-only native stacks, depth 24.
- `shared`: guarded shared-state increments in PRE/POST, without explicit
  context snapshots; shared hooks do not emit ordinary native trace records.
  This measures state access without snapshot serialization or transport.

Every trace/stack trial validates 3,200 warmup-plus-measured records, sequence
continuity, call pairing, arguments, results, drops and entry restoration.
Stack records must be complete and contain application frames. All instrumented
modes check callback errors and restoration; every target checks its workload
checksum and exits cleanly. Shared-state semantics are also covered by the SDK
integration suite.

## Measured overhead

All 72 trials passed. The 36 ordinary trace/stack trials validated 115,200
records, including 57,600 complete stack records. All 54 instrumented trials
reported zero callback errors, restored their entry bytes and stopped cleanly.
No reported drops or input-binary hash changes occurred.

The table shows median target wall time per call across nine batches. The
percentage column compares those two medians. The last column instead takes
the median of the nine paired round-by-round percentage changes; these are
different statistics, especially for noisy short operations.

| Mode | Current build, µs/call | SDK build, µs/call | Ratio of medians | Median paired change |
| --- | ---: | ---: | ---: | ---: |
| Untraced workload | 0.0105 | 0.0121 | +14.9% | +12.7% |
| PRE/POST tracing | 7.020 | 7.319 | +4.3% | +0.25% |
| Tracing with native stacks | 249.258 | 319.702 | +28.3% | +28.1% |
| Shared-state increments, no snapshots | 5.603 | 5.909 | +5.5% | +2.9% |

Stack capture was slower in every pair: +24.1% to +36.2% wall time. Calling
thread CPU time confirms the cost: 241.194 versus 309.280 µs/call, with a
median paired increase of 28.3%. The SDK profile adds about 70.4 µs per
stack-traced call compared with the installed build for this workload.

Ordinary tracing and shared-state results are much noisier. Paired wall-time
changes range from −37.8% to +27.4% for tracing and −36.6% to +53.8% for
shared state. The measured +4.3% and +5.5% ratios do not establish a reliable
small regression under this shared-host run. Calling-thread CPU medians are
4.448 versus 4.963 µs for tracing and 5.526 versus 5.836 µs for shared state;
their paired changes also span both signs. The untraced difference occurs
without any tracer loaded and must not be attributed to tracing overhead.

Subtracting each profile's untraced median gives approximate SDK costs of
7.306 µs per ordinary traced call, 319.690 µs with stacks, and 5.896 µs for
guarded state increments without snapshots. Application impact depends on
how often those probes execute. For example, 1,000 stack-traced calls per
second would add approximately 0.32 seconds of target elapsed work per second
for this fixture; asynchronous drain and other application work can change
the result. This is not a whole-application slowdown prediction.

Setup is outside these loop timings. Median attach was about 122–124 ms for
both profiles. Ordinary probe installation/compilation was 0.825 s current
versus 0.670 s SDK; shared-state installation was 6.442 versus 6.775 s. These
are host-dependent setup observations, not per-call costs.

## Evidence and reproduction

Local raw evidence is retained under
`/Users/husam/.codex/worktrees/gcc85-runtime/clang-tracer/benchmark-results/gcc85-runtime/`;
remote evidence is under `linux-dev:/tmp/tracer-gcc85-build-20261102/`.
The local directory is ignored by Git and includes test/build logs, original
application validation, ELF linkage and symbol-version evidence, benchmark
trial records, events, process maps and immutable-input hashes.

Use [runtime_compat.py](../benchmarks/runtime_compat.py) with the public Python
client to repeat the measured campaign on the retained remote inputs:

```sh
root=/tmp/tracer-gcc85-build-20261102
/home/husam/.local/share/uv/tools/clang-tracer-client/bin/python \
  "$root/source/benchmarks/runtime_compat.py" \
  --baseline-server /usr/local/bin/clang-tracer-server \
  --baseline-agent /usr/local/lib64/libclang_tracer_agent.so \
  --candidate-server "$root/sdk-build/clang-tracer-server" \
  --candidate-agent "$root/sdk-build/libclang_tracer_agent.so" \
  --target "$root/benchmark-input/host-target" \
  --candidate-target "$root/benchmark-input/old-target" \
  --target-library "$root/benchmark-input/libcalculator.so" \
  --output "$root/benchmark-repeat" --rounds 9 --traced-calls 1500 \
  --modes off trace stack shared --target-cpu 6 --server-cpu 7
```

The output path must be fresh. The shared library uses the repository's
`examples/call-stacks/calculator.cpp`, SDK GCC 8.5, C++17, `-g -O0 -fPIC -shared
-fno-omit-frame-pointer`, plus an 8 KiB immutable rodata padding symbol.
That padding gives both old GNU ld load segments distinct file-page mappings
so the unmodified baseline can discover the tiny DSO. The workload executable
uses `examples/call-stacks/benchmark.cpp`, C++17, `-g -O2 -fno-inline
-fno-optimize-sibling-calls -fomit-frame-pointer -fasynchronous-unwind-tables`.
Both executable variants use the same library and force retention of libstdc++;
the old variant additionally selects the SDK loader and runtime search paths.
The padding source and exact binary identities are retained with the evidence.

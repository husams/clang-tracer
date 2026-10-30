# Validation and benchmark notes

The native backend supports Linux x86_64 and was exercised with LLVM/Clang
21.1.8. The source repository includes test fixtures, callback examples and
output checkers. Generated build trees, process recordings and development
progress notes are local artifacts and are excluded from version control.
References elsewhere to `*-results/` describe those local recordings.

## Reproduce the checks

Configure and build as described in the root README. The repository's CMake
configuration registers the integration checks for CTest:

```sh
ctest --test-dir build --output-on-failure
```

The callback examples can also be exercised through the public CLI:

```sh
bash examples/cpp-interceptors/run.sh
bash examples/order-arguments/run.sh
bash examples/call-stacks/run.sh /tmp/clang-tracer-stacks complete
```

Use a fresh output directory for each run. These commands attach to their own
fixtures and record results locally. Stack checks cover complete, partial,
truncated and disabled capture, nested calls, multiple calling threads and
restoration after detach. Additional scripts in `examples/call-stacks/` compare
frame metadata and optional callback values with the emitted events.

## Measured tracing overhead

Measurements on 30 October 2026 used an eight-vCPU Linux x86_64 VM, Clang21.1.8
and a RelWithDebInfo agent. The benchmark binary used `-O2 -g -fno-inline
-fomit-frame-pointer -fno-optimize-sibling-calls -fasynchronous-unwind-tables
-fcf-protection=none`, linked to the existing calculator fixture. The application
call chain was `main -> benchmark_iteration -> library_dispatch ->
StackCalculator::multiply -> stack_add`.

Both traced modes used the empty `automatic.cpp` callback. Stack mode requested
24 frames with application filtering; callback argument/result text was off.
Each mode had nine rotated repetitions. Traced repetitions used 100 warmup plus
1,500 timed calls; unattached repetitions used five million timed calls. Target
and its sender ran on CPU2; the server ran on CPU3. Setup, JIT compilation,
warmup, text output and detach were outside the timed loop. Capture, event
serialization and enqueue were included; transport draining was asynchronous.

An accepted mapping-read batching change produced the following fresh comparison:

| Mode | Before median/call | After median/call | Events lost before / after |
| --- | ---: | ---: | ---: |
| Tracing off | 9.56 ns | 9.82 ns | — |
| Tracing without stacks | 6.968 us | 7.268 us | 0 / 0 |
| Application-only stacks | 92.094 us | 85.582 us | 0 / 0 |

Both versions received all 28,800 events per traced mode, including warmup.
All 144,000 before/after application frame records matched. With the receiver
stopped, the updated implementation completed 1,500 stack-enabled calls in
130.482 ms; all 3,200 events arrived after resume and immediate detach.

A subsequent positioned-read candidate was evaluated with three interleaved
three-round chunks per version, preserving nine repetitions and the original
mode rotation. Its stack-enabled median was 89.403 us/call versus 88.179 us for
the retained implementation. Both versions again received all events. There was
no reliable overall gain, so the candidate was reverted and the original source
and agent restored exactly.

These are repetition averages for a small synthetic workload. Ranges overlapped
and controls varied; they are neither individual-call latency percentiles nor a
performance guarantee. The benchmark driver and its summary tool remain in
`examples/call-stacks/` for further measurements. Build `benchmark.cpp` against
the calculator fixture with the flags above and an appropriate runtime library
path before running the driver.

```sh
BENCH_ROUNDS=9 bash examples/call-stacks/benchmark.sh /tmp/clang-tracer-benchmark
ruby examples/call-stacks/summarize-benchmark.rb /tmp/clang-tracer-benchmark
PRESSURE_STACK_DEPTH=24 bash examples/call-stacks/transport-pressure.sh \
  /tmp/clang-tracer-pressure 1500
```

CPU affinity can be adjusted with `BENCH_TARGET_CPU` and `BENCH_SERVER_CPU` for
machines with fewer CPUs. Stack capture still performs synchronous fresh mapping
reads and uses the LLVM metadata mutex. Event delivery and stream-pool ownership
do not wait on capacity; the finite event queue explicitly counts overflow.

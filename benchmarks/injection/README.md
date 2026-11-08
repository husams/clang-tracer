# Real probe injection benchmark

See [common-header cache results](PCH-RESULTS.md) for the latest comparison,
including separate first-in-process and subsequent injection timings.

Build the fixture once on an isolated Linux x86-64 host with Clang 21:

The fixture builds in C++23 mode. For a custom toolchain, set CXX to
the matching compiler and use TRACER_BENCH_CXX_FLAGS/TRACER_BENCH_LINK_FLAGS for
the same sysroot, loader and runtime paths used by the tracer.

```bash
bash benchmarks/injection/build-fixture.sh /tmp/injection-fixture
```

The generated shared library contains a called function with a stable NOP entry
and 30,000 unrelated function definitions with embedded DWARF. Baseline and
candidate runs must reuse this exact fixture. Use an environment containing this
checkout's Python client, with matching server and agent builds:

```bash
PYTHONPATH="$PWD/python/src" python benchmarks/injection/measure.py \
  --server /path/to/build/clang-tracer-server \
  --agent /path/to/build/libclang_tracer_agent.so \
  --fixture /tmp/injection-fixture --output /tmp/injection-before
```

Each run starts an owned server and target, attaches once, and measures explicit
`resolve_probe` and `add_probe` calls separately. It verifies callback POST
results, removes every probe, checks restored bytes and detaches. All repeats,
including the first, are recorded. Repeats share a target process but use fresh
probe IDs; compare first and subsequent injections separately. The target limits
its own arithmetic arguments and exits if callbacks change its expected result.

Use fresh output directories, preserve errors and compare baseline/candidate in
interleaved order. Complete all builds before timing, check for leftover fixture
processes and record pod/host CPU throttling. Agent phase-profile builds should
be treated as diagnostic runs separately from the primary timing comparison.

To report first and subsequent injections separately across fresh-process runs:

```bash
python benchmarks/injection/summarize.py \
  --baseline /tmp/before-a1/timings.json /tmp/before-a2/timings.json \
  --candidate /tmp/after-b1/timings.json /tmp/after-b2/timings.json \
  --output /tmp/injection-comparison.json
```

The summary validates the driver's restoration/event checks and reports both
pooled medians and each run's median. Repeats in one process are related; a
cache-warm median does not describe first-in-process latency.

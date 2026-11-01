# Native shared-context benchmarks

Measure native interception, shared-state hooks, event delivery and worker startup
through the public CLI. The harness starts private targets and servers; it never
installs runtime files. Linux x86-64, Python 3.9+, matching LLVM/Clang development
libraries, Protobuf `protoc`, and `taskset` are required. Build prerequisites are
in the repository README. Optional stack sampling also needs `eu-stack`.

## Workspace layout

Choose a private experiment directory, represented below by `$BENCH_ROOT`:

```
$BENCH_ROOT/
  fixture/        # Copy the files in this directory here; compile target below.
  before/         # Baseline source tree, including proto/ and build/.
  after/          # Candidate source tree, including proto/ and build/.
```

Build each source tree with matching compilers, SDKs and optimized settings, and
finish its correctness tests before timing. Each build directory must contain
`clang-tracer`, `clang-tracer-server`, `libclang_tracer_agent.so` and its matching
private context standard library. Do not overlap builds, tests, profilers or
other benchmark campaigns with measurement. CPU affinity is not CPU reservation.
The harness expects CPUs 2–5 for targets and CPUs 6–7 for servers.

Compile the same frozen workload for both revisions, using the same flags:

```sh
clang++ -std=c++20 -O2 -g -fno-omit-frame-pointer \
  -fno-optimize-sibling-calls -fcf-protection=none -pthread \
  "$BENCH_ROOT/fixture/target.cpp" -o "$BENCH_ROOT/fixture/target"
```

Socket paths use a short private temporary directory, independent of result-path
length, and are removed after cleanup. Results can live in deeply nested paths.

## Run comparisons

Always pass an explicit workspace and case list; each case is
`source-directory:mode:workers`. Source directories must be simple names within
the workspace, such as `before` or `after`. The special revision `installed` directly uses
`/usr/local/bin`, `/usr/local/lib64` and the installed protocol schema; override
that prefix/libdir with `--installed-prefix` and `--installed-libdir`.

```sh
python3 "$BENCH_ROOT/fixture/run.py" --root "$BENCH_ROOT" \
  --output "$BENCH_ROOT/results/steady" --rounds 5 --count 1536 \
  --bursts 20 --burst-interval-us 20000 \
  --cases before:ordinary:1,after:ordinary:1,before:ordinary:4,after:ordinary:4

python3 "$BENCH_ROOT/fixture/run.py" --root "$BENCH_ROOT" \
  --output "$BENCH_ROOT/results/requests" --rounds 5 --count 4 \
  --baseline-count 4 --bursts 100 --burst-interval-us 20000 \
  --cases before:off:4,after:off:4,before:ordinary:4,after:ordinary:4,before:shared:4,after:shared:4
```

`--bursts` retains each active batch/request in `batches.json`, with the interval
outside active-call timing. Every fresh process has a separate target and probe
compilation followed by a 32-leaf-call warmup batch. Orders rotate and reverse
between rounds. Treat processes, rather than correlated batches in one process,
as independent replicates; keep first timed-request latency distinct from a
repeated-request average. A handful of tiny
single-batch samples cannot establish a small version effect by itself.

Available modes include `off`, `ordinary`, `scoped`, `shared`, `small`, `nested`,
`durable`, `stack`, `shared-stack`, `blocked` and `fresh`. The root spans the leaf
batch and propagates shared state into newly created workers. `off` is the same
untraced thread workload. `fresh` traces only the leaf. Ordinary hooks emit two
PRE/POST events per call; exporting state hooks emit explicit snapshots. Capture
budgets and overload rejection make these different amounts of work.

For paced leaf samples with one worker, add
`--latency --pace-us 20000 --count 128`. Sampled leaf latency excludes the sleep,
while process CPU over the paced interval includes
helper work. Each sampled batch appends a separate `SAMPLES,...` row to
`latencies.csv`. Four-worker requests create and join four threads; do not compare
that whole-request cost to amortized hot leaf overhead as the same metric.

Use `--identity-manifest FILE` with a `sha256sum` manifest containing the frozen
runtime, target, schema, callback and harness inputs. Relative paths resolve
under `--root`. All entries are checked before and after each trial, outside
active timing; an input replacement stops the run. Save the manifest alongside
the results and do not change the installed runtime during an installed run.

Checksums, callback errors, original-byte restoration and post-detach target work
are checked. Installed cases also verify the mapped agent path. Retain all raw
commands, frames and validation failures; inspect expected delivery and agent
losses before accepting comparisons. Existing trial directories are rejected
before any target starts; choose a fresh output directory for each run so changed
settings cannot silently reuse old measurements.

## Analyze without rewriting historical evidence

```sh
python3 "$BENCH_ROOT/fixture/analyze.py" "$BENCH_ROOT/results" \
  --output "$BENCH_ROOT/analysis"
```

The input layout is `campaign/trial/stats.json` with sibling logs and frames.
The analyzer writes `analysis.json` and `samples.csv` to `ROOT/analysis` by default,
or to the explicit output directory. It refuses the input directory itself and
existing analysis files. Missing trial/detach evidence, incomplete ordinary-hook
delivery, corrupt frames, inconsistent batch totals and validation failures
produce a nonzero exit status. Snapshot capture remains best effort and reports
its delivered fraction. The analyzer reports independent process count, total
timed batches, recorded individual samples and first-batch
statistics. Legacy aggregated bursts retain their known batch count, but missing
individual/first-batch samples are not fabricated from the aggregate. Affected
single-batch cohorts get explicit sampling warnings.

Wall and process CPU times divide active batch time by leaf calls and include
amortized root/thread work. Process CPU includes agent helpers scheduled during
that window; it is not exclusive hook CPU. Compare useful delivered snapshots
as well as attempted calls. `max_server_history_evictions` names the legacy wire
counter correctly: history eviction can increase while every live event arrives.
The old `max_server_drop` field remains available for existing analysis readers.

For small residual differences, add identical-binary controls, enough fresh
process blocks and balanced randomized ordering. Fix the estimand, practical
margin and stopping rules before inference. Bootstrap independent blocks, keep
within-process samples nested, and preserve the original estimator as a check.
Practical equivalence at a margin does not prove exact equality.

## Checks

Run the portable harness regression checks from this directory:

```sh
python3 -m unittest discover -s . -p 'test_*.py' -v
```

The tests cover real Unix-socket binding with a deep results path, private-directory
cleanup, repeated versus independent sample counts, legacy burst metadata,
first-request availability, history-eviction labeling and separate analysis output.
A Linux integration run additionally needs the native binaries and development
libraries described above. Raw benchmark results belong outside version control.

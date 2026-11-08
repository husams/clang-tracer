# Attach and stream implementation progress

Base: `0dca368` (main), 2026-11-03.

- [x] Measure attach/inject with many symbols and modules; identify the dominant phase.
- [x] Implement and compare a focused optimization, preserving admission and mapping checks.
- [x] Make console `collect` and `events` unbounded by default and cancellable with Ctrl+C.
- [x] Run relevant native/Python checks and independently review the final changes.

Native validation requires Linux x86-64; avoid interfering with the registered linux-dev study.

## Stream validation

`collect` now accepts an optional count and displays events as they arrive through
the cancellable stream. Neither bare command supplies a deadline; explicit counts
and `--timeout` remain available. A real PTY/gRPC regression verifies Ctrl+C
cancels an idle collection and returns to the prompt. POSIX-only test imports
are guarded for Windows client test collection.

Full final Python suite: 180 passed, 14 skipped. Real PTY checks cover both
`collect` and `events`, including an idle stream and cancellation at the prompt.
Ruff check/format and diff checks pass.

## Native measurement environment

The registered linux-dev study is live, so no build or measurement runs there.
A disposable Ubuntu 26.04 x86-64 pod on separate worker01 provides LLVM 21.1.8.
It shares cluster-node resources; before/after comparisons must acknowledge noise.
The baseline source is an archive of `0dca368`; the harness under
`benchmarks/attach/` isolates discovery/validation and measures real sleeping-child
ptrace attachment plus remote `dlopen` in a separate bootstrap executable.

## Measurement checkpoints

The first harness forked its rebuilt executable as the target. Its 612 ms
16-library bootstrap median is diagnostic only. The revised harness execs a
separate frozen target, reuses identical libraries, matches `RTLD_GLOBAL`, records
all binary hashes, and preserves every raw sample.

The frozen-target baseline at 16 added libraries (160,000 generated functions)
has a 559.631 ms bootstrap median: discovery 257.709 ms, attachment 255.872 ms,
remote bootstrap 49.393 ms. Separate phase instrumentation attributes 2,587 ms
to 240 complete module-CFI validations and 212 ms to 260 image reads across five
discovery/validation repeats, making CFI the dominant cost.

Sparse ELF reads and filtering unused symbol copies alone reduce the measured
bootstrap median to 516.794 ms (7.7%); this is an intermediate candidate. Exact
CFI-input memoization is included in the final comparison.

Raw checkpoints and profiles: `benchmark-results/2026-11-03-attach/` (ignored
measurement artifacts). Initial originals remain under
`/tmp/clang-tracer-attach-20261103/`.

## Final result

Clean frozen-target ABBA comparison: total median **591.568 → 296.607 ms**
(49.9% lower, about 2× faster); attachment/revalidation **277.248 → 8.331 ms**
(97.0% lower). The pod had no CPU throttling during those eight runs. An earlier
cache run contaminated by orphaned timeout-test children is preserved and
excluded from this comparison.

The independent CFI-cache CTest and ASan+UBSan run pass using the final actual
source. An existing test-fixture temporary-path buffer overflow found by UBSan
was fixed. Existing native startup/coalesced-mapping failures reproduce on the
exact baseline and remain documented, without changing admission policy.

See [results and validation limits](benchmarks/attach/RESULTS.md) for phase
timings, provenance, raw artifact links, scope, and reproduction commands.

## Injection follow-up

- [x] Measure actual DWARF resolution, AddProbe, callback compilation and activation.
- [x] Optimize the measured injection bottleneck without sharing mutable probe state.
- [x] Verify callback events and restored bytes; compare frozen-input timings.

Baseline snapshot `/tmp/clang-tracer-inject-20261103/base` includes the completed
attach optimization and preserves the current injection implementation.

Real console/API injection median **1,548.217 → 1,193.690 ms** (22.9% lower)
across clean ABBA runs. Resolution improves **422.018 → 28.978 ms** after
batching expensive gRPC cancellation checks. The agent reuses its verified DWARF
result within each operation. Compiler and receiver preflights remain present;
AddProbe-only improvement is not established by the variable samples.

Four focused Linux native Python tests pass, including metadata tampering,
cancellation/budgets, free functions, typed methods and restoration. Two selected
CTests pass; the void-entry and inheritance-message failures reproduce on the
frozen baseline. Local Python: 180 passed, 15 native skips. See
[injection results and limits](benchmarks/injection/RESULTS.md).

## Common compiler prefix follow-up

- [x] Precompile the invariant callback headers with the matching Clang environment.
- [x] Restore that prefix into fresh compiler/JIT instances without sharing user state.
- [x] Verify rejected input, macro isolation, typed receivers and concurrent probes.
- [x] Compare first and subsequent injections against the frozen preceding candidate.

The baseline snapshot and new evidence live under
`/tmp/clang-tracer-pch-20261103/`; native validation stays on separate worker01.
The existing three validation/compilation stages remain in place.

Clean eight-run ABBA comparison: subsequent injection median
**1,143.745 → 384.091 ms** (66.4% lower latency, about 3× faster).
First-in-process median is similar, **1,130.081 → 1,145.684 ms**, while preparing
both syntax and incremental PCH variants. All 24 injections verified callback
arguments/results and restored bytes; no timed run incurred CPU throttling.

Eight focused native Python cases pass, including both shared-context history
cases. The two cache CTests, unchanged agent audit and raw client CTest pass;
the class-interceptor failure reproduces on the frozen baseline. LLVM 22 macOS
helper checks and the final local Python suite (180 passed, 16 native skips)
also pass. Cache tests include changed/missing/shadowing headers, seven unsafe
prefix cases, independent JIT state and unavailable cache storage.

See [PCH results and scope](benchmarks/injection/PCH-RESULTS.md).
Raw evidence is copied to the ignored benchmark-results/2026-11-03-pch/results
directory. Archive checksums were verified and the disposable namespace removed.

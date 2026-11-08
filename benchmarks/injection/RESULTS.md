# Real injection latency — 2026-11-03

> **C++17 port:** These are the original C++20 measurements, not C++17 results.
> See [C++17 integration and validation](../../docs/cpp17-performance-merge.md).

The console/Python API injection path improved from **1.548 s to 1.194 s** median
on the recorded 30,000-symbol fixture: **22.9% lower latency**. Attachment is
measured separately and excluded from these injection timings.

## Changes

- DWARF resolution polls cancellation at module/unit boundaries, every 128
  entries, before limit errors and before returning. Entry/depth budgets still
  apply to every entry. A cancellation request remains observable at completion.
- The agent verifies the complete resolved metadata once per injection and
  passes that verified spec/address through receiver setup. This result is
  operation-local; each new request performs its own verification.

The existing callback syntax, receiver-layout and JIT validation stages remain
in the injection path.

## Measurements

Ubuntu 26.04 x86-64, LLVM/Clang 21.1.8, isolated test pod on worker01, 4 CPU/8 GiB.
The active linux-dev study used a separate host throughout. The target and shared
library were built once and reused unchanged. The library contains one called
function with a stable NOP prologue and 30,000 unrelated DWARF-backed functions.

Baseline includes the preceding attach optimization. Candidate changes are the
resolver cancellation polling and agent validation reuse. Four fresh-process
runs in `A,B,B,A` order each perform three injections/removals. All twelve
injections produced two verified POST callbacks and restored the original bytes.
CPU-throttling counters remained unchanged within each timed run.

| Median, milliseconds | Baseline (6 injections) | Candidate (6 injections) |
| --- | ---: | ---: |
| ResolveProbe | 422.018 | 28.978 |
| AddProbe | 1,137.235 | 1,166.455 |
| ResolveProbe + AddProbe | **1,548.217** | **1,193.690** |

The observed improvement comes from resolution. AddProbe varied between runs;
these samples do not establish an AddProbe-stage speedup. Each run shares its
target across three injections, so samples within a run are related. This is a
fixture-specific latency comparison, not a confidence interval or a universal
speedup estimate.

Separate diagnostic builds explain the bottleneck:

- Server resolution: 90,016 gRPC cancellation polls consumed 392.3 ms of a
  425.2 ms lookup. Batched polling reduced the measured lookup median by 93.1%.
- Agent: the two baseline DWARF checks consumed 30.3 and 26.1 ms. The duplicate
  check is removed while the initial full comparison remains.
- Ordinary callback compilation: user AST preflight 308.7 ms, complete-source
  AST preflight 325.9 ms, ParseAndExecute 324.6 ms. These stages account for most
  remaining injection time.

## Verification

- Local Python suite: **180 passed, 15 skipped** (native tests require Linux).
- Candidate native Python checks: **4/4 passed**. They cover free-function and
  method injection, shutdown restoration, rejected tampered debug/signature/name
  metadata, successful injection after rejection, RPC cancellation, explicit
  entry/depth limits, large symbol pages and deep selectors.
- Candidate CTests `grpc_client_integration` and
  `native_module_cfi_cache_integration` passed. The agent passed the unmodified
  export/dependency audit: exactly three exports and no shared transport deps.
- `grpc_native_integration` rejects the fixture's unsupported/PC-relative void
  entry; `grpc_typed_this_integration` differs from an expected inheritance-error
  message. Both failures reproduce with the frozen baseline on the same
  fixtures. The known module-coalescing fixture failure also remains.
- Independent review found no correctness blocker in cancellation priority,
  token lifetime or the typed/raw validation paths. Ruff, changed-line C++
  formatting and diff checks pass.

## Reproduction and artifacts

See [README.md](README.md) and [the driver](measure.py). The driver times public
`resolve_probe` and `add_probe` calls, validates callback-recorded arguments and
native results, checks restored bytes, and detaches its owned session.

Raw timing samples, computed medians, exact diagnostic sources, build/audit logs
and native checks are retained under
`benchmark-results/2026-11-03-injection/results/`; primary summary:
[summary.json](../../benchmark-results/2026-11-03-injection/results/summary.json).
The original evidence root is `/tmp/clang-tracer-inject-20261103/`.
Baseline/candidate binaries, frozen inputs, source snapshots and dependency build
configuration are preserved with the logs. The disposable pod and dedicated
namespace were removed after archiving; no other workloads were changed.

Candidate source SHA256:

- `src/agent.cpp`: `feedf7e1c3bd9c074f346f8a34e15e6b8186634cae9c9965360180d687230eaa`
- `src/debug_resolver.cpp`: `d6f3cde9f00a469710b86c4c9f355b8921af82fd371905e8cbe0639fb6cfda82`

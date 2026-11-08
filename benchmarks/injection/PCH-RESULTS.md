# Common callback header cache — 2026-11-03

> **C++17 port:** These are the original C++20 measurements, not C++17 results.
> See [C++17 integration and validation](../../docs/cpp17-performance-merge.md).

The common standard headers are now precompiled and restored into fresh
compiler/JIT instances for subsequent injectors in the same target process.
The first injector builds the cache lazily. The user callback and its mutable
state are compiled independently each time.

Subsequent injection median fell **1,143.745 → 384.091 ms**: **66.4% lower
latency**, about **3× faster**. First-in-process latency was similar
(1,130.081 → 1,145.684 ms), because that operation prepares both PCH variants.

## Measurements

| Median, milliseconds | Baseline | PCH cache |
| --- | ---: | ---: |
| First injection total (4 processes each) | 1,130.081 | 1,145.684 |
| Subsequent ResolveProbe (8 samples each) | 30.495 | 31.490 |
| Subsequent AddProbe (8 samples each) | 1,113.250 | 355.303 |
| Subsequent injection total | **1,143.745** | **384.091** |
| All injections total (12 samples each) | 1,134.843 | 396.778 |

All 24 injections produced two verified POST callbacks and restored their
original bytes. No cgroup CPU-throttling counter increased during any timed
run. Each label used four fresh target processes with three injections each.
The all-injection median reflects that particular first/subsequent ratio.

The measured improvement is reuse within an agent process. A fresh process
pays setup again; this comparison establishes no first-injection speedup.
These are fixture-specific medians with related within-process repeats, not
confidence intervals or universal speedup estimates. The ordinary callback path
is timed here; the shared-context owner's separate compilation pipeline remains
unchanged.

## Implementation

The cache covers the standard headers cstdint, cstddef, string_view, stdexcept
and type_traits. Syntax validation and Clang's incremental interpreter use
separate compatible PCH artifacts. Cache identity includes the complete
normalized compiler invocation, LLVM version, working directory and prefix
content. Files live in a process-owned private directory.

The three existing validation/compilation passes remain. Protocol/context/Call
definitions, argument aliases, typed receiver declarations and callback source
stay in their original per-injector source order. Each probe receives fresh
compiler, AST and JIT state.

Dependency checks cover opened inputs and filesystem lookup probes, including
previously missing files and earlier include search results. Unsupported forced
includes, remaps, overlays, plugins and explicit module inputs bypass caching.
Cache construction or load failures fall back before user execution.

PCH generation completes pending header template instantiations before checking
for runtime global initialization, nontrivial cleanup, constructor/destructor
attributes and file-scope assembly. Such headers use the original path so their
effects cannot move ahead of callback validation.

## Verification

- Linux x86-64, Ubuntu 26.04, LLVM/Clang 21.1.8, separate worker01 pod.
- The callback_pch_reuse and callback_pch_unavailable CTests and the unchanged
  agent symbol audit pass. The cache test covers real JIT execution, independent
  static state, edited headers, newly available includes, new search-path
  shadows, forced include bypass, seven unsafe prefix cases, and unavailable
  storage.
- **Eight native Python cases pass**, covering rejected cold/warm callbacks, user macro
  and declaration isolation, simultaneously active distinct callbacks, typed
  argument aliases, typed self()/field inference, restoration/detach, metadata
  tampering, large/deep symbol discovery, cancellation/budgets and retention.
  Both shared-context history cases also pass.
- The grpc_client_integration CTest passes. The class-interceptor integration
  rejects Fixture::overloaded(int) with the same no-matching-callable diagnostic
  on the frozen baseline; that existing native coverage gap remains.
- The actual helper also builds/runs with macOS arm64 LLVM 22.1.8. This verifies
  the portable cache implementation, not the Linux-only native tracer on macOS.
- Local Python: **180 passed, 16 native skips**. Ruff and diff checks pass.

## Measurement protocol

The baseline is the immediately preceding attach/injection candidate, including
batched cancellation polling and operation-local DWARF reuse. PCH reuse is the
only new production change in this comparison.

The same frozen 30,000-function debug library and target from the preceding
[injection comparison](RESULTS.md) are used. Attach time is excluded. Eight
fresh-process runs follow **A,B,B,A,A,B,B,A**, with three verified
injection/removal operations per process. First and subsequent injections are
reported separately; repeats within one process are related.

All builds/tests finish before timing. Each injection verifies callback-recorded
arguments and native POST results, restored bytes and target survival. The
active linux-dev study is kept on its separate host.

The initial smoke run accidentally selected the older attach-only binary with
~430 ms resolution; it is retained as excluded evidence. It is not part of the
PCH comparison.

## Retained evidence

Raw samples, computed medians, the exact summary tool, tests and baseline failure
controls, source/binary/fixture hashes, frozen source snapshots, runnable
baseline/candidate binaries, and static dependency build/audit configuration are
preserved under
[benchmark-results/2026-11-03-pch/results](../../benchmark-results/2026-11-03-pch/results/).
The primary numerical artifact is
[abba-summary.json](../../benchmark-results/2026-11-03-pch/results/abba-summary.json).
Original archive: /tmp/clang-tracer-pch-20261103/results.
Local LLVM 22/helper and Python logs are retained in the adjacent
[local directory](../../benchmark-results/2026-11-03-pch/local/).

The archive checksums were verified before the disposable validation namespace
was deleted; its absence was confirmed. No linux-dev runtime was changed.

## Sources and reproduction

- [Driver and reproduction instructions](README.md), [summary tool](summarize.py).
- [Cache implementation](../../src/callback_pch.cpp),
  [agent integration](../../src/agent.cpp),
  [cache tests](../../tests/callback_pch_tests.cpp),
  [native API tests](../../python/tests/test_native.py).
- The wiki's [PCH validity guidance](/Users/husam/workspace/wiki/pages/research/cpp-repl-editor-startup-profile-latency.md)
  informed dependency and isolation checks; it did not contain an injector cache.
- LLVM 21.1.8 primary implementation:
  [Interpreter.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/clang/lib/Interpreter/Interpreter.cpp),
  [Sema.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/clang/lib/Sema/Sema.cpp),
  [ASTReader.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.8/clang/lib/Serialization/ASTReader.cpp).

# Attach metadata optimization — 2026-11-03

> **C++17 port:** These are the original C++20 measurements, not C++17 results.
> See [C++17 integration and validation](../../docs/cpp17-performance-merge.md).

Source baseline: `0dca368` (main). Candidate branch:
`perf/attach-and-unbounded-streams`.

The candidate reads required ELF sections rather than the complete file and
retains only the five libc startup symbols used by admission. Discovered module
handles retain successful CFI validation inputs. Every later control attachment
still reopens the image and checks canonical paths, live mappings and load bias;
the full CFI validator runs again whenever any of its inputs differ.

## Measured result

Linux x86-64, Ubuntu 26.04, Clang/LLVM 21.1.8, disposable pod on worker01,
4-CPU/8-GiB limits. The existing linux-dev study used a separate host throughout.
The fixture loads 16 libraries with 10,000 generated functions each. The target
and injected ELF files are frozen across both versions. The primary measurement
uses real ptrace attachment and remote `dlopen(RTLD_NOW | RTLD_GLOBAL)`.

Eight runs in `A,B,B,A,A,B,B,A` order gave these medians, in milliseconds:

| Phase | Baseline | Candidate |
| --- | ---: | ---: |
| Module discovery | 256.065 | 238.933 |
| Control attachment, including fresh module revalidation | 277.248 | 8.331 |
| Remote bootstrap and detach | 49.508 | 49.337 |
| Total | **591.568** | **296.607** |

Total latency falls **49.9%**, about **2.0× faster**. The attachment phase falls
97.0%. Phase medians are calculated independently and need not add to the total
median. The pod's CPU-throttling counters did not change during these runs.

The baseline profile attributed 2,587 ms to CFI validation versus 212 ms to ELF
loading across five discovery/revalidation cycles. A sparse-read-only candidate
improved the first comparison by 7.7%; memoizing unchanged CFI addresses the
dominant repeated work. A separate single diagnostic run observed 102 MiB
baseline peak RSS and 77 MiB candidate peak RSS. The cache retains unwind bytes
and load headers for the lifetime of each discovered handle/session.

This measures native attachment/bootstrap with synthetic libraries. RPC latency,
DWARF selector resolution and callback compilation require separate measurement.
The shared cluster node can introduce timing noise; this result is specific to
the recorded fixture and environment.

## Verification and limits

- Final Python suite: **180 passed, 14 skipped**; real PTY/gRPC checks verify bare
  `collect` and `events` have no deadline and Ctrl+C cancels the idle stream and
  returns to the prompt. Explicit count/timeout options remain available.
- Native cache regression: **1/1 passed**, also passed ASan+UBSan. It verifies
  valid reuse, timestamp-only changes, and rejection of changed malformed CFI.
- Sanitizers exposed an existing one-byte temporary-path buffer overflow in the
  test fixture; its buffer now includes space for the terminating NUL.
- The normal, frame-pointer and stripped admission tests fail with the existing
  startup-chain rejection on this environment. The module-identity suite fails
  on its coalesced-mapping fixture. Unmodified normal-admission and
  module-identity controls reproduce both signatures; the new cache regression
  runs independently of those fixtures.
- C++ changed-line formatting, Ruff check/format and `git diff --check` pass.
- Independent review found no correctness blockers in the cache or stream code.

The first cache timing run was contaminated by orphaned busy-loop children from
timed-out admission tests. Those six temporary children were stopped and that
run is excluded from the result above. Its raw samples remain preserved as
`candidate-v3`; the clean comparison is `abba-v1`. The original fork-self harness
was also replaced by the frozen-target harness before the primary comparison.

## Reproduction and artifacts

Run [the benchmark script](run-metadata.sh) as described in [README.md](README.md).
Raw samples, binary identities, process/cgroup audits and the computed summary:
[benchmark artifacts](../../benchmark-results/2026-11-03-attach/abba-v1/summary.json).
Validation logs are under `benchmark-results/2026-11-03-attach/`.
The disposable test pod and its dedicated namespace were removed after evidence
was archived locally.

Candidate admission source SHA256:
`0ee08d33a7961d4878fd6294e250d6fa37742d4b7ad112eab9ebe4d759abec98`.
Frozen target SHA256:
`5b4e711908911db73b79e56845d10a4abc7c12e399001e5632de38bde35fc92b`.

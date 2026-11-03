# Attach metadata scaling benchmark

On an otherwise idle Linux x86-64 host with LLVM 21 development files:

```bash
bash benchmarks/attach/run-metadata.sh "$PWD" /tmp/attach-baseline
```

Set `CXX` and `LLVM_CONFIG` for another matching supported LLVM installation.
The harness loads 0, 1, 8 and 16 copies of a generated shared library with
10,000 function symbols and unwind records each. It records five discovery and
mapping-validation samples, including the first sample. CSV module counts also
include any eligible dependencies of the harness. No timing threshold is used.

Run the same script against baseline and modified checkouts on the same host,
with different output directories and a shared frozen fixture directory:

```bash
ATTACH_FIXTURES=/tmp/attach-fixtures bash benchmarks/attach/run-metadata.sh \
  /path/to/baseline /tmp/attach-before
ATTACH_FIXTURES=/tmp/attach-fixtures bash benchmarks/attach/run-metadata.sh \
  /path/to/modified /tmp/attach-after
```

Existing measurement directories are never overwritten. The shared fixture
includes a separate target executable and identical generated libraries; hashes
for every executable and library are recorded with each run.
Inspect both raw samples and medians. This isolates metadata overhead during
attach and subsequent control operations. A second harness uses real ptrace to
attach to a sleeping child and inject a library with remote `dlopen`, recording
discovery, attachment and bootstrap times separately. It terminates its own child
after each sample. RPC, DWARF selector resolution and callback compilation
require separate measurement.

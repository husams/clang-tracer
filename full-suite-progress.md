# Full Linux SDK suite

Base: 801757b4, C++17; linux-dev build: /tmp/clang-tracer-remove-limits/build.
All configurable limits, accepted ranges and defaults remain unchanged.

Fixes:
- Plugin preprocessing now tolerates SDK driver options unused by that stage, matching the existing code-generation behavior; this removes stray warning summaries during agent RPC operations.
- Live JIT imports resolve GCC 8's __pthread_key_create predicate from the verified loaded libc/libpthread provider. A direct JIT regression verifies the resolved address.
- The live-mock fixture supplies legacy Linux ptrace constants absent from SDK headers.
- The live-data fixture preserves manual-owner coverage on older libraries and verifies unknown automatic ownership when atomic shared ownership is unavailable; supported libraries retain the atomic-owner cases.
- Session-history coverage now verifies all 68 sessions survive attach/detach instead of expecting the removed 64-session eviction policy.
- Restored build/install_manifest.txt ownership after privileged installation; deployment now restores that ownership before user-run installation tests.

Validation:
- Complete 222-test run: 213 passed, eight failed, one permission-related skip (full-tests.log; 2510.75 seconds).
- After fixes, all 15 affected tests/regressions passed, covering every initial failure (full-fixes-tests.log).
- The syscall perf test passed separately with the privileges needed to read tracefs (syscalls-privileged.log; process exit 0).
- Aggregated evidence verifies 222 successful checks: 221 ordinary passes and one privileged pass (full-suite-results.json).
- Full rebuild passed. No configurable limit changes were needed.

Evidence files are under linux-dev:/tmp/clang-tracer-remove-limits/.

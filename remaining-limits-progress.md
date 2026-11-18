# Remove remaining fixed ceilings (C++17)

Base: origin/c++17 at abb467ca; existing user checkout preserved.

Completed implementation:
- Remove session/store and snapshot-method count policies.
- Remove fixed input ceilings; retain representation overflow checks.
- Preserve everything already configurable, including accepted ranges and defaults for timeouts, buffers, memory inspection, snapshots and context budgets.
- Replace fixed argument/named-value storage and update bootstrap ABI to 33.
- Grow shared-context registration, invocation, capture and bookkeeping storage.
- Remove fixed logging/transport capacities while preserving explicit budgets.
- Keep named-value allocation on the native side of the private-runtime callback ABI.

Validation:
- Full Linux GCC 8.5 SDK / Clang 21 build passed.
- 27 targeted Linux tests passed across the final suite and corrected-fixture rerun: 26/27 in tests-final.log, then native_repl_fixed_limits passed in fixed-final.log after adding its required scalar-override target annotation.
- Coverage includes 40 arguments/overrides, 40 named values, source over 4 MiB, complete long log messages, 40 simultaneous probes, 5,000 pending event packets, growing context storage, reload, snapshots, memory scans and instruction recording/replay.
- macOS context engine: 9/9 passed under AddressSanitizer and UndefinedBehaviorSanitizer.
- Known unrelated SDK plugin test failure reproduced on baseline 7d389b67: compiled fallback instead of the expected REPL-object loading path.

Deployment uses a versioned release with installed dependency and fixed-limit regression checks before changing the current symlink; its installation.json records the deployed commit and binary hashes.

No wiki access; work and evidence tracked here.

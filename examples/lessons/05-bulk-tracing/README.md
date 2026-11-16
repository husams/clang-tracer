# Lesson 5: preview and apply bulk tracing

Guide section: [Preview and apply bulk tracing](../../../docs/guide/tutorials.md#preview-and-apply-bulk-tracing).

Instead of one `%probe inject` per function, you describe a selection, preview
what it would install, save it as a manifest, and apply it as one batch.

| File | Purpose |
| --- | --- |
| `target.cpp` | Calls `add` and `multiply` on each Enter. |
| `hook.cpp` | One observer for every function in the batch. |

```bash
make
./target
```

```bash
%attach 12345
%trace preview --exact "add(int, int)" --exact "multiply(int, int)" --save trace.textproto
%trace apply --manifest trace.textproto --callback hook.cpp
%collect --count 4 --timeout 30 --as bulk
%events stats bulk
```

Press Enter once in the target terminal while `%collect` waits. Expected: two
PRE/POST pairs, one for each function, and `%events stats` aggregates the
counts and durations per probe.

`%trace apply` returns a batch ID. Use it to remove everything the batch installed:

```bash
%trace remove BATCH_ID
%detach
```

Selection rules: repeat `--module GLOB`, `--include GLOB`, `--exclude GLOB` and
`--exact SELECTOR`; at least one include or exact rule is required. `preview`
installs nothing and reports eligibility and capacity, so it is safe to run on
a large target first. A saved manifest can be previewed against another
session with `%trace preview --manifest trace.textproto`. `%trace apply`
restores newly installed probes if the batch only partly succeeds.

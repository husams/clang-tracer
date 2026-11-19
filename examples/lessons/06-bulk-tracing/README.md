# Lesson 6: preview and apply bulk tracing

Guide section: [Preview and apply bulk tracing](../../../docs/guide/tutorials.md#preview-and-apply-bulk-tracing).

| File | Purpose |
| --- | --- |
| `target.cpp`, `hook.cpp` | The first lesson's target and observer. |
| `stack-hook.cpp` | `hook.cpp` plus `capture_stack(8, false)` in PRE, for the caller edge. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%trace preview --exact "add(int, int)" --save trace.textproto
%trace apply --manifest trace.textproto --callback hook.cpp
%collect --count 2 --timeout 30 --as bulk
%events stats bulk
%trace remove BATCH_ID
```

Press Enter in the target terminal while `%collect` waits. `BATCH_ID` is the
`batch=...` value printed by `%trace apply`.

To see the caller edge, apply the manifest again with `stack-hook.cpp` and
collect into a new name:

```bash
%trace apply --manifest trace.textproto --callback stack-hook.cpp
%collect --count 2 --timeout 30 --as bulk_stack
%events stats bulk_stack
%trace remove BATCH_ID
%trace preview --manifest trace.textproto
%detach
```

Expected: the summary shows `PRE=1 POST=1 paired=1`, and with `stack-hook.cpp`
an `edge` line with `caller="main"`.

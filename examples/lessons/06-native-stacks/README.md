# Lesson 6: capture and inspect native stacks

Guide section: [Capture and inspect complete native stacks](../../../docs/guide/tutorials.md#capture-and-inspect-complete-native-stacks).

`leaf` is called by `middle`, which is called by `outer`. You will probe `leaf`
and ask the tracer to record who called it.

```bash
make
./target
```

```bash
%attach 12345
%probe inject "leaf(int)" --callback hook.cpp --stack-depth full --stack-application-only false --print-result
%collect --count 2 --timeout 30 --as fullstack
%events stack fullstack --index 0
%events stats fullstack
```

Press Enter once in the target terminal while `%collect` waits. Expected: the
stack lists `leaf`, `middle`, `outer`, `main`, then runtime frames, each with
module and source location where they can be recovered. Both phases describe
the stack at the function entry.

- `--stack-depth N` captures up to N frames; `full` follows the supported unwind chain, and `0` turns capture off.
- `--stack-application-only true` (the default) hides runtime frames; `false` shows them.
- `%events stats` shows the caller edge `middle -> leaf` per probe.

If a deep stack exceeds the shared transport capacity, restart with a larger
`--context-event-bytes` (and if needed `--context-transport-memory-bytes`).
The [call-stacks example](../../call-stacks/README.md) covers application-only
filtering and symbolization inside the callback.

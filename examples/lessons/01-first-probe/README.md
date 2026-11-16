# Lesson 1: your first probe

Guide section: [Create a target and a basic probe](../../../docs/guide/user.md#create-a-target-and-a-basic-probe).

You will attach to a running process, install a probe on `add(int, int)`, read
its arguments and result, and detach. This is the loop every later lesson
builds on.

| File | Purpose |
| --- | --- |
| `target.cpp` | Calls `add(20, 22)` each time you press Enter. It has no dependency on the tracer. |
| `hook.cpp` | The observer: reads the arguments in PRE and the result in POST. |

Build and start the target in terminal one. Keep it running.

```bash
make
./target
```

In terminal two, run `clang-tracer-repl` in this directory. Use the PID the target printed.

```bash
%attach 12345
%symbols --filter "add("
%probe resolve "add(int, int)"
%probe validate "add(int, int)" --callback hook.cpp
%probe inject "add(int, int)" --callback hook.cpp --print-arguments --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as first
```

Press Enter once in terminal one while `%collect` waits. The target prints
`add(20, 22) = 42`. Then inspect what was captured:

```bash
%events list first
%events show first --index 1
%events calls first
%detach
%quit
```

Expected: two events, a PRE with `left=20` and `right=22`, and a POST with
`sum=42`. `%events calls` pairs them into one call with a duration.

## What each step did

- `%symbols` lists callables the DWARF describes; it proves the tracer can see `add`.
- `%probe resolve` shows the signature, ABI and entry the probe will patch.
- `%probe validate` compiles `hook.cpp` against that signature without installing it.
- `%probe inject` patches the function entry. `--print-arguments --print-result`
  also records the arguments and result automatically, next to the fields your hook sets.
- `%collect` waits for events and keeps them in the collection named `first`.

If `%symbols` is empty, check that the target was built with `-g` and that you
attached to the right PID.

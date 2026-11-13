# Lesson 14: launch a target from the REPL

Guide section: [Launch a local application](../../../docs/native-repl.md#launch-a-local-application).

In the other lessons you start `./target` yourself and attach to it. Here the
REPL starts the program with `%launch`, holds it at a checkpoint while you
install probes, and continues it with `%resume`.

| File | Purpose |
| --- | --- |
| `target.cpp` | Calls `tracer::launch::wait_for_resume()` first, then `add(20, 22)` once a second. |
| `hook.cpp` | The first lesson's observer. |
| `Makefile` | Builds `target` against the installed `<tracer/launch.hpp>`. |

Two things make a program launchable, and the other lessons' targets have
neither, so `%launch` reports "target exited with status 0 before readiness"
for them:

- It calls `tracer::launch::wait_for_resume()` at the start of `main`.
- It does not wait for terminal input. `%launch` gives it `/dev/null` as
  standard input unless you pass `--stdin FILE`.

Only one terminal is needed. Run `make`, start `clang-tracer-repl` in this
directory, and enter:

```bash
%launch ./target
%probe inject "add(int, int)" --callback hook.cpp --print-arguments --print-result
%resume
%collect --name "add(int, int)" --count 2 --timeout 30 --as launched
%events list launched
%terminate
%quit
```

Expected: `launched pid=... paused=true`, then a PRE event with
`left=20 right=22` and a POST event with `sum=42 result=42`. The program's
output goes to the `stdout.log` path printed by `%launch`.

The server must run on the same machine as the REPL. If Clang Tracer is not
installed under `/usr/local/opt/clang-tracer/current`, build with
`make TRACER_INCLUDE=/path/to/include`.

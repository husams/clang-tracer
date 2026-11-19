# Lesson 5: trace a function template instantiation

Guide section: [Trace a function template instantiation](../../../docs/guide/tutorials.md#trace-a-function-template-instantiation).

| File | Purpose |
| --- | --- |
| `echo.hpp` | The `echo<T>` function template. |
| `target.cpp` | The first lesson's target, which also calls `echo<int>(42)` on Enter. |
| `echo-hook.cpp` | Observer specialized on `tracer::ResolvedSignature`. |
| `Makefile` | Builds `target` and writes `compile_commands.json` for this directory. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%symbols --filter "echo<"
%probe prepare "echo<int>(int)" --include echo.hpp --translation-unit target.cpp --module ./target --output prepared-echo
%probe validate "echo<int>(int)" --callback echo-hook.cpp --prepared prepared-echo --prepared-mode require
%probe inject "echo<int>(int)" --callback echo-hook.cpp --prepared prepared-echo --prepared-mode require --print-result
%collect --name "echo<int>(int)" --count 2 --timeout 30 --as echo_call
%events list echo_call
%detach
```

Press Enter in the target terminal while `%collect` waits.

Expected: PRE shows `input=42` and POST shows `output=42`. Run `make clean`
before repeating the lesson so `prepared-echo` can be created again.

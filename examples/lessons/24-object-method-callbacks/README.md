# Lesson 24: Prepared C++ method callbacks

Attach to a running Linux x86-64 target and prepare typed callbacks from its
source. Observe an ordinary method and a concrete override reached through a
base reference. Use `make`, a C++17 compiler, Python 3, and a matching
installed `clang-tracer-repl`, server and agent.

## Build and attach

Run these commands from this lesson directory. `make` builds the fixture and
uses the same compiler, flags and working directory to write
`build/compile_commands.json`.

```sh
make
sha256sum build/object-method-target > target.sha256
./build/object-method-target
```

The target shows its PID and waits for input. In the second terminal, from the
same lesson directory, substitute the displayed PID:

```sh
clang-tracer-repl --attach PID --fail-fast
```

## Observe the ordinary method

At the attached prompt, run `%probe prepare` with the method signature,
declaration header, compilation database and the translation unit to select
from that database. Use a fresh output directory when preparing again. The
REPL resolves the executable through the attached process when injecting.

```text
%probe prepare "lesson::Counter::add(int)" --include ./objects.hpp --compdb ./build/compile_commands.json --translation-unit ./target.cpp --output ./counter-package
%probe validate "lesson::Counter::add(int)" --callback ./counter-hook.cpp --prepared ./counter-package --prepared-mode require
%probe inject "lesson::Counter::add(int)" --callback ./counter-hook.cpp --prepared ./counter-package --prepared-mode require
%collect --count 2 --timeout 30 --as ordinary
```

Validation reports `prepared_used: true`, and injection prints a probe ID.
Press Enter once in the target terminal. The target prints `sum=12 score=15`.
The REPL should report `received=2` and `complete=true`. At the next prompt:

```text
%events list ordinary
%events show ordinary --index 0
%events show ordinary --index 1
```

The two rows share a call ID. PRE has argument 2 and the callback value 12
from adding the incoming argument to the receiver's initial value, 10. POST
has result 12, which the callback also returns as its event value.
`counter-hook.cpp` uses `call.self()`, `call.argument<0>()` and
`call.result()`. The hook's `long long` is recorded as event metadata.

Exit with `%quit` to detach. Leave the target waiting at its next input.

## Observe the override

Attach again and prepare `Derived::score(int) const` from the same source:

```sh
clang-tracer-repl --attach PID --fail-fast
```

```text
%probe prepare "lesson::Derived::score(int) const" --include ./objects.hpp --compdb ./build/compile_commands.json --translation-unit ./target.cpp --output ./derived-package
%probe validate "lesson::Derived::score(int) const" --callback ./derived-hook.cpp --prepared ./derived-package --prepared-mode require
%probe inject "lesson::Derived::score(int) const" --callback ./derived-hook.cpp --prepared ./derived-package --prepared-mode require
%collect --count 2 --timeout 30 --as virtual_call
```

In the target terminal, type `b` followed by Enter, then press Enter once
more. The first input calls the qualified base implementation and prints
`qualified base score=10`. The second goes through `Base&`, reaches the
selected override, and prints `sum=12 score=15`.

```text
%events list virtual_call
%events show virtual_call --index 0
%events show virtual_call --index 1
```

The collected PRE and POST belong to the call that reached the override.
Its argument is 3. The PRE callback reports `bonus + 3 = 8`, and POST
reports result 15. The experiment selects a concrete entry by name. Type-wide
callback registration and automatic receiver classification remain proposals.

## Try a variation

- Collect another pair and type `a` in the target. On the override, the
  argument is 8 and the result is 20. To collect only newer events, pass
  `--after CURSOR` with the cursor printed by the previous collection.
- After building, move `objects.hpp` temporarily out of the lesson directory.
  Reattach using either startup script. `--prepared-mode require` verifies
  that the prepared adapter is sufficient. Restore the header before editing
  or rebuilding the fixture.
- Edit either hook's PRE calculation, then quit and reattach for that hook.
  Compare its callback value with the unchanged result printed by the target.

Quit the REPL with `%quit` and enter `q` in the target terminal. Verify the
executable digest with `sha256sum -c target.sha256`.

## Prepare before building

In a fresh copy of `examples/lessons`, enter this lesson directory. Generate
the compilation database, prepare both packages from source, and build the
target using the same compiler and flags for both `make` commands.

```sh
make build/compile_commands.json
clang-tracer-repl --fail-fast < prepare.repl
make
```

Start the target as above. Attach with `--startup counter.repl` or
`--startup derived.repl` to validate and inject the corresponding package.

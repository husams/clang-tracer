# Lesson 9: prepare a reusable probe

Guide section: [Prepare a reusable probe](../../../docs/guide/tutorials.md#prepare-a-reusable-probe).

`%probe inject` normally resolves the function and compiles your callback live.
`%probe prepare` does the expensive part once, from your headers and compile
command, and writes a package you can reuse and check.

| File | Purpose |
| --- | --- |
| `add.hpp` | The declaration of `add`, as the application would ship it. |
| `target.cpp` | Includes `add.hpp` and defines `add`. |
| `Makefile` | Builds `target` and writes `compile_commands.json` for this directory. |
| `hook.cpp` | The lesson 1 observer. |

```bash
make
./target
```

Generate the package. `--output` must name a new directory.

```bash
%probe prepare "add(int, int)" --include add.hpp --translation-unit target.cpp --module ./target --output prepared
%attach 12345
%probe validate "add(int, int)" --callback hook.cpp --prepared prepared --prepared-mode require
%probe inject "add(int, int)" --callback hook.cpp --prepared prepared --prepared-mode require --print-result
%collect --count 2 --timeout 30 --as prepared_call
%events show prepared_call --index 1
```

Press Enter once while `%collect` waits. Expected: the POST event has `sum=42`.
`--prepared-mode require` fails if the package cannot be used, so a passing
inject proves the package was used; `prefer` falls back to live resolution and `off` never uses it.

Repeat `--include` to bring in more headers that the callback payload needs.
`--cwd` runs the command relative to another project directory and
`--extra-arg` passes supplemental compiler flags. The shell workflow is in the
[command reference](../../../docs/guide/commands.md#offline-probe-preparation).
For methods and a custom typed event, run the [prepared-injectors example](../../prepared-injectors/README.md).

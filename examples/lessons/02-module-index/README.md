# Lesson 2: module index and an indexed probe

Guide section: [Index debug information and prepare a probe from a binary](../../../docs/guide/user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

| File | Purpose |
| --- | --- |
| `target.cpp`, `hook.cpp` | The first lesson's target and observer. |
| `Makefile` | `make` builds `target`; `make split` also creates `target-split` and `target-split.debug`. |

## Build and browse the index

Run `make`, start `clang-tracer-repl` in this directory, and enter the
following. No target needs to be running.

```bash
%module index --file ./target --output target.sqlite
%module types --filter int
%module symbols --filter "add*"
%module show --symbol "add(int, int)"
%module show --type int
```

For the separate-debug step, `make split` runs the guide's `cp` and `objcopy`
commands. Then, in a shell:

```bash
make split
clang-tracer-module index --file ./target-split --debug-file ./target-split.debug --output target-split.sqlite
clang-tracer-module info --index target-split.sqlite
```

## Generate declarations and install the indexed probe

Back in the REPL:

```bash
%module use target.sqlite
%module emit --symbol "add(int, int)" --output recovered-add
%probe prepare "add(int, int)" --module-index target.sqlite --module ./target --generated recovered-add --output prepared-add
```

Start `./target` in another terminal, then:

```bash
%attach 12345
%probe validate "add(int, int)" --callback hook.cpp --prepared prepared-add --prepared-mode require
%probe inject "add(int, int)" --callback hook.cpp --prepared prepared-add --prepared-mode require --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as indexed
%events list indexed
%detach
```

Expected: validation reports `prepared_used: true` and the POST event shows
`sum=42`. An index file cannot be overwritten, so run `make clean` before
repeating the lesson.

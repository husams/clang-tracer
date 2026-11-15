# Lesson 17: trace an object's lifetime and the writes to one field

Reference: [Trace object lifetimes and state in a prebuilt binary](../../../docs/native-repl.md#trace-object-lifetimes-and-state-in-a-prebuilt-binary)
(REPL walkthrough; the user guide has no section for this lesson yet).

Earlier lessons traced function calls. An object watch follows one object
instead: when it is constructed, every write to a chosen field, and when it is
destroyed. You need no observer source and no header. The watch is prepared
from the binary and its debug information alone, and the binary on disk is
never changed.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `account.hpp` | `bank::Account` with an `id` and a `balance`, and the declaration of `bank::deposit`. |
| `account.cpp` | The constructor, the destructor and `deposit`, which adds to `balance`. |
| `target.cpp` | Opens an account with balance 100, deposits 25 and 40, and closes it each time you press Enter. |
| `Makefile` | Builds `target` with `-O2 -g`. |

The constructor, the destructor and `deposit` live in `account.cpp` so the
compiler keeps them as real functions. A watch cannot see a constructor or a
writer that was inlined away.

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare, preview and start the watch

`%object prepare` needs no running process. It finds the field, the writer
and the constructor and destructor in the binary, and saves the plan in
`account-watch`.

```bash
%object prepare bank::Account --module ./target --field balance --code "bank::deposit(bank::Account &, int)" --backend code-cache --output ./account-watch
%attach 12345
%object preview --plan ./account-watch
%object start --plan ./account-watch --stack-depth 4
```

Expected: prepare lists a `writer`, a `construct`, a `destroy` and a `store`
site and reports `binding=offline`. After `%attach`, preview shows the same
sites with live addresses and `binding=same-build`, and changes nothing in
the process. Start prints `active=yes` and the watch ID.

## Part 2: record one account

```bash
%record account.db --sqlite --name object.trace --count 5 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=5` and `complete=true`.

```bash
%object status
%object stop
%detach
```

Expected: status reports `executed_stores=2` and `dropped=0`. Stop reports
`active=no`, `restoration_pending=no` and `all code-cache redirects
restored`. Check both before you detach. If restoration is pending, run
`%object stop` again.

## Part 3: replay the recording

The recording is a file, so this part also works in a new REPL with no
target running.

```bash
%events open account.db --as saved
%events list saved
%events show saved --index 1
%events stack saved --index 1
```

Expected: five events for `bank::Account#1`.

```text
0 CREATED bank::Account#1 ... balance 100 ... function="bank::Account::Account(int, int)" ... account.cpp:4
1 FIELD_CHANGED bank::Account#1 ... bank::Account::balance 100 -> 125 ... function="bank::deposit(bank::Account&, int)" ... account.cpp:8
2 FIELD_CHANGED bank::Account#1 ... bank::Account::balance 125 -> 165 ... account.cpp:8
3 DESTROY_BEGIN bank::Account#1 ... function="bank::Account::~Account()" ... account.cpp:5
4 TEARDOWN_END bank::Account#1
```

`%events show` prints the whole record, including `old_integer: 100` and
`new_integer: 125`. `%events stack` starts at `bank::deposit` on
`account.cpp:8`, the line that wrote the field.

## How the watch works

- The watch patches the running process only. `--backend code-cache` runs the
  store inside the target without stopping it for every write. The default,
  `--backend ptrace`, single-steps each store and is meant for diagnosis.
- `#1` is the lifetime. The next account you open is `#2`, even though it
  reuses the same stack address. `DESTROY_BEGIN` and `TEARDOWN_END` mark the
  start and the end of the destructor.
- Preview and status say `coverage=partial`, because only the store in
  `deposit` is watched. A write to `balance` from any other function is not
  reported.

## Things to try

- Record every store without reading the values. Restart the target, attach,
  and start the same plan with `--no-values`, then record to a new file:

  ```bash
  %object start --plan ./account-watch --no-values
  %record account-all.db --sqlite --name object.trace --count 5 --timeout 30
  ```

  The two writes appear as `FIELD_WRITTEN` with no old and new value.
- Record `--count 10` to a new file and press Enter twice. The second account
  is `bank::Account#2` at the same address as the first.

Run `make clean` before repeating the lesson so `account-watch` can be created
again.

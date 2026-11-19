# Lesson 18: see what one method call changed in an object

Reference: [Prepare an object watch for method snapshots](../../../docs/native-repl.md#prepare-an-object-watch-for-method-snapshots)
(REPL walkthrough; the user guide has no section for this lesson yet).

Lesson 17 watched single writes to one integer field. A method snapshot works
at a coarser level: it copies the object when a method is entered and again
when it returns, and reports the difference. That reaches members a write
watch cannot follow, such as vectors, maps and strings, and it tells you the
net effect of the call without naming every store.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `cart.hpp` | `shop::Cart` with `lines` (vector), `discounts` (map) and `total`. |
| `cart.cpp` | `Cart::apply_offer`, which changes all three members. |
| `target.cpp` | Calls `apply_offer` on one cart each time you press Enter. |
| `Makefile` | Builds `target` and writes `compile_commands.json` for this directory. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare the snapshot watch

A snapshot has to read the object's members, so prepare needs the header
that defines the class and the compile command of a source file that
includes it.

```bash
%object prepare shop::Cart --module ./target --include ./cart.hpp --compdb ./compile_commands.json --translation-unit ./cart.cpp --on "shop::Cart::apply_offer()" --field lines --field discounts --field total --key lines=id --key discounts=key --synchronization thread-confined --output ./cart-watch --emit-source ./generated
%attach 12345
%object preview --plan ./cart-watch
%object start --plan ./cart-watch --stack-depth 4
```

- `--on` names the method whose entry and return are sampled.
- `--field` selects the members to compare. Repeat it for each one.
- `--key lines=id` names a changed line by its `id` member instead of its
  position. `--key discounts=key` names a map entry by its key.
- `--synchronization thread-confined` states that only one thread uses the
  cart. The watch relies on this to read the object safely.
- `--emit-source ./generated` keeps the generated reader so you can look at it.

Expected: prepare prints `object package=` and a `method` site. Preview
reports `backend=method-detour` and `binding=same-build`. Start prints
`active=yes` and a `method_probe=... active=yes` line.

## Part 2: record one call

```bash
%record cart.db --sqlite --name object.trace --count 3 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=3` and `complete=true`.

```bash
%object status
%object stop
%detach
%events open cart.db --as saved
%events list saved
%events show saved --index 1
```

Expected: stop reports `active=no` and `method_probe=... restored=yes`. The
recording holds one event for each member the call changed:

```text
0 SNAPSHOT_DELTA discounts["bundle"] ... function="shop::Cart::apply_offer()" ... | added
1 SNAPSHOT_DELTA lines[id=12].quantity ... | 1 -> 2
2 SNAPSHOT_DELTA total ... | 0 -> 35
```

`%events show` prints `snapshot_before`, `snapshot_after` and
`location_kind: CALL_BOUNDARY`. Its `call_id` numbers the call: the events
of the next `apply_offer` call carry `call_id: 2`.

## How the snapshot works

- A delta is the net change between entry and return. `total` is written
  twice inside `apply_offer`, but you see one `0 -> 35`.
- Every event points at the method entry (`cart.cpp:5`), not at the line that
  made the change. Use a write watch (lesson 17) when you need the exact line.
- An unchanged member produces no event. Line 11 is untouched, so only
  `lines[id=12]` is reported.

## Things to try

- Record `--count 3` to a new file and press Enter again. The discount is now
  a change (`5 -> 10`) instead of `added`, and the total goes `35 -> 40`.
- Prepare a second plan with only `--field total` and a different `--output`.
  The same call then produces one event.

Run `make clean` before repeating the lesson so `cart-watch` and `generated`
can be created again.

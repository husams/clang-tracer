# Lesson 19: follow one vector entry while its storage moves

Reference: [Follow logical collection entries during writes](../../../docs/native-repl.md#follow-logical-collection-entries-during-writes)
(REPL walkthrough; the user guide has no section for this lesson yet).

Lesson 17 watched a field at a fixed place inside an object. A field inside a
vector element has no fixed place: when the vector grows, every element moves
to a new address. This lesson watches `quantity` of the cart line with
`id == 12` and shows the watch following that line to its new storage.

This lesson needs a Linux x86-64 build of the tools that includes `%object`.

| File | Purpose |
| --- | --- |
| `cart.hpp` | `shop::Cart` with a vector of `Line{id, quantity}`. |
| `cart.cpp` | `shop::grow`, which doubles the capacity, and `Cart::restock`, which writes line 12, grows the vector, and writes line 12 again. |
| `target.cpp` | Calls `restock` on one cart each time you press Enter. |
| `Makefile` | Builds `target` with `-O2 -g`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## Part 1: prepare the watch

```bash
%object prepare shop::Cart --module ./target --field quantity --vector lines --element-key id=12 --code "shop::Cart::restock()" --mutation _ZN4shop4growERSt6vectorINS_4LineESaIS1_EE --output ./line-watch
%attach 12345
%object preview --plan ./line-watch
%object start --plan ./line-watch --stack-depth 4
```

- `--vector lines --element-key id=12` selects the element whose `id` is 12.
  `--field quantity` is the member of that element to watch.
- `--code` is the function whose writes are reported.
- `--mutation` is the function that can move the storage. It must take the
  vector as its first argument. The name here is the linker symbol of
  `shop::grow`; list it with `nm target | grep grow`. The full name printed by
  `nm -C` also works if you quote it.

Expected: prepare lists a `writer`, a `container mutation`, and two `store`
sites, one for each write in `restock`. Preview reports `binding=same-build`.

## Part 2: record one restock

```bash
%record lines.db --sqlite --name object.trace --count 7 --timeout 30
```

Press Enter once in the target terminal while `%record` waits. Expected:
`received=7` and `complete=true`.

```bash
%object stop
%detach
%events open lines.db --as saved
%events list saved
```

Expected: seven events. The addresses differ on your machine; the pattern
does not.

```text
0 FIRST_SEEN       shop::Cart#1 ... function="shop::Cart::restock()"
1 RANGE_BOUND      shop::Cart::lines[id=12].quantity destination=0x...1ecc
2 FIELD_CHANGED    shop::Cart::lines[id=12].quantity 1 -> 6  destination=0x...1ecc ... cart.cpp:10
3 RANGE_SUSPENDED  shop::Cart::lines[id=12].quantity ... function="shop::grow(...)"
4 RANGE_BOUND      shop::Cart::lines[id=12].quantity destination=0x...4a3c ... function="shop::grow(...)"
5 FIELD_CHANGED    shop::Cart::lines[id=12].quantity 6 -> 11 destination=0x...4a3c ... cart.cpp:12
6 OBSERVATION_ENDED shop::Cart#1
```

## How to read the events

- `FIRST_SEEN`: the cart was built before you attached, so the watch never saw
  its constructor. It identifies the cart for the length of this one
  `restock` call. `OBSERVATION_ENDED` closes that call.
- `RANGE_BOUND`: the watch found line 12 and now watches its address.
- `RANGE_SUSPENDED`: `grow` was entered, so the address can no longer be
  trusted. When `grow` returns, the watch looks line 12 up again and binds the
  new address. Compare `destination` in events 1 and 4.
- The two `FIELD_CHANGED` events are the same line at two different
  addresses.

## Things to try

- Change `--element-key` to `id=11` in a new plan with a different `--output`.
  `restock` never writes that line, so you get the bind and suspend events but
  no `FIELD_CHANGED`.
- Use a key that does not exist, such as `id=99`. The watch reports
  `COVERAGE_GAP` events instead of a binding, and `%object status` counts
  them in `coverage_gaps`.

In both cases `%record --count 7` waits for its timeout, because fewer than
seven events arrive.

Run `make clean` before repeating the lesson so `line-watch` can be created
again.

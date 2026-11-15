# Lesson 26: Nested union stores and keyed collection updates

Watch the same union store with and without a discriminator, then follow a
vector entry by its ID as it moves, disappears and returns. Both exercises
use the native REPL's existing `%object` commands and replay the events
from SQLite. User-supplied object change callbacks are proposed in the
[custom object probe design](https://chatgpt.com/space/page_fc052ad1306481919d26962eb86a984c).

## Build once

On Linux x86-64, run from this lesson directory with the matching
`clang-tracer-repl`, server and agent installed. The fixture uses C++17 and
the libstdc++ vector layout.

```sh
make
sha256sum target > target.sha256
```

You can choose the compiler with `make CXX=/path/to/g++`. Build with the
included flags for the expected writer instructions. To switch compilers,
run `make clean` before rebuilding. Each trace uses the existing executable.

## Exercise 1: compare the tag and the bytes

In the target terminal, start the union workload. It prints its PID and waits.
Each Enter starts at an amount of 100, raises it to 250, switches the active
member to a rate of 0.25, and resets the input for the next run.

```sh
./target union
```

Open a second terminal in this lesson directory. Use a fresh run directory
so you can repeat the exercise and compare recordings.

```sh
run=$(mktemp -d run.XXXXXX)
cd "$run"
clang-tracer-repl
```

Attach using the PID printed by the target. The tracer locates the executable
and its matching debug information from the attachment. `--first-seen`
scopes observation to one invocation of the selected writer. At each
`%record`, press Enter once in the target terminal. The first recording
captures a typed sample while the tag stays stable.

```text
%attach PID
%object start --type shop::Discount --field state.value --code "shop::Discount::raise()" --union-tag state.kind --union-case 1=amount --union-case 2=rate --first-seen --synchronization thread-confined --backend code-cache
%record raise.db --sqlite --name object.trace --count 3 --timeout 30
%object stop
%detach
%events open raise.db --as raised
%events list raised
```

Expect `active=yes`, a complete recording, and a `REPRESENTATION`
store with tag 1 and amount 100 → 250. Attach again for a fresh event stream
and watch the transition.

```text
%attach PID
%object start --type shop::Discount --field state.value --code "shop::Discount::to_rate()" --union-tag state.kind --union-case 1=amount --union-case 2=rate --first-seen --synchronization thread-confined --backend code-cache
%record tagged.db --sqlite --name object.trace --count 4 --timeout 30
%object stop
%detach
%events open tagged.db --as switched
%events list switched
%events show switched --index 1
```

The four events include the `UNION_TAG` store from 1 to 2 and then the
`REPRESENTATION` store of rate 0.25. The typed value before the rate write is
unavailable because those bytes belonged to the previous member. The
`storage_write` in the tag event includes both discriminator values.

```text
%attach PID
%object start --type shop::Discount --field state.value --code "shop::Discount::to_rate()" --first-seen --synchronization thread-confined --backend code-cache
%record raw.db --sqlite --name object.trace --count 3 --timeout 30
%object stop
%detach
%events open raw.db --as untagged
%events list untagged
%events show untagged --index 1
%quit
```

The untagged recording includes the overlapping bytes at the union store.
For a fourth recording, rerun the tagged watch in a fresh session with only
`--union-case 1=amount`. The discriminator changes to 2 and the new
alternative remains raw bytes.

## Exercise 2: follow a cart line by ID

Stop the union target with Ctrl+C. From the lesson directory, run the
collection workload in the target terminal.

```sh
./target cart
```

In the second terminal, return to the lesson directory and create another
run directory.

```sh
run=$(mktemp -d run.XXXXXX)
cd "$run"
clang-tracer-repl
```

The cart starts with line 12 at index 1 and quantity 3. The writer rotates the
line to index 0, erases it, appends line 12 with quantity 1, then writes
quantity 5. Declare each function that changes the vector so the watch can
rebind the entry on its verified return. The fixture is single-threaded.

```text
%attach PID
%object start --type shop::Cart --mode logical-writes --field quantity --vector lines --element-key id=12 --code "shop::Cart::rebuild()" --synchronization thread-confined --mutation "shop::move_to_front" --mutation "shop::drop_front" --mutation "shop::add_line"
%record cart.db --sqlite --name object.trace --count 11 --timeout 30
```

Press Enter once in the cart terminal. Expect eleven events, including these
four in order.

| Event | Evidence |
| --- | --- |
| `LOGICAL_UPDATE MOVED` | Index 1 → 0, quantity 3 → 3 |
| `LOGICAL_UPDATE ERASED` | Last observed quantity 3 |
| `LOGICAL_UPDATE INSERTED` | Index 1 and new quantity 1 |
| `FIELD_CHANGED` | Quantity 1 → 5 at the store in `Cart::rebuild` |

```text
%object status
%object stop
%detach
%events open cart.db --as updates
%events list updates
%events export updates --format json --output cart.json
%quit
```

Find `CALL_BOUNDARY` in the exported logical updates and `STORE` on the
final field write. To see a coverage gap, start a fresh watch with the
`shop::drop_front` mutation omitted and record into a new database with a
short timeout. A missing mutation boundary prevents the tracer from safely
attributing the erase and reinsertion.

Stop the target with Ctrl+C. From the lesson directory, verify the binary
checksum after tracing.

```sh
sha256sum -c target.sha256
```

See the native REPL guide for
[union stores](../../../docs/native-repl.md#trace-individual-writes-to-a-nested-union)
and [logical collection updates](../../../docs/native-repl.md#follow-logical-collection-entries-during-writes).
The [lifetime and state lesson](../25-object-lifetimes-and-state/README.md)
shows how to prepare from source for direct writer discovery or method snapshots.
The fixtures are adapted from the
[C++17 logical updates](https://github.com/husams/clang-tracer/tree/0d8416fb72511d9b96ccddc833fb07e81f01b363/examples/lessons/21-logical-updates)
and [union writes](https://github.com/husams/clang-tracer/tree/0d8416fb72511d9b96ccddc833fb07e81f01b363/examples/lessons/23-union-writes)
lessons.

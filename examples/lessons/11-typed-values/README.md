# Lesson 11: decode your own record as a C++ value

Guide section: [Decode objects, containers, and method receivers](../../../docs/guide/tutorials.md#decode-objects-containers-and-method-receivers).

The first half of that section runs the installed
[native-repl-values](../../native-repl-values/README.md) example and needs no
project. This lesson covers the second half: mapping your own public record.

| File | Purpose |
| --- | --- |
| `model.hpp` | `app::Order` and the declaration of `app::submit`. |
| `src/target.cpp` | Calls `app::submit(order)` each time you press Enter. |
| `order-hook.cpp` | Observer that reads `order.id` and `order.quantity` from debug information. |
| `Makefile` | Builds `target` and writes `build/compile_commands.json`. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this
directory. Set the compilation options before any C++ input.

```bash
%cpp options compdb build/compile_commands.json --file src/target.cpp
%cpp import model.hpp --type app::Order
%attach 12345
%probe inject "app::submit(const app::Order&)" --callback order-hook.cpp --print-arguments --print-result
%collect --count 2 --timeout 30 --as orders
%events list orders
```

Press Enter in the target terminal while `%collect` waits, then read the
argument as an `app::Order`:

```cpp
auto sample = tracer.batch("orders").at(0);
auto order = sample.argument_as<app::Order>(0);
order.quantity
order.unit_price
sample.argument_snapshot(0).field("id").as<int>()
```

```bash
%detach
```

Expected: `order.quantity` is `3`, `order.unit_price` is `19.5`, and the
snapshot field matches the `order.id` shown in the PRE event.

This lesson uses `%cpp import`, the one-step form. With the installed build,
the two-step form in the guide fails at `%cpp load mappings.hpp` with
"C++ checker stopped unexpectedly". If you want the two-step form, generate
the file and include it instead:

```bash
%cpp generate model.hpp --type app::Order --output mappings.hpp
```

```cpp
#include "mappings.hpp"
```

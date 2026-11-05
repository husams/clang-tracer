# Typed values in the native C++17 REPL

These examples attach to a real process, upload an interceptor, collect POST
events, and read arguments and results as C++ values. Run them on Linux x86-64
with `clang-tracer-repl` and `clang-tracer-server` installed on `PATH`, plus
Clang 21 (`clang++-21`). The target and REPL examples use C++17.
The existing Linux process-attach/ptrace permission requirements also apply.

From this directory:

```sh
bash run.sh all
```

With the standard `/usr/local` installation (including linux-dev), run from
any directory:

```sh
bash /usr/local/share/clang-tracer/examples/native-repl-values/run.sh all
```

The runner compiles a small target with debug information, starts it, and runs
seven demonstrations. Each prints the actual values and `example passed: NAME`
after checking them. It detaches and stops its own server after each case,
then stops its target on exit. It uses a temporary build directory, so the
installed examples directory can be read-only. It needs no Python.
Only `target.cpp` is compiled as a program; the other `.cpp` files are REPL
cells loaded by `%cpp load`, without a `main()` function.

Run individual categories:

```sh
bash run.sh user
bash run.sh simple
bash run.sh sequences
bash run.sh associative
# Or run a single associative type:
bash run.sh map
bash run.sh unordered_map
bash run.sh set
bash run.sh unordered_set
```

For an interactive prompt with Tab completion:

```sh
bash run.sh user --interactive
```

Enter each printed command separately and press Enter before entering the next;
the editor treats a multiline paste as one cell. After the user-type example
finishes, `data` and `event` remain available for exploration, for example:

```cpp
event.argument_snapshot(1).field("owner").as<std::string>()
tracer.print(data.at(0))
```

Use `%help EventValue`, Tab after `event.`, or `%quit` to finish. Each category
gets a fresh REPL so their example variable names do not conflict. The runner
uses a 60-second operation budget for compiling the target's headers/codecs.
Set `CXX`, `CLANG_TRACER_REPL`, or `CLANG_TRACER_SERVER` to use other matching
compiler/runtime executables.
`CLANG_TRACER_VALUES_TARGET` can reuse a prebuilt copy of this example's target
instead of compiling it again; the runner still starts and stops its own process.

## 1. User-defined structs and classes

[`types.hpp`](types.hpp) defines a struct `Address` and a class `Account` with a
nested `Address`. The target receives both **by value** and returns an updated
`Account` by value. [`user.cpp`](user.cpp) compares the captured input with the
returned object, calls a method on the reconstructed class, and inspects nested
fields without converting to a user-defined C++ type.

```cpp
auto address = event.argument_as<value_examples::Address>(0);
auto before = event.argument_as<value_examples::Account>(1);
auto after = event.result_as<value_examples::Account>();
std::cout << after.describe() << std::endl;
auto city = event.argument_snapshot(1).field("address").field("city")
                .as<std::string>();
```

[`mappings.hpp`](mappings.hpp) contains explicit `EventRecord<T>` field mappings
for reconstructing these local types. The same `Address` mapping also works
inside `std::vector<Address>`. Automatic capture requires public serializable
fields; the example class deliberately uses public fields. A local
`EventCodec<T>::decode(const EventValue&)` factory is an alternative for local
types that cannot be default-constructed/assigned; it does not expose private
fields of a target class.

Expected output includes:

```text
struct Address: Paris, 75001
class Account before: Ada #7 in London
class Account returned: Ada #8 in Paris
field inspection: Ada, London
example passed: user
```

## 2. Scalars and simple standard types

[`simple.cpp`](simple.cpp) reads `bool`, `int`, `double`, `std::string`, and
`std::optional<int>`, plus a `std::string` result. The optional alternates
between a present zero and `std::nullopt`; the checks distinguish those cases.
Standard types need no `EventRecord` mapping.

```cpp
bool enabled = event.argument_as<bool>(0);
int count = event.argument_as<int>(1);
double ratio = event.argument_as<double>(2);
auto text = event.argument_as<std::string>(3);
auto serial = event.argument_as<std::optional<int>>(4);
auto returned = event.result_as<std::string>();
```

Expected lines include `bool=true int=3 double=1.25 string=hello`, both
`optional=0` and `optional=none`, and `returned=hello-returned`.

## 3. Sequence containers and nested records

[`sequences.cpp`](sequences.cpp) reads each of these argument types separately:

| Index | C++ type | Example access |
| --- | --- | --- |
| 0 | `std::array<int, 3>` | `coordinates.at(2)` → `6` |
| 1 | `std::vector<Address>` | `addresses.at(0).city` → `Paris` |
| 2 | `std::list<int>` | `numbers.front()` → `3` |
| 3 | `std::deque<std::string>` | `messages.front()` → `queued` |
| 4 | `std::vector<std::vector<int>>` | `matrix.at(0).at(1)` → `2` |

The nested vector also contains an empty inner vector, which stays empty after
capture and reconstruction.

## 4. Associative containers

Each example has its own function, probe, and source file. This keeps their
combined DWARF metadata within the tracing service's existing bounds.

| Source | C++ argument type (index 0) | Example access |
| --- | --- | --- |
| [`map.cpp`](map.cpp) | `std::map<std::string, int>` | `counts.at("apples")` → `2` |
| [`unordered_map.cpp`](unordered_map.cpp) | `std::unordered_map<int, std::vector<int>>` | `groups.at(2).at(1)` → `4` |
| [`set.cpp`](set.cpp) | `std::set<int>` | `ids.count(4)` → `1` |
| [`unordered_set.cpp`](unordered_set.cpp) | `std::unordered_set<std::string>` | `labels.count("red")` → `1` |

The unordered map also contains an empty mapped vector. The examples compare
contents, not the iteration order of unordered containers.

## Capture boundaries

Values are owned snapshots: arguments reflect entry values and results reflect
return values. They remain readable after detach or target exit. These are
examples cover the supported type families. Automatic target capture excludes
private/protected fields, unions, bit-fields, raw pointees, wide/custom strings,
smart pointers, `std::tuple`, and `std::variant`. Existing snapshot
size/schema limits still apply; see
[`docs/native-repl.md`](../../docs/native-repl.md).

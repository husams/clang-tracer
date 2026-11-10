# Generate and inject from the native C++ REPL

Build and launch the example on Linux x86-64 from the repository root:

```bash
cmake -S . -B build -G Ninja -DTRACER_BUILD_REPL=ON
cmake --build build --target clang-tracer-repl clang-tracer-server clang_tracer_agent prepared-injectors-target
./examples/prepared-injectors/run-native.sh
```

The launcher prints the example directory and target PID, attaches automatically,
and presents the native `tracer[PID]>` prompt. Paste the commands below into that
prompt. Keep each `%` command on one line.

## 1. Generate the package from the header

```text
%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
```

The launcher links the build's compilation database into the current directory.
`%probe prepare` uses `./compile_commands.json` by default. Pass `--compdb FILE`
to select another database.

Preparation prefers the first header's recorded compile command. If that header
has no entry, it uses the first eligible source entry in the same directory. Pass
`--translation-unit FILE` to select an entry explicitly.

The selector chooses the integer overload declared in the header. Clang parses
the headers with the selected flags and records the symbol and ABI. This can
run before the target is built. Pass `--module ELF` during preparation if you
want to pin the package to a particular build.

The newly created package contains:

```text
prepared/
  prepared.pb
  generated/
    adapter.hpp
    adapter.cpp
```

The native runtime consumes `prepared.pb`. A typed package embeds the generated
declarations and the standard library headers they need. Preparation uses the
application headers to extract and verify the adapter. Injection uses the
adapter from the package. Use `--metadata-only` to save just `prepared.pb`,
including the embedded adapter. The optional adapter files remain separate from
the interceptor. Use a new output directory each time.

## 2. Validate against the running application

```text
%probe validate "prepared_demo::compute(int)" --callback demo/interceptor.cpp
```

Expect `success: true` and `prepared_used: true`. Validation checks the prepared
contract against the mapped executable and compiles the callback without
installing a hook.

The entire example interceptor is:

```cpp
template <> class Interceptor<int(int)> {
public:
  long long pre(const Call &call) { return call.arg<int>(0) * 100 + 17; }
  long long post(const Call &call) { return call.result<int>(); }
};
```

The callback observes arguments and results. The target still computes
`value * 3 + 7`; for input `1`, its result is `10`.

## 3. Inject and collect

```text
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
```

Injection reports `installed probe=...`; retain that number for removal. Expect
a collection containing four POST events.
Then enter ordinary C++ at the same native prompt:

```cpp
auto captured = tracer.batch("captured");
captured.size()
tracer.print(captured.at(0))
for (const auto& e : captured) { std::cout << e.argument_as<int>(0) << " -> " << e.result_as<int>() << '\n'; }
```

For background capture, type `tracer.events_a` and press Tab to complete
`tracer.events_async`. After injecting, start capture with
`auto live = tracer.events_async(session);`. Run `live.snapshot().size()`
to see how many events are retained and `live.stop();` to stop capture.

## 4. Restore and exit

```text
%probe remove PROBE_ID
%quit
```

Replace `PROBE_ID` with the number reported during injection. Removal reports
`restored=true`. Quitting detaches and stops the server owned by
this console; the launcher stops its example application. Generated files and
`target.log` remain in the directory printed at startup. Use `%quit` to leave the
native prompt.

## Equivalent native C++ API

In a fresh example session, these calls perform preparation and injection without
the `%probe` commands:

```cpp
tracer.prepare("prepared_demo::compute(int)", "demo/calculator.hpp", "prepared_cpp");
auto hook = tracer.inject(session, "prepared_demo::compute(int)", "demo/interceptor.cpp");
auto events = tracer.collect(session, 4, 15);
tracer.print(events);
```

Use `%quit` to restore and detach after this variant, or `%probe remove` with the
probe ID reported during injection.

## Modes and scope of this draft

- `require`: use the prepared package or report why it could not be bound.
- `prefer`: attempt prepared resolution, then fall back to DWARF if it fails.
- `off`: use the ordinary DWARF resolution path.

The REPL automatically tries the latest package it generated for the selector
and falls back to DWARF when it cannot use the package. Pass
`--prepared DIRECTORY` to select a package in another directory or from another
REPL session. Fallback happens during resolution, before the hook is installed.

Prepared packages support concrete Linux x86-64 free functions and methods.
For virtual dispatch, select the override, such as `Derived::run(int) const`.
Calls through a base pointer are captured when dispatch reaches that
implementation, including calls through an adjusting thunk. A selector
installs one hook at its executable entry. Typed packages retain the declared C++ signature and
header layout. The declarations and build flags must describe the running
function's ABI. Injection binds the package to the symbol in the mapped ELF
image and checks the adapter's ABI and layouts before compiling the callback. If multiple
modules define the symbol, pass `--module PATH` to select one.

Regenerate version 3 typed packages to obtain the standalone adapter. Generated
record declarations support layout and value inspection. The current adapter
generator supports namespace-scope records, enums, polymorphic bases, concrete
class template specializations, and supported standard library types.
Preparation checks virtual base layout and rejects types whose ABI it cannot
reproduce.

Use explicit template arguments and the concrete parameter list to prepare
template functions and methods, for example `twice<int>(int)`,
`Handler::convert<double>(double) const`, or `Box<int, 5>::apply(int) const`.
The preparer resolves the concrete declaration without building the target.
Injection requires the instance to have an executable symbol and a safe patch
window in the target.

The concrete demo prepares `Handler::scale(int)`, `Handler::shift<int>(int)`,
`Handler::Nested::adjust(int)`, and `Handler::Nested::adjust<int>(int)` with the
`prepared_concrete::` qualifier. The `native_repl_prepared_concrete` test
injects all four static methods after removing the application header and
checks that they return 12, 6, 13, and 14 for input 4. For nested static methods,
the package preserves the original symbol and callable signature. Argument
and result types follow the same adapter rules as other prepared probes.

For records with application-defined copy, move, or destruction code, the
standalone adapter preserves the lifetime declarations and lets the target
perform those operations. Reference and by-value snapshots can capture fields
from those records. For a raw pointer, the callback decides when to read the
pointee and can publish an owned field payload. Use
`--compile-option "-fno-access-control"` on injection if the callback itself
names protected or private fields. Automatic snapshots already include those
fields. If the adapter cannot reproduce a pointee layout, the probe can still
capture the pointer address.

To run the same workflow automatically through the native REPL:

```bash
./examples/prepared-injectors/run-native.sh --smoke
```

Expect `NATIVE_EVENT_COUNT=4`, `NATIVE_RESULTS=1`, and `detached session=...`.
This variant restores its hook by detaching through `tracer.detach(session)`.

Set `CLANG_TRACER_PREPARED_BUILD` to use a different CMake build directory.

## Automatic snapshots for returned pointers

Build and run the Linux x86-64 pointee example:

```bash
cmake --build build --target clang-tracer-repl clang-tracer-server clang_tracer_agent prepared-pointee-target
./examples/prepared-injectors/run-native.sh --pointee
```

At the native prompt, prepare the declared return type and inject with the
opt-in pointee snapshot flag:

```text
%probe prepare "prepared_pointee::HfwManager::createItem(int)" --include demo/pointee/item.hpp --output prepared
%probe inject "prepared_pointee::HfwManager::createItem(int)" --prepared prepared --prepared-mode require --snapshot-return-pointee
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
auto event = captured.at(captured.at(0).result() == "0x0" ? 1 : 0);
auto item = event.result_as<prepared_pointee::HfwItem>();
auto snapshot = event.result_snapshot();
std::cout << item.label << " " << snapshot.field("details").field("revision").as<int>() << std::endl;
%quit
```

The target alternates between returning an `HfwItem*` and `nullptr`.
For non-null returns, `result_snapshot()` includes `id`, `label`, and nested
`details` fields. The prepared package imports the mapping automatically, so
`result_as<prepared_pointee::HfwItem>()` returns an owned copy.
`receiver_snapshot()` contains the manager's fields. Null returns display
`0x0`. Run the same capture as a smoke test with this command.

```bash
./examples/prepared-injectors/run-native.sh --pointee --smoke
```

The corresponding CTest is `native_repl_prepared_pointee`.

### Polymorphic pointer returns

Build `prepared-polymorphic-pointee-target` and start the example.

```bash
./examples/prepared-injectors/run-native.sh --polymorphic-pointee
```

The launcher prints the commands in
[`inspect.repl`](polymorphic-pointee/inspect.repl). The target returns a
`Base*` to a `Child` whose base subobject sits at a nonzero offset. Source
preparation discovers `Child` and captures `Child::x` as `42` without a
type-specific command option.
The example also reads `x` through `result_as<ChildFields>()` with a local
decoder. Add `--smoke` to check the child, base and null cases.

### PImpl return snapshots

Build the C++17 PImpl fixture and run either return form in its own native REPL
session:

```bash
cmake --build build --target clang-tracer-repl clang-tracer-server clang_tracer_agent prepared-pimpl-target
./examples/prepared-injectors/run-native.sh --pimpl-pointer
./examples/prepared-injectors/run-native.sh --pimpl-reference
```

The public [`Item` declaration](pimpl/item.hpp) keeps its pointer fields private
and forward-declares `ItemImp`. The internal header defines `ItemV1`, `ItemV2`,
and `ItemV3`. The pointer and reference demos prepare from the two application
headers without a pointer policy; source discovery follows the polymorphic
`_imp` and `_other` members and captures each concrete implementation. The
complete non-polymorphic `_stamp` record remains address-only without an
explicit policy. The pointer probe uses the existing
`--snapshot-return-pointee` opt-in, while the reference probe captures its
`Item` directly.

Both sessions inspect private and nested fields for all three known
implementations and use `result_as<LocalFields>()` to read an owned typed value.
The fourth implementation is defined only in `item.cpp`, so the snapshot keeps
its `_imp` address. An `_unreadable` member points to a `PROT_NONE` page and the
guarded lookup leaves its address in the snapshot. Both sessions check null
members. Pointer mode also checks a null outer result. The `--pimpl-custom`
and `--pimpl-roles` examples use the explicit
[`FollowPointers` policy](pimpl/snapshot-policy.hpp) to cover `_stamp`,
arguments, and receivers.

Run the automated cases with:

```bash
./examples/prepared-injectors/run-native.sh --pimpl-pointer --smoke
./examples/prepared-injectors/run-native.sh --pimpl-reference --smoke
./examples/prepared-injectors/run-native.sh --pimpl-roles --smoke
```

CTest registers these as `native_repl_prepared_pimpl_pointer` and
`native_repl_prepared_pimpl_reference`; `native_repl_prepared_pimpl_roles`
checks that the same policy also captures arguments and receivers. Separate
sessions avoid re-importing the same application declarations into one REPL
process.

The `--pimpl-custom` mode demonstrates a user encoder for ItemV2. It replaces
the automatic ItemV2 fields with a single `summary` projection while preserving
the `HFW::ItemV2` result type; ItemV1 and ItemV3 remain automatic. Its callback
has no PRE or POST hook:

```bash
./examples/prepared-injectors/run-native.sh --pimpl-custom
./examples/prepared-injectors/run-native.sh --pimpl-custom --smoke
```

CTest registers this as `native_repl_prepared_pimpl_custom`.

## Typed method example

Build `prepared-methods-target` and launch with `--methods`. The method demo
target has no DWARF. The launcher prints the commands and full selector from
`methods/try.repl` and starts the native C++ REPL.

The method declaration and fields are in `methods/account.hpp`. The callback
in `methods/interceptor.cpp` specializes `Interceptor<Signature>` with the
method’s concrete argument and result types. Preparation and injection use the
same selector. The REPL finds the package registered by `%probe prepare`.

The event accessors return owned snapshots of supported arguments, results,
and receiver fields. `e.receiver_snapshot()` captures the declaring class
and its bases at method entry. PRE and POST events for the same call share that
entry snapshot. Fields at every access level use their short names when unique.
For hidden fields, use the qualified name, such as
`e.receiver_snapshot().field("prepared_demo::Audit::account_id").as<int>()`.

The method example exercises strings and string views, a vector by value,
optional values, a nested user record, a pair, a tuple, an array, a signed
bit-field, and fields inherited through private and protected bases. It also
passes owning and weak smart pointers and a variant as typed method arguments.
The inherited fields exercise a non-null and a null unique pointer, a shared
pointer, live and expired weak pointers, and variants holding duplicate types
and `std::monostate`.

An owning smart pointer snapshot exposes `address` and an owned copy under
`value` when it points to a supported object. A null pointer has address zero.
Array, opaque, and non-owning aliasing pointers are captured by address.
A weak pointer records `expired` without acquiring ownership. A variant records
its `index` and the matching `alternative_N`. For decodable alternatives,
`argument_as<std::variant<...>>()` reconstructs an owned variant. A valueless
variant retains its `std::variant_npos` index in the snapshot. Pointer values and
variants use the same bounded capture budget as the other fields.

Variadic and virtual methods, constructors and destructors, ambiguous repeated
non-virtual base subobjects, unions, and private reference members are outside
this example's automatic capture support. Unique pointers with custom deleters
and recursive pointee graphs also report unavailable snapshots.

Run `./examples/prepared-injectors/run-native.sh --methods --smoke` to check
the prepared binding and the field values through the native REPL.

## Typed custom event

Launch the example with `--state`. The launcher runs
`methods/setup-state.repl` in the native REPL to prepare the package, install
the probe, collect an event, and bind `reading`. Use the inspection commands
in `methods/try-state.repl` at the prompt. The example prepares the method
with `--metadata-only` and requires the package during injection.
Omit `--metadata-only` to keep the separate generated C++ adapter files
alongside the metadata.

The interceptor saves a copy of three typed arguments in a scoped
`prepared_demo::Reading` and publishes `account.adjusted` on return.
The public payload definition is in `methods/reading.hpp`. Additional
`--include` inputs can supply callback payload types. After injection, the
same REPL imports mappings from its generated adapter.

The `--state --smoke` run deletes the staged application headers and compilation
database after preparation. It requires prepared validation and injection and
checks the decoded `Reading` in the native REPL.

```text
%collect --name account.adjusted --count 1 --timeout 15 --as adjusted
auto event = tracer.batch("adjusted").at(0);
auto reading = event.state_as<prepared_demo::Reading>();
reading.parts.at(1)
reading.adjustment.delta
```

The label is `deposit`, the vector is `{2, 5}`, and the nested adjustment
has delta `4` and limit `30`. Use `event.state_snapshot()` for field access
without a C++ mapping. Both forms read owned capture-time data. A truncated
state reports an error.

# Native C++ REPL

For runnable examples of structs/classes, strings, optionals, sequence
containers, maps, sets and nested containers, see
[`examples/native-repl-values`](../examples/native-repl-values/README.md).

`clang-tracer-repl` manages a tracing session from one prompt. Commands start
an owned local server, attach to a process, upload C++ interceptor source, and
collect and inspect events. Ordinary C++ input runs locally through Clang
Interpreter. Python is not required to run the console.

## Build

The console, controller, generated RPC bindings and command parser are built
with the project C++23 toolchain. Build the client alone on Linux or macOS:

```sh
cmake -S . -B build-repl -G Ninja -DTRACER_REPL_ONLY=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm \
  -DClang_DIR=/path/to/llvm/lib/cmake/clang
cmake --build build-repl -j2
ctest --test-dir build-repl --output-on-failure
```

Supply a matching LLVM/Clang 21 or 22 development installation, the gRPC and
Protobuf development packages and compiler plugin, and the SQLite development
package. The implementation has separate
embedding paths for LLVM 21.1.8 and 22.1.8. Set `CMAKE_PREFIX_PATH` for dependencies
outside the system search path. The existing full Linux build includes the REPL
by default; `-DTRACER_BUILD_REPL=OFF` keeps the prior backend-only build.

The two new dependency downloads have exact archive checksums:

| Package | Version | SHA256 |
| --- | --- | --- |
| replxx | 0.0.4, tag `release-0.0.4` | `a22988b2184e1d256e2d111b5749e16ffb1accbf757c7b248226d73c426844c4` |
| ANTLR C++ runtime | 4.13.2 | `0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4` |

Use `FETCHCONTENT_SOURCE_DIR_TRACER_REPLXX` and
`FETCHCONTENT_SOURCE_DIR_TRACER_ANTLR` with pre-extracted versions for offline
builds. Their licenses are installed with the console. gRPC and Protobuf use
the build's existing package selection; the REPL does not impose new versions.

Stage an installation without changing the live installation:

```sh
cmake --install build-repl --prefix "$PWD/build-repl/stage" --component Runtime
```

The runtime includes the facade header under `share/clang-tracer/repl/api.hpp`.
Clang/LLVM libraries, builtin headers and a compatible C++ SDK must remain
available. `--cpp-arg ARG` adds an interpreter compiler argument; repeat it for
include directories or other options. The prelude uses the build compiler's
standard-library include roots. Installing the client alone does not install a
Linux server or agent.


## Prepare a probe from a binary without headers

Build a module index from a Linux x86-64 ELF executable or shared library.
Embedded DWARF and matching separate debug files provide type and signature
information. ELF exports remain searchable in stripped modules.

```text
%module index --file ./libdemo.so
%module types --filter "demo::*"
%module symbols --exported --filter "demo::*"
%module show --type demo::Packet
%module show --symbol "demo::score(demo::Packet const&)"
%module emit --symbol "demo::score(demo::Packet const&)" --output recovered
%probe prepare "demo::score(demo::Packet const&)" --generated recovered --output prepared-score
```

The index defaults to `libdemo.so.tracer.sqlite`. Use `--output INDEX` to name
it or `--debug-file FILE` to supply a matching companion. To index a live module
using the REPL's local server, run `%modules --details` and pass its exact
module path and token to `%module index --module PATH --mapping TOKEN`.

Indexing selects the new index for the session. In another REPL,
`%module use INDEX` selects a saved index. With no headers or explicit
signature, preparation checks the explicit `--module-index` first, then
the `<ELF>.tracer.sqlite` sidecar when you pass `--module ELF`, then the
selected index, then `module-index.sqlite` in the command's working directory.
When `--module` is omitted, it uses the ELF path recorded in the index.

List commands accept glob filters, `--format json`, and optional
`--limit N` and `--page TOKEN`. Use the full C++ signature to select an
overload, or `--id ID` when a name is ambiguous. The index preserves missing
type and layout information. Symbols with incomplete signatures can still be
looked up. Supported indexed signatures use scalar, pointer and borrowed
reference parameters and results. The generated `views.hpp` contains read-only
accessors for supported fixed-offset fields, and the manifest lists omitted
fields. `--generated` is optional. The preparer can regenerate the checked C++
from the index.

The prepared package embeds its declarations and accessors. A callback can
use them directly. This example reads a scalar field from the borrowed argument.

```cpp
template <> class Interceptor<int(const demo::Packet&)> {
public:
  long long pre(const Call& call) {
    return tracer_recovered::demo::PacketView::value(call.argument<0>());
  }
  long long post(const Call& call) { return call.result<int>(); }
};
```

In the attached REPL, validate and inject the package.

```text
%probe validate "demo::score(demo::Packet const&)" --callback observer.cpp --prepared prepared-score --prepared-mode require
%probe inject "demo::score(demo::Packet const&)" --callback observer.cpp --prepared prepared-score --prepared-mode require
```

The native C++ API uses the same index and preparation workflow.

```cpp
auto index = tracer.open_module_index("libdemo.so.tracer.sqlite");
auto score = index.find_symbol("demo::score(demo::Packet const&)");
auto packet = index.find_type("demo::Packet");
index.prepare_probe(score.id, {}, "prepared-score");
```

Standalone C++ callers can include `<tracer/module_index.hpp>` and link
`tracer_module_index`. Use `tracer_module::ModuleIndex::build` with
`tracer_module::ModuleSource::file(elf_path)` to build an index or
`ModuleIndex::open` to reuse one.

## Start and attach

On Linux x86-64 with the native server and agent installed:

```sh
clang-tracer-repl --attach 12345
# Optional explicit build-tree server:
clang-tracer-repl --server /path/to/build/clang-tracer-server --attach 12345
```

The console creates a private endpoint, starts its server in a separate process
session, waits for readiness, and attaches before returning to the prompt.
Agent discovery is performed by the server; `--agent PATH` is an override.
Server lookup checks an adjacent executable and then `PATH`.

For an existing server:

```sh
clang-tracer-repl --endpoint 127.0.0.1:50051 --attach 12345
```

An explicit endpoint, including `CLANG_TRACER_ENDPOINT`, connects to that service
and never silently starts a replacement. The PID belongs to the server host.
The existing tracing service's permissions and target/ABI restrictions apply.
The macOS client connects to a Linux server; it cannot start a Linux backend
locally.

With an owned local Linux server, `--process NAME` matches executable paths.
`%attach` without a PID opens the process picker; multiple matches require
choosing a displayed PID. The picker shows each candidate's executable and Linux
start time and checks the selected identity at attach. `%processes --filter TEXT`
lists the same local candidates. In scripts, use `%attach PID --start-time TICKS`
to attach only when the PID still has the displayed start time.
Remote process enumeration is not in the service API, so remote attach uses an
explicit PID.

If an attach reply fails or times out, the operation may still finish on the
server. The console reports the endpoint, PID and newly observed candidate
session IDs, blocks automatic retries for that PID, and reconciles again on
`%status` and exit. Inspect `%sessions` and explicitly `%detach SESSION` when
appropriate. Candidates are never assumed to belong to this console on an
external server; recovery information is retained until explicitly resolved.

## Browse a local C++ target

```text
%threads
%modules --details
%symbols --filter Calculator --addresses
%symbols --address 0xADDRESS --mapping TOKEN
```

`%threads` shows thread IDs, names and start times for the selected process.
`%modules` lists application module paths. Add `--details` for file-backed
executable address ranges, file offsets, device and inode, and the ELF build
ID when the backing file can be verified.

`%symbols --addresses` shows runtime and ELF entries for supported C++ callables
when there is a unique verified mapping. For reverse lookup, copy a runtime
address and the `mapping=` token from the same session. Addresses inside a
supported callable also work. If the mappings or backing files change, take a
new snapshot before using an address. The token checks observed mapping
identity. An unload and reload of the same image at the same address between
snapshots can produce identical observations.

## Preview a bulk trace

```text
%trace preview --module 'libapp*' --include 'app::*' --exclude 'app::detail::*' --save trace.textproto
%trace preview --exact "Calculator::multiply(int, int) const"
%trace preview --manifest trace.textproto
```

Run the preview after attaching to a local Linux x86-64 C++ application.
Repeat `--module` to match any executable module by basename or path.
`--include` and `--exact` combine their selections, then `--exclude` removes
matches. Globs are case-sensitive and support `*`, `?` and bracket patterns.
They match either qualified names or full signatures. Use `--exact` to choose
an overload by its full C++ signature. Quote patterns at the REPL.

The dry run reads executable mappings, ELF symbols and DWARF, including matching
separate debug files. It reports a live entry when one can be verified.
`DWARF_SUPPORTED` means the entry passed static ABI checks. Callback compilation
and installation checks run separately. `PREPARED_REQUIRED` identifies a
concrete virtual implementation that needs a compatible prepared package.
Unsupported entries and ambiguous selections show a reason. Narrow a
duplicate signature with `--module`.

The capacity line shows retained slots and whether the DWARF-supported
selection fits. It is a snapshot and reserves no slots. The preview runs
without installing probes, stopping target threads or executing code inside
the target. `--save` creates a new versioned text-protobuf file with selection
rules. Reusing it resolves against the current session and mappings.

## Apply and remove a bulk trace

```text
%trace apply --manifest trace.textproto
%trace apply --manifest trace.textproto --callback observer.cpp
%trace remove BATCH_ID
```

Apply re-resolves the manifest against the selected session. The default
Clang JIT observer records typed PRE and POST events. With `--callback`, the
same C++ source is compiled for each matched function. Every match must be
DWARF-supported and fit in the available retained slots. Ambiguous or
unmatched selections, entries requiring prepared packages, and entries with
active probes block installation.

The REPL prints the batch ID before applying and reports the status of each
entry. If an install fails, the server restores this batch's probes in
reverse order. Use the same batch ID with `%trace remove` to retry entries
reported as `RESTORE_PENDING` or to check a batch after an interrupted
command. Removal is repeatable for a retained batch in its original session.
Removal and rollback restore entry bytes. The agent still retains compiled
slots until the target exits.

## Read and scan mapped memory

```text
%memory maps
%memory read 0xADDRESS --mapping TOKEN --count 32
%memory read 0xADDRESS --mapping TOKEN --type i64
%memory scan 0xADDRESS --mapping TOKEN --count 4096 --pattern "48 8? ?? ff" --limit 16
```

Run these commands after attaching to a local Linux x86-64 target. Copy the
token after `memory mapping=` and use an address from one of the listed
ranges. Ranges include anonymous and file-backed mappings, with permissions
shown as reported by Linux. The memory token is specific to the attached
session and its memory map.

Use `--count` for a hex and ASCII dump of 1 to 65536 bytes. Use `--type`
for one little-endian `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`,
`f32` or `f64` value. Reads can cross page and mapping boundaries when the
entire span is readable. A gap, inaccessible page, or short kernel read
fails the request at the first affected address and returns no bytes.

The server checks the mapping snapshot and target identity around each
read or scan. Run `%memory maps` again after any mapping or permission change.
The target keeps running during inspection, so bytes can change while
they are copied. An unmap and remap with identical visible metadata between
checks can produce the same snapshot.

Scans accept 1 to 1048576 bytes and patterns of 1 to 256 bytes. Separate hex
bytes with spaces. Use `??` for an entire wildcard byte, or `8?` or `?f`
for a wildcard nibble. To choose individual bits, append a colon and a
same-length hex mask, as in `"13 37 : 1f ff"`. A bit participates only
if the explicit mask enables it and the bit belongs to a non-wildcard nibble.

Matches are reported in ascending address order and can overlap or cross
adjacent readable mappings. The default match limit is 64 and the maximum
is 1024. `truncated=true` means the scan found at least one more match.
A scan fails without returning matches if any part of its requested range
is unreadable or the snapshot changes during inspection. The scan uses the
configured operation timeout and supports Ctrl+C.

## Preview a verified function entry

```text
%memory maps
%probe entry "Calculator::multiply(int, int) const" --mapping TOKEN
%probe entry --module /absolute/path/to/app --elf-address 0xELF_VA --abi linux-x86-64-sysv --mapping TOKEN
```

Use the token from the attached session's memory snapshot. A named entry
uses DWARF. The explicit form requires the exact mapped module path and a
link-time ELF virtual address. It accepts an exact function symbol start
or an exact `.eh_frame` range start in a stripped image. The ABI label in
that form is supplied by the caller. The preview verifies executable
mapping identity and compares the live bytes with the ELF file, which
must be a readable regular file of at most 128 MiB.

The preview disassembles up to 64 verified entry bytes and follows direct
branches within that window. Call targets are reported without following
them. Indirect targets are shown as unresolved. `patch_size` shows the
instruction-aligned window accepted by the current copy-only detour policy.
If the policy rejects the entry, the preview shows the reason. It performs
no installation or writes.

## Probe a short getter

`%probe inject` uses the `auto` backend by default. It keeps the verified
entry jump when the overwrite window and relocation fit. The installed
probe reports its backend, selection reason, and patch address.

```text
%probe inject "demo::Circle::get_radius() const" --backend auto --caller "demo::call_radius(const demo::Circle &)" --stack-depth 6
%record getter.db --sqlite --phase POST --probe PROBE_ID --count 4 --timeout 30
%detach
```

For a supported short integer getter, `--caller` selects a named function
in the same module. The call-site backend requires one verified five-byte
direct call to the getter and an executable relay within rel32 range of the
next instruction. It traces calls through that site. Use `--backend entry`
to require an entry jump.

When auto has no verified caller or no reachable relay, the trap backend
copies the complete getter and installs one `INT3` at its verified entry.
The server redirects owned traps to the normal wrapper, leaving the
application's return address on its stack. The initial supported body is
a 32-bit load from the receiver followed by `ret`. An existing ENDBR64
stays in place and selects the trap path. The trap owns execution control for its session and
restores its byte at stop, detach, or a thread or process creation stop.
Remove a fallback probe before replacing it.

## Trace object lifetimes and state in a prebuilt binary

Use the full Linux build on x86-64 or AArch64, with the REPL, server, and agent from the same build. Supply the existing ELF and its matching DWARF, either embedded or in a matching separate debug file. The watch installs patches in process memory. The application executable and source files stay intact.

The plan records the capture mode you select during preparation.

- **Individual writes.** Select one `--field` and a writer with `--code`. Use `--backend code-cache` to run supported stores in the target without a ptrace stop for every write. `--backend ptrace` is the default and steps verified stores for diagnostic runs. By default, the watch samples values and reports changes. Start a saved write plan with `--no-values` to record `FIELD_WRITTEN` for each completed admitted store, including same-value stores, without field samples. The backend is part of the saved plan.
- **Method snapshots.** Use `--on` with the original header and compile command to compare the receiver at verified entry and return boundaries. A snapshot delta describes the net change during one call. Supply the synchronization policy that protects the sampled state. See [method snapshots](#prepare-an-object-watch-for-method-snapshots).

### Run a scalar write watch on x86-64

From the repository root on Linux x86-64, use a full build in `build` with tests enabled. Build the demonstration fixture once and start it in the first terminal. It prints its PID and waits for input.

```sh
cmake --build build --target repl-object-plan-target
sh -c 'printf "PID=%s\n" "$$"; exec "$1"' _ ./build/repl-object-plan-target
```

From the same directory in a second terminal, open the REPL. Use a fresh package directory and SQLite path for each run.

```sh
./build/clang-tracer-repl --server ./build/clang-tracer-server --agent ./build/libclang_tracer_agent.so --fail-fast
```

Prepare the plan before attaching. Then replace `PID` with the printed PID and check that preview reports `binding=same-build` and a verified store site. Start installs the redirects and prints the watch ID.

```text
%object prepare demo::PlanRecord --module ./build/repl-object-plan-target --field value --code "demo::write_value(demo::PlanRecord &)" --backend code-cache --output ./record-watch
%attach PID
%object preview --plan ./record-watch
%object start --plan ./record-watch --stack-depth 4
%record object-writes.db --sqlite --name object.trace --probe WATCH_ID --count 2 --timeout 30
```

Replace `WATCH_ID` with the ID printed by start. While `%record` is waiting, enter `g` in the first terminal. The fixture prints `value=23`. The recorder should report `received=2` and `complete=true` for the creation and the write from 17 to 23.

```text
%object status WATCH_ID
%object stop WATCH_ID
%detach
%events open object-writes.db --as saved
%events list saved
%events show saved --index 1
%events stack saved --index 1
```

Stop restores the verified instruction bytes. Check `active=no` and `restoration_pending=no` before detaching. If restoration is pending, retry stop using the same ID. Replay works after detach, including in a fresh REPL. Enter `q` in the first terminal to exit the fixture. To repeat with `--no-values`, restart the fixture, reuse the write plan, add the flag to `%object start`, and use a fresh recording path. The two events are still creation and a store, with the store shown as `FIELD_WRITTEN`.

### Read coverage and cleanup status

- Review the resolved fields, writer sites, and binding status in preview. Preview is read-only. A same-build restart resolves fresh live addresses. A compatible rebuild requires verifiable field and code bindings. Missing or mismatched evidence can prevent a watch from starting.
- Lifecycle events require observable, unambiguous construction and teardown. Optimized-away boundaries, unverified tail entries, and some ownership mutations can leave partial timelines. `--first-seen` can scope a supported standalone write watch to one verified writer invocation when construction is unavailable.
- Collection and pointee watches require verified mutation boundaries and supported identities. Unverified inline paths can leave coverage gaps. Tagged-union decoding requires a verified unsigned 32-bit discriminator and supported 32-bit alternatives. Untagged union observations carry storage bytes.
- Native capture covers admitted code and threads. Generated helper windows can leave signal or stack gaps, including AArch64 scalable signal records that exceed the fixed Linux signal frame. Shared-file identity correlates recordings across processes, where ordering can remain partial.
- Use `%object status` to check drops, unsupported sites, coverage gaps, and capture limits. Use `%object stop` or `%detach` to retire the watch and restore patches. See the [verification record](../object-watch-progress.md) for the exercised cases and limits.

## Prepare an object watch for method snapshots

Prepare from the original headers and compile command. The package contains the generated adapter and the verified ELF entry. Declare the synchronization that protects the receiver and any storage the adapter follows. Use `thread-confined` when that graph is accessed from a single thread. Use `caller-lock` when the caller holds the application's lock over the method and both samples. Use `stable-at-boundaries` when the application establishes safe entry and return samples, for example by joining a child writer before return. For the Portfolio commands below, run the rebalance while `%record` is waiting and substitute the PID and watch ID from your run.

```text
%object prepare demo::Portfolio --module ./portfolio --include ./portfolio.hpp --policy ./snapshot-policy.hpp --compdb ./compile_commands.json --translation-unit ./portfolio.cpp --on "demo::Portfolio::rebalance()" --field legs --field risk --key legs=id --key risk=key --synchronization thread-confined --output ./portfolio-watch --emit-source ./generated
%attach PID
%object preview --plan ./portfolio-watch
%object start --plan ./portfolio-watch --stack-depth 8
%record portfolio.db --sqlite --name object.trace --probe WATCH_ID --count 2 --timeout 30
%object stop WATCH_ID
%detach
%events open portfolio.db --as saved
```

Snapshot deltas describe the interval between a method's entry and return. Fields select the reported paths. The generated receiver codec reads the receiver and the pointees authorized by the preparation policy. A failed sample yields a coverage gap. A missing return yields a coverage gap when the probe retires. Set `--max-elements`, `--max-string-bytes`, `--max-depth`, `--max-snapshot-bytes`, and `--max-instances` to bound capture and observation.

A compatible rebuilt ELF can reuse a prepared method watch when DWARF confirms the receiver layout and the original void, scalar, or pointer signature. Pass `--module` to preview and start if the rebuilt image has a new name.

On AArch64 the native agent samples the receiver at a verified entry and at each verified return instruction. The original method executes in its application mapping, and CFI stack capture starts at its original entry. An exception that leaves the method can produce a missing-return gap when the watch retires.

An `AnyPayloads<Record>` specialization can register named record payloads for a direct `std::any` field. Name the field with a type alias to a tuple of the admitted types. The typed projection appears as `extension_snapshot` for the example below.

```cpp
namespace tracer_snapshot {
template <class Record> struct AnyPayloads;
template <> struct AnyPayloads<demo::Portfolio> {
  using extension = std::tuple<demo::RiskState>;
};
}
```

## Follow logical collection entries during writes

```text
%object start --type demo::Portfolio --field quantity --vector legs --element-key id=12 --code "demo::Portfolio::rebalance()" --mutation _ZNSt6vectorIN4demo3LegESaIS1_EE7reserveEm
```

The watch validates the writer's first receiver argument and the selected libstdc++ vector layout from DWARF. It binds a unique element by its 32-bit unsigned key and suspends that address during the selected mutation. A verified return can admit a new storage generation. Add `--backend code-cache --synchronization thread-confined` to translate the admitted fragments of a nonleaf writer and its selected mutation. On either backend, use `--first-seen` for a standalone scalar receiver whose constructor was inlined, absent, or ran before attaching. The provisional identity lasts for one writer invocation.

Repeat `--mutation` when the writer uses more than one storage-changing function. Each selected function must accept the verified container as its first argument. Native capture verifies direct calls and their return continuations. For an inlined function, the watch verifies the receiver and the control-flow edges into and out of its matching DWARF ranges. An inline instance can span disjoint ranges. With declared synchronization and value capture, `RANGE_BOUND` includes the verified value at binding. A rebind includes the prior value when it was sampled safely before suspension.

Use `--map risk --map-key AAPL --field exposure.limit` to follow a nested field in a `std::map<std::string, T>` entry. Select a mutation that takes the map as its first argument. The watch verifies the libstdc++ tree and key layout from DWARF. Each verified return binds a new map generation, including when an allocator reuses an address. An absent key or a traversal that exceeds its budget yields a coverage gap. Map and vector writes require a signed 32-bit value field.

## Trace a Shape through Image

Build the fixture once with a Linux x86-64 GCC 8.5 compiler and matching
embedded DWARF. Run it in one terminal and leave it at `READY`.

```sh
"$GXX85" -std=c++17 -O2 -g -fno-omit-frame-pointer -fcf-protection=none \
  -fPIE -pie -pthread -Wl,--build-id=sha1 tests/fixtures/repl_object_watch.cpp -o shapes
./shapes
```

Attach from the native REPL and start the watch. Enter `g` in the target
terminal while the REPL is recording. The fixture builds, mutates, and
removes two Circles in the same allocation slot.

```text
%attach PID
%object start --type demo::Circle --base demo::Shape --owner demo::Image --field radius --collection _shapes --snapshot-field _description --code borrowed_write --stack-depth 5
%record shapes.db --sqlite --name object.trace --count 15 --timeout 30
%object stop
%detach
%events open shapes.db --as shapes
%events list shapes --limit 15
%events stack shapes --index 4
```

The diagnostic backend admits existing target threads and one writer function
in the main executable. It uses ELF linkage for the complete constructor and
destructor and the `add`, `get`, `remove`, and `set_description` boundaries.
It verifies the mapped build ID, live instruction bytes, DWARF layouts and
RTTI. The supported layout uses a verified fixed base offset, an `int32`
field, the libstdc++ C++11 string ABI and a vector of default-sized unique
pointers. The remove boundary checks the watched entries against the verified
membership after return.

The server owns the breakpoints until stop or detach. It parks peers while
the writer steps each admitted instruction. The effective destination must
equal the selected field of a live generation. An interrupted store emits a
coverage gap. A successful retry can produce a fresh delta at the same PC.
Enter `f` instead of `g` and record 16 events to exercise that path.
The string delta comes from the verified setter boundary. Stack depth defaults
to zero. Enabled write stacks walk available frame pointers in the main executable.
Use `--max-elements` and `--max-string-bytes` to set snapshot limits.

Add `--no-values` to record `FIELD_WRITTEN` for every completed store,
including a same-value store. This skips pre/post value reads and omits
value samples from lifecycle and fault records. Status shows `values=off`.
The default captures value changes and verified boundary snapshots.

Coverage is partial. Stores outside the selected function, unsupported
instructions and optimized-away lifetime boundaries are outside this draft.
New threads in the same process can enter the diagnostic watch. An exec or
fork boundary that changes the observed process reports partial coverage.
Use `%object status` for counts and restoration state. Unsupported sites,
reported gaps, or dropped observations mark coverage partial. If restoration
needs a retry, the watch keeps its ID and the target remains stopped.
Use `--tid` to select the thread whose stores are reported. The diagnostic backend restores its patches when `--max-events` or `--timeout` is reached. Native code-cache and method watches cap observations at those limits and keep their patches until stop or detach.
Set `--event-buffer` to size the native code-cache event queue. The default is 4096 observations. When the queue fills, `--overflow drop` counts and drops the new observation. `--overflow stop` ends capture at the first overflow and reports that reason. Application stores continue in either mode.

For a standalone scalar with all its writers covered by the code-cache watch, set `--serialize-wait-us N` to serialize the observed stores and event publication for each field address. The wait is bounded by the supplied number of microseconds. If writers overlap after the wait or tracing is interrupted by re-entry, the event reports unavailable ordering and withholds the old value. A mismatch between consecutive value samples reports an intervening change outside the observed store sequence. The default uses concurrent capture.

Use `--shared-file PATH` on each process's native `std::atomic<int>` watch to correlate writes to a writable shared file mapping. A scalar watch can also use this option when the application protects the field with a process-shared lock and the watch declares `--synchronization caller-lock`. The server holds the file open, and the agent checks the live device, inode and byte offset. Recordings expose those three values as `sharedStorage` across overlapping watches on the same host. An atomic read-modify-write can report its own previous value. Coverage remains partial because other writers and their ordering are unknown. Both capture modes retain the shared storage identity.

Use `--pointee "routes.head->node" --field state.quantity` to follow a pointer path within a verified writer invocation. Dots select embedded members and arrows follow raw data pointers, verified libstdc++ default-deleter `std::unique_ptr`, and verified libstdc++ `std::shared_ptr`. Supply `--mutation` for the owner operation that changes the path and declare `thread-confined` or `caller-lock` synchronization. Each successful rebind starts a storage generation, even when the final address repeats. A null or unreadable path suspends write attribution and produces a coverage gap.

## Trace a direct integer assignment

To find direct writes in source during preparation, pass `--discover-writers`, `--compdb`, and one or more `--translation-unit` arguments. The command saves source candidates with the ELF plan and can select their verified writer functions. Repeat `--code` to select additional verified writers, including writers that reach the field through aliases.

Build the `tests/fixtures/person` sources as separate translation units with
`-O2 -g` and LTO disabled. Run the prebuilt fixture until it prints `READY`.
The standalone watch binds a signed 32-bit field using the record layout
from the writer's DWARF and a verified constructor return.

```text
%attach PID
%memory maps
%probe entry "demo::update(demo::Person &)" --mapping TOKEN
%memory read PC --mapping TOKEN --count SIZE
%object start --type demo::Person --field age --code "demo::update(demo::Person &)" --stack-depth 4
%memory maps
%memory read PC --mapping PATCHED_TOKEN --count SIZE
%record age.db --sqlite --name object.trace --count 2 --timeout 30
%detach
%attach PID
%memory maps
%memory read PC --mapping RESTORED_TOKEN --count SIZE
%events open age.db --as ages
%events show ages --index 1
%detach
```

Use the address and instruction size of the store shown by `%probe entry`.
Enter `g` in the fixture terminal while recording to construct the Person
with age 42 and assign 1 twice. The watch records the original writer PC
and DWARF source line. With `--no-values`, record three events to include
the constructor and both writes. After verifying the restored instruction,
enter `r` to read the live value. After the last detach, enter `v` to run
the assignment again, then `q` to exit.

## Trace atomic integer updates

For a DWARF-verified libstdc++ `std::atomic<int>` field, the watch admits
32-bit `mov`, `lock xadd`, and `lock cmpxchg` in the selected function.
With values enabled, a store takes its new value from the source operand
and displays `old=unavailable`. Fetch-add uses the returned old value and
the input increment. Compare-and-exchange emits a write only when the
post-instruction zero flag is set, including in `--no-values` mode.
The diagnostic backend steps the complete instruction, including LOCK.
At detach it restores the full original bytes and rewinds any owned trap
that has yet to execute.

## Decode a tagged union write

Pass `--union-tag FIELD` and a `--union-case TAG=MEMBER` for each admitted
alternative. Use a direct member path such as `--field payload.integer`
to identify the four-byte storage, and declare `--synchronization thread-confined`
or `caller-lock` to protect the tag and storage during both samples.

```text
%object prepare tagged_test::State --module ./fixture --field payload.integer --code _ZN11tagged_test10write_bitsERNS_5StateEj --union-tag kind --union-case 1=integer --union-case 2=real --synchronization thread-confined --backend code-cache --output watch
```

The plan checks the tag offset and each alternative against DWARF. A tag
can be an unsigned 32-bit field or an enum with that underlying type.
Signed integers, unsigned integers, and floats are supported as 32-bit
alternatives. Unknown or unreadable tags retain raw storage bytes.
`--no-values` skips both the discriminator and storage reads.

## Trace writes in the native code cache

```text
%object start --backend code-cache --type demo::Person --field age --code person_write_many
%object status
%record age.db --sqlite --name object.trace --count 4000 --timeout 30
%detach
```

`ptrace` is the default. Select `code-cache` to translate verified leaf
functions or independently verified writer fragments into executable
memory in the target. Standalone signed 32-bit fields and `std::atomic<int>` use
complete constructor and destructor boundaries in the main executable.
On x86-64, repeat `--code` with `--writer-entry` to cover calls through
function pointers to multiple verified leaf writers.
For a shared writer, `--module` takes a unique mapped ELF basename or
absolute path. A shared leaf that uses an entry redirect needs a verified
nonfaulting overwrite window.
Inlined lifetimes, forked children, and unadmitted store instructions are
outside coverage.

Application threads run the original store encoding once in the translated
function. The agent reads the effective address and operands around that
instruction and publishes to a shared event queue. `--no-values` skips
value sampling and still checks the outcome of compare-and-exchange.
Use `--writer-entry` to admit direct and indirect calls into the selected
standalone leaf. Its entry must have a verified overwrite window.
`%object status` reports each redirect, the number of completed translated
store instructions, and event drops. The store count includes failed
compare-and-exchanges and stores to untracked addresses.

Events use the original application PC. For a synchronous SIGSEGV or
SIGBUS on a copied instruction, a chained application handler sees that PC
and can retry or skip the instruction. Keep fault signal dispositions
stable during tracing. Use `--async-signal N` for a signal with an existing
application handler. At a copied application instruction the handler sees
the original PC and can resume or select another original instruction.
The x86-64 and AArch64 hooks defer these signals during supported capture
callbacks and runtime call resolution when system calls are unfiltered.
The handler sees the original callee and caller return address after
ordinary and tail call resolution. For AArch64 scalable state that fits
the fixed Linux signal frame, the agent restores the spilled SVE or SME
records before invoking the handler. Delivery in an uncovered helper
window records a coverage gap.
Optional stacks use
available application frame pointers. The hooks use application stack
space to save registers and extended processor state.
`--stack-cfi` with `--stack-depth N` unwinds from the saved application
registers and original store PC. The emitted stack reports whether CFI
reached the root, hit the depth limit, or stopped at a missing rule.
Unwinding directly through generated helper code remains unavailable.

Stop or detach restores the complete redirect instructions and drains
queued observations. The agent releases retired translated bodies after
active executions have drained. Stable entry and exit stubs and the
associated signal metadata remain available for delayed execution.
When the process replaces its executable image, status closes the watch
with a mapping-change coverage gap after confirming the old redirects
and translated code are absent.

### Trace scalar writes on Linux AArch64

Use the normal REPL with the AArch64 object server. Offline preparation,
preview, lifecycle and scalar write events, SQLite recording, and replay
use the same commands and event types as on x86-64. The diagnostic backend
uses four-byte breakpoint instructions. The code-cache backend installs
four-byte branches to relocated instructions and releases ptrace control
after arming.

```text
%attach PID
%object start --type arm_test::Record --field value --code arm_write_value --backend code-cache
%record writes.db --sqlite --name object.trace --count 10 --timeout 30
%object stop
```

Live installation selects an application thread at a verified idle
sleep or a userspace rendezvous. For a rendezvous, pass
`--rendezvous SYMBOL` and select an existing main-image function where
the thread can safely enter the loader. Threads created after installation
can execute the redirects. The backend supports 32-bit
STR/STUR, release stores, LSE fetch-add with a returned prior value, and
LSE compare-and-exchange. An admitted writer in a loaded shared library
can be selected with `--module`. CFI capture follows saved link registers,
including signed return addresses. In-process capture preserves SVE
vector, predicate, and first-fault registers. Injection and capture also
preserve SME streaming state, ZA, TPIDR2 and, on SME2, ZT0.

### Follow live callees

```text
%object start --type demo::Record --field value --backend code-cache --code dispatch --follow-calls
```

The selected executable or mapped shared-library scope supplies the call sites. A verified direct
or indirect call enters a native gateway with its original return
address. Verified direct edges can extend the selected scope to nested
functions in the same image that keep executing at their original PCs. A runtime
gateway checks live targets against their ELF bytes and translates
supported leaf instructions and tail calls. For a discovered nonleaf
scope, the controller verifies and installs its call and store redirects
before the first covered execution. Its original call sites keep their
return PCs. Ordinary stores run without controller stops.
Stores match the
watched address and lifetime even when the callee receives it through an
alias. Replayed stores include the mapped ELF identity and the runtime
module generation. A shared library can load after arming and unload
between calls.

Use `--runtime-limit N` to bound nested scope discovery and retained translations. The default is
1,024. When a callee exceeds the limit or its code cannot be verified and
translated, the original call runs and the watch records a coverage gap.
Stopping restores the redirects and closes entry to cached callees. The
agent releases each retired callee mapping after its active executions
have drained. A shared scope selected with `--module` keeps a loader
reference through patch restoration and drain. `%object status` reports
runtime translation and release counts.

### Observe startup before main

`clang-tracer-object-watch launch definition.json ./target [ARGS...]`
starts a selected prebuilt ELF under the diagnostic backend and arms the
watch at the Linux exec stop. The definition is a JSON object watch
request with `concreteType`, `field`, and `codeSymbol`. The command emits
JSON event lines and the `exec-stop` coverage marker to stdout. The
target's output goes to stderr and its stdin reads EOF. SIGINT requests
live detachment and exact patch restoration.

For a native code-cache definition, run
`clang-tracer-object-watch launch --agent ./libclang_tracer_agent.so definition.json ./target [ARGS...]`
with the agent built for the target architecture. This arms at the verified
ELF executable entry after the dynamic loader transfers control. The
`executable-entry` marker identifies the start of coverage, which includes
constructors run by the executable startup code and `main`.

On an eligible glibc target, add `--loader-ready` after the agent path to
arm after relocation and before preinit and dependency constructors.
The launcher verifies the loader mapping and its consistent-state
notification. Set `codeModule` in the JSON definition to select a shared
library already present in the initial loader link map.

Use `--syscall-output --synchronization thread-confined` on a diagnostic
scalar watch to observe `read` and `pread` when the destination is the
exact 32-bit field. A short, overlapping, or unreadable output records a
coverage gap and withholds the values. These events identify the verified
syscall instruction and its mapped image.

### Trace a virtual write through a borrowed reference

```text
%object start --backend code-cache --type demo::Circle --base demo::Shape --owner demo::Image --collection _shapes --field radius --caller borrowed_resize --module libct-shapes-gcc85.so --code "demo::Circle::resize(int)"
%record radius.db --sqlite --name object.trace --count 2 --timeout 30
%object status
%detach
```

The named main-executable caller must call the leaf getter directly, then
reach one register- or memory-indirect near call along a straight-line path
with a stable stack pointer and no intervening stores. The call executes
in place and resolves its target normally. A verified entry jump at the
selected writer preserves the original call frame and any ENDBR64 landing
instruction. Other targets execute through their original entries.

The watch starts before construction. A verified return from the complete
constructor registers a lifetime ID and advances the generation at its
address. The constructor may use relocatable RIP-relative loads or address
computations. A leaf `Image::add` returns the inserted pointer in RAX, and
the agent confirms membership in the DWARF-verified vector of unique
pointers. The getter and writer check that owner, the primary-base RTTI,
and the live lifetime ID. Each operation also carries a `borrow_id`.

The deleting and complete destructor entries retire the lifetime before
their original bodies continue. A deleting destructor that invokes the
complete destructor produces one retirement. If the owner has already
cleared the slot, the same boundary emits `OWNER_REMOVED`. The fixture uses
`unique_ptr::reset` for that ordering and a class allocator that reuses one
aligned slot. Each lifetime has its own ID even when the object address
and name repeat.

The verified lifecycle path covers a concrete type with an unambiguous
complete-constructor entry, including aliases for a final type. It needs
a leaf constructor and add, safe entry windows, and a stable owner during
the borrowed operation. Ambiguous base construction, inlined boundaries,
arbitrary owner mutations, and string snapshots are outside coverage.
Destruction reports its start. The fixture independently checks completed
deallocation and reuse.

The loader pins the selected shared image at its verified load bias for
the lifetime of the retained translation. Patch installation and restoration
verify the live file-backed mappings while the target threads are stopped.
This supports uniquely mapped, loader-managed ELF images in the base
namespace. Manual unmapping, alternate loader namespaces, and indirect
tail jumps at the borrow site are outside coverage.

## Interceptor upload

```text
%server status
%sessions
%modules
%symbols --filter Calculator --limit 20
%probe resolve "Calculator::multiply(int, int) const"
%interceptor load method.cpp --as multiply_hook
%interceptor show multiply_hook
%interceptor validate multiply_hook --target "Calculator::multiply(int, int) const"
%interceptor upload multiply_hook --target "Calculator::multiply(int, int) const" --stack-depth 16
%probes
```

`load` reads the file on the console machine. `upload` sends its source by value
through ResolveProbe/AddProbe and reports success only for an active probe.
The draft remains available after deployment errors. Use
`--stack-depth full --stack-application-only false` to capture through the
thread root and include runtime and JIT frames. A numeric depth caps the
unwind at that many frames before filtering. For large shared events, pass
`--context-event-bytes 25165824 --context-transport-memory-bytes 234881024`
to the REPL at launch or to `%server start` for 24 MiB events. Pass the same
options to `clang-tracer-server` when starting an external server. For REPL collections,
`--buffer-bytes` controls how much space is available for the retained event
and its projected stack. The short equivalent is:

```text
%probe inject "Calculator::multiply(int, int) const" --callback method.cpp
```

Omit `--callback` to use the built-in observer for event capture, including
receiver snapshots when class layout is available.

Use `%interceptor edit NAME` for a multiline draft, `.end` to save, and `.cancel`
to discard. `%probe validate SELECTOR --callback FILE` requests validation.
Resolution/deployment options include `--include`, `--compdb`, `--module` and
repeatable `--compile-option`. Headers and compilation-database paths refer to
the server host. Source files are limited to 4 MiB. Uploaded interceptors are
compiled by the server/agent; local REPL variables, pointers and lambdas are not
implicitly transferred into the target.

### Probe working directory

Add `--cwd DIRECTORY` to any `%probe` subcommand or to `%probes`.
Relative paths for that command use the specified directory on the REPL host.
This includes callbacks, prepared packages, supplemental include paths, and
preparation inputs and output. Backtick expansions also run in that directory.
For server-side headers, databases and translation units, the resulting paths
must be accessible on the server host. Paths inside raw `--compile-option`
arguments follow the compiler's working directory. A plain module name still
works as a filter. Later commands use their own working directory.

```text
%probe prepare "demo::compute(int)" --include src/compute.cpp --output prepared --cwd /work/demo
%probe inject "demo::compute(int)" --callback hook.cpp --prepared prepared --cwd /work/demo
```

Preparation looks for `compile_commands.json` in the command's working directory
by default. With either a database file or a directory supplied through
`--compdb`, it prefers a unique recorded command for the first include. If the
include has no entry, it uses the first source beside it. Use a unique
`--translation-unit` or a narrower database when the included source has
multiple commands.

### Supplemental probe compiler arguments

Use `--extra-arg` for one compiler argument and `--extra-args` for a list.
Backticks run a command on the REPL host and split its stdout into arguments.
A failed shell command stops the probe command.

```text
%probe prepare "Calculator::multiply(int, int) const" --include calculator.hpp --output multiply --extra-args `my-conf --cxxflags`
%probe inject "Calculator::multiply(int, int) const" --callback method.cpp --prepared multiply --extra-args "-I/opt/project/include -DTRACE_EXTRA=1"
```

Set defaults for preparation and agent compilation at REPL startup.

```bash
clang-tracer-repl --attach 12345 --extra-arg=-DTRACE_EXTRA=1
clang-tracer-repl --attach 12345 --extra-args "$(my-conf --cxxflags)"
```

Supplemental `-I`, `-isystem`, and `-idirafter` directories are searched
after the selected toolchain and compilation database directories. Supplemental
`-D` accepts new object-like macros and rejects names defined or undefined by
the compilation database. The clang-cl spellings `/I` and `/D` are also
accepted. The selected driver, language standard, target,
sysroot, and ABI remain authoritative. Preparation extras apply while reading
the original headers. Pass callback-specific extras to injection or at startup.
Relative supplemental include paths are resolved from the probe command's working
directory, which defaults to the REPL's working directory. These paths must be visible to the agent during callback compilation.

Attach checks the loaded agent's bootstrap protocol and ELF build ID against
the server and its configured agent file before accepting the session. Use
`%sessions` to see the process start time and target JIT runtime profile.
The reported libstdc++ header release and C++11 ABI flag describe the agent
build defaults. Probe-specific flags and target signature ABI checks still apply.
Successful installs report resolve, target JIT compile and patch durations
in nanoseconds when the server supports them.

### Reload an observer

```text
%probe reload PROBE_ID --callback observer-v2.cpp
```

Use the ID of the active probe. Reload keeps its target binding, ABI and
deployment options. The agent prepares the new observer before switching the
entry. A rejected callback or a switch that rolls back leaves the existing
observer in place. Cancellation is checked up to the entry switch. Use
`%probes` to identify the current generation if the reply is interrupted.

The entry switches directly to the new wrapper. Calls that already entered
the previous wrapper finish there, with both PRE and POST tagged by the
previous probe ID. A successful reload reports the new ID and retained
capacity. The 32-slot process limit includes retired wrappers, which stay
resident for safe late arrivals. Remove the active generation to restore the
original entry. Removing a superseded generation drains it while leaving the
successor in place. Detach drains all generations.

Borrow target pointers and references only during the callback. Use owned
snapshots for REPL inspection and replay.

## Collect and inspect

Missing semicolons inside loops or conditionals are syntax errors. They are
rejected by the isolated checker before the live interpreter parses the cell;
earlier variables, sessions and background collectors remain usable. Bare
top-level void calls retain their optional trailing semicolon. Temporary
collections can be inspected directly, including `live.snapshot().size()`.

### Event ranges

Synchronous collection waits for the count, timeout (30 seconds by default),
or Ctrl+C, then returns an iterable snapshot range:

```cpp
auto captured = tracer.events(session, 100, 10);
for (const auto &event : captured) tracer.print(event);
```

Background collection returns immediately so the prompt remains usable:

```cpp
auto live = tracer.events_async(session);
// Enter other REPL commands while collection continues.
for (const auto &event : live) tracer.print(event);
auto data = live.snapshot();
live.running();
live.error();
live.stop();
```

Each loop visits a stable snapshot of currently retained events, without waiting
for new arrivals or consuming them. Later loops can include the same events.
`snapshot()` returns a `Collection` with received/evicted counters and completion,
cancellation and timeout flags. Stream failures appear in `error()` in either
mode; partial results remain inspectable. `events_async(session, 100, 10)` stops
at 100 events or 10 seconds; zero count/timeout means no corresponding limit.
Synchronous counts and timeouts must be positive. Both modes use the REPL
collection API without requiring coroutines or standard-library ranges.

A range reserves the remaining shared event-count and byte budget. While it is
retained, another capture may have no room; release all copies of the range
(for example, with `%cpp reset`) to free that reservation. Snapshots can retain
their owned events beyond that point. The oldest events are evicted at the
budget limit. Stopping preserves the range and its reservation. The last range
copy's destruction, session detach, disconnection and console shutdown cancel
and join its worker. Workers never execute user C++ or print into the prompt.
The controller/facade remains a foreground API; use these range handles rather
than calling the facade from user-created threads.

Use `%help api EventRange` for the range API. Existing `tracer.collect(...)` and
named collection commands retain their behavior.

### Collection adaptors

Use `%help api adaptors` for an overview, or look up an individual operation
with `%help api drop`, `%help api filter`, or any other adaptor name.

`take`, `drop`, `filter`, `take_while`, `drop_while`, `transform`, `stride`,
`enumerate`, `reverse`, `to_vector`, and `first` are available directly in
the REPL. They also work with `Collection` values from `collect`, `batch`, and
`snapshot`:

```cpp
auto events = tracer.events(session, 100, 10);
for (const auto& e : events | take(10)) tracer.print(e);
auto is_post = [](const auto& e) { return e.phase() == "POST"; };
for (const auto& e : events | filter(is_post) | take(10)) tracer.print(e);
for (const auto& e : events | drop(5) | take(10)) tracer.print(e);
auto early = [](const auto& e) { return e.sequence() < 50; };
for (const auto& e : events | take_while(early)) tracer.print(e);
auto first_post = events | filter([](const auto& e) {
  return e.phase() == "POST";
}) | first();
if (first_post) tracer.print(*first_post);
auto names = events | transform([](const auto& e) {
  return e.function_name();
}) | to_vector();
for (const auto& [index, e] : events | stride(2) | enumerate()) {
  std::cout << index << ": " << e.function_name() << std::endl;
}
auto newest_first = events | reverse();
auto tail = events | drop_while([](const auto& e) {
  return e.sequence() < 50;
}) | to_vector();
```

`take(n)` returns at most the first n events; `drop(n)` skips the first n;
`filter(predicate)` keeps matching events; `take_while(predicate)` stops at the
first non-match. `first()` is a terminal adaptor returning an owned
`std::optional<Event>`, empty when there is no match. Counts must be nonnegative.
Predicates are evaluated lazily during iteration, and pipe order matters:
`filter(p) | take(10)` takes ten matches, while `take(10) | filter(p)` tests only
the first ten events. `take(0)` does not evaluate predicates.

Declare predicates before using them in a top-level REPL loop, or store the
pipeline in a variable first. Clang Interpreter can crash while mangling an
inline lambda inside a top-level loop; ordinary compiled C++ accepts it.

`drop_while(p)` skips only the initial matching prefix. `transform(f)` lazily
projects each event into an owned value, such as its name, sequence or typed
argument. `stride(n)` selects every nth element, starting with the first; n
must be positive. `enumerate()` pairs each element with a zero-based index in
the current pipeline. `to_vector()` materializes the selected values into an
owned vector; `reverse()` materializes a vector in reverse order. These two
operations evaluate their input immediately. Materialized results and views
can be piped into further adaptors. Exceptions from predicates/projections
propagate on the foreground thread.

Views own their input: lvalue collections are copied and temporaries are moved,
so stored pipelines remain valid. An event-range view retains its collector
handle and takes a fresh, non-consuming snapshot at each loop's start. Thus
`live | take(10)` inspects up to ten **currently retained** events; it does not
wait for ten new arrivals or limit background collection. Use
`tracer.events_async(session, 10, 5)` to limit collection itself. Other C++ code
can include `event_ranges.hpp` and use the `tracer_repl` namespace explicitly.

### Console commands

```text
%events
%collect --as live
%collect --count 100 --timeout 10 --as batch
%events list batch --limit 20
%events show batch --index 0
%events stack batch --index 0
%events calls batch
%events stats batch
%record capture.pb --count 1000
%record capture.db --sqlite --count 1000
%events open capture.pb --as saved
%events open capture.db --as stored
%events stats saved
%events export saved --format json --output events.json
%events clear saved
```

`%events`, `%collect`, and `%record` run until Ctrl+C when no count is supplied.
Ctrl+C cancels the stream, returns to the prompt, and leaves probes and the
server running. Counted streams default to a 30-second total timeout; explicit
`--timeout` overrides it. Partial batches survive cancellation, timeout, or a
service error and report their completion state and last cursor.

Filter with `--probe ID`, `--phase PRE|POST`, repeatable `--name SELECTOR`,
`--session ID`, `--all`, and `--after CURSOR`. Selection defaults to the current
session. Name/session/sequence filters go to the service; probe and phase
filters are applied locally, and counts refer to matching events.

All named collections share the configured event-count and Protobuf resident
allocation budget (defaults: 4096 events, 64 MiB, plus bounded container overhead). A collecting ring
evicts its oldest events when full and reports local evictions separately from
agent/server loss counters. Existing named collections reserve their capacity;
clear them to make room. C++ snapshots keep their own shared references and can
retain events after the console releases them.
Collecting again under the same name releases that collection before receiving
new events. Offline replay requires an unused name or an explicit `%events clear`
first, so a corrupt recording cannot overwrite an existing capture.

Detail inspection shows typed arguments/results, receiver fields, stack frames,
and raw metadata. `%events stack NAME --index N` prints the capture status and
each retained stack frame directly, including frames from replayed events.
Shared-state payloads are decoded with their attached schema when available.
Native PRE/POST pairing includes server lifetime, session,
probe, thread and call identity, and marks missing halves. Shared-context
invocation/task identities remain available in event details.
Loss statistics sum the latest observed cumulative agent counter per probe and
server counter per server lifetime, without counting repeated samples twice.
Unknown native phases remain inspectable and are counted outside PRE/POST pairs.

`%events stats NAME` uses the same summary for captured and replayed collections.
Each function row counts distinct observed calls for a probe in its process and
session. A call with just one phase contributes to `missing_pre` or
`missing_post`. Only a pair with one unambiguous timestamp per phase and a
nonnegative interval contributes to `timed` and the histogram. Histogram
labels are inclusive nanosecond ranges, with empty buckets omitted and a final
bucket for one second or longer. Durations measure the elapsed interval between
the two trace timestamps, including tracing overhead.

Caller edges count calls with evidence of the direct native caller in either
phase's entry stack. Caller labels use the function and module when available,
then the module offset or native PC. `caller_unknown` counts calls whose stack
does not establish that edge. Enable stack capture with `--stack-depth 8` when
installing a single probe. For a bulk manifest, pass this observer with
`%trace apply --manifest trace.textproto --callback observer.cpp` to capture
the stack on PRE and retain both phases for timing.

```cpp
struct Interceptor {
  void pre(const Call& call) { call.capture_stack(8, false); }
  void post(const Call&) {}
};
```

Agent and server loss counters are cumulative samples from retained events.
They and local buffer evictions are reported separately from incomplete calls.

By default, `%record` uses four-byte little-endian lengths and Protobuf
TraceEvent frames, with a 32 MiB per-frame limit. Framed recording and export
create new files exclusively. `%record FILE --sqlite` creates or appends to a
SQLite recording and deduplicates events by server instance and sequence.
Replay rejects truncated headers/bodies and invalid messages. A failed
recording keeps its partial file. Offline loading retains the configured
bounded window.

### Query a recording offline

Open the native console without attaching to a process. To browse a SQLite
recording from `clang-tracer drain --sqlite`, use the same commands as for
a framed Protobuf recording.

```text
%events open capture.db --as saved
%events list saved --limit 10
%events show saved --index 0
```

Use `tracer.replay` to filter the recording before loading a collection.

```cpp
tracer_repl::EventQuery query;
query.names = {"demo::compute(int)"};
query.phase = "POST";
query.count = 20;
auto matches = tracer.replay("capture.db", query);
for (const auto& event : matches) std::cout << event.result_as<int>() << '\n';
```

Set `pid` or `session_id` to narrow a capture that contains multiple processes
or sessions. Names match exact function selectors or custom event names. A
list of names matches any of them, and the other filters are combined. A
positive count reads up to that many matches in recording order. Zero reads
all matches and retains the latest events that fit the memory budget.
`received` and `evicted` show how much of the query was retained. Replay opens
SQLite read-only and works after the target and tracing server have exited.
The results support the same typed accessors and C++ adaptors as live events.

The native SQLite recorders (`%record FILE --sqlite`, `tracer.record()`, `clang-tracer events
--sqlite`, and `clang-tracer drain --sqlite`) save the standalone C++ type bundle for
prepared probes with custom state. Opening the database or calling
`tracer.replay` makes those types and their generated mappings available at
the next prompt. A fresh REPL can reconstruct the recorded C++ value without
access to the application headers:

```text
tracer_repl::EventQuery query;
query.names = {"account.adjusted"};
query.count = 1;
auto matches = tracer.replay("capture.db", query);
auto reading = matches.at(0).state_as<prepared_demo::Reading>();
```

Earlier recordings and recordings without a standalone type bundle still
support `state_snapshot()` and explicit C++ imports. Replay checks the saved
declarations before loading them. If a declaration is unsupported or conflicts
with a type already loaded in the REPL, the recorded fields remain inspectable.

To record from C++, call `tracer.record` while attached.

```cpp
tracer_repl::RecordOptions options;
options.names = {"account.adjusted"};
options.count = 12;
options.timeout_seconds = 15;
auto saved = tracer.record("capture.db", options);
```

This creates or appends to the SQLite recording. By default it uses the
selected session. Set `options.session` to another session handle or
`options.all_sessions` to record from all sessions. The filters also include
`phase`, `probe_id`, and `after_sequence`. `saved.complete` reports whether
the requested count was reached. Use `saved.received`, `saved.cursor`,
`saved.cancelled`, and `saved.timed_out` to inspect the outcome. See
`%help api tracer.record` for the native REPL topic.

## Analyze in C++

Discover the API from the prompt before writing analysis:

```text
%help
%help probe inject
%help api
%help api tracer
%help api Event
%help api tracer.events_async
%complete "tracer."
```

The command index groups commands by task. Look up a command or subcommand for
its usage, options, and examples. The C++ API index lists types and operations.
Individual API topics explain when to use them and show C++ examples. Type topics
also show the public signatures from the loaded declarations. Shortcuts such
as `%help Event` still work. Help uses syntax highlighting on color terminals
and plain text for redirected output or when `NO_COLOR` is set.

Tab completes commands, subcommands, options, known option values, help topics,
and C++ keywords and members. For example, `%collect --ph` completes to
`--phase`, and `%collect --phase PO` completes to `POST`. In a path argument
such as `%cpp load ./` or `%probe prepare "demo()" --cwd ./`, Tab lists local
matches. Directories end in `/` so you can continue completing the path.
Quoted C++ includes also complete local paths.

Press Tab after `#incl`, `if`, `for`, `while`, `%cpp load`, or
`%probe prepare` to insert a template. Type to replace the current placeholder
and press Tab or Shift+Tab to move between placeholders. Use Alt+/ to complete
a value while editing a placeholder. For `%probe inject`, Tab moves from the
selector to an optional arguments field. Type `%probe inject --callback` or
`%probe inject --prepared` before Tab to start with a path placeholder. The editor closes brackets and quotes as you type,
and Enter indents continued C++ input. Inside an empty brace pair, Enter
opens an indented line between the braces.

Member completion for expressions such as `tracer.`, `data.`, and
`data.at(0).` uses Clang and the session's accepted inputs. `%complete "data.at(0)."` prints the same candidates
when `data` has been declared. Completion parses code without executing it or
issuing tracing operations. It runs on Tab, rather than on each keystroke;
longer sessions can increase completion time. `%cpp reset` clears that session.
Declarations remain discoverable after an execution exception. After a C++
parse error, use `%cpp reset` to restore synchronized completion and bare
expressions; tracing commands, API help and statements terminated with `;`
remain available. Ambiguous bare input reports recovery guidance instead of
silently dropping a call.

Use `%type SYMBOL` to inspect a declaration in the current C++ session.
The command accepts simple names, qualified names such as `ns::function`,
and members such as `tracer.events_async`. It lists function overloads and
shows readable types such as `std::string` and `std::map<std::string, int>`.
It looks up declarations without evaluating C++ expressions.

Color terminals distinguish preprocessor directives, include headers, C++
keywords, strings, numbers, operators, percent commands, options, and
comments. The prompt, help examples, and `%type` output share the palette.

Ordinary C++ checks retain accepted syntax in a separate, syntax-only compiler
process. Each new input is checked in a disposable fork, so rejected declarations
and macros cannot corrupt the retained state or execute initializers. Later inputs
avoid reparsing the API and earlier cells; first use still initializes both
compilers. `%cpp reset` releases this checker along with the local C++ state.
Cancellation and the existing 15-second check deadline terminate only the owned
checker process group. Completion and header generation use separate helpers.

```cpp
auto data = tracer.batch("batch");
data.size()
data.at(0).argument(0)
std::cout << data.at(1).result() << std::endl;
tracer.print(data.at(0))
tracer.print(data)
int argument = data.at(0).argument_as<int>(0);
for (const auto &event : data) { if (event.has_result()) std::cout << event.result_as<int>() + argument << std::endl; }
for (const auto &event : data) tracer.print(event);
auto captured_session = session;
auto more = tracer.collect(captured_session, 100, 10);
more.complete
more.timed_out
```

`argument_as<T>(index)` and `result_as<T>()` return owned C++ values for
live and replayed events, including shared-context events. For a function with
a captured boolean argument/result:

```cpp
bool input = event.argument_as<bool>(0);
bool output = event.result_as<bool>();
if (output && input) std::cout << "both true" << std::endl;
```

Bool access requires a captured `BOOL`; integral access requires `INT32`,
`INT64`, or `UINT64`; floating access requires `FLOAT32` or `FLOAT64`.
Integer conversions check the destination range, including signed/unsigned
boundaries. Floating conversions check range and round to the requested
precision; NaN and infinity are preserved. Mismatched types, missing/invalid
payloads and void throw `std::invalid_argument`. Addresses without a captured
snapshot cannot become a string or local object.
Out-of-range numeric conversions and argument indices throw `std::out_of_range`.
A missing result throws `std::runtime_error`; use `has_result()` to check
presence before accessing it. Native PRE events normally have no result.

`result_value()` and `argument_value(index)` return a `ScalarValue`, defined as
`std::variant<bool, std::int64_t, std::uint64_t, double>`, for generic analysis
with `std::get`, `std::get_if`, or `std::visit`. The existing `result()` and
`argument(index)` methods remain text formatters. Inspect a complete event with
`tracer.print(event)` or `%events show batch --index 1`.

### Strings and class/struct values

New captures copy string and record contents while the target objects are
alive. `argument_as<std::string>(index)` and `result_as<std::string>()` return
owned strings, preserving empty strings, embedded NUL and arbitrary bytes.
Reference/value arguments are snapshotted at entry; POST events retain that
entry snapshot even if the function mutates or frees the original contents.
Return objects are captured at return, before leaving their valid lifetime.

For a raw pointer to a record with layout metadata, add
`--snapshot-return-pointee` to `%probe inject`. The built-in observer copies
the fields immediately after the function returns. Use a type declaration that
matches the target build and ensure each non-null pointer refers to a live
object whose fields are stable during capture.
Read the fields with `event.result_snapshot()` or decode an owned `T` with
`event.result_as<T>()` after importing a mapping for `T`. `event.result()`
still shows the address, including `0x0` for null returns.

For a polymorphic pointer return, preparation discovers eligible implementations
from the matching source inputs. RTTI selects a prepared concrete layout during
capture. Other live objects use the declared base layout. Pass an
implementation's `.cpp` with `--include` if its definition is in that file.
For typed local decoding of a polymorphic snapshot, supply an `EventCodec<T>`
for the value you want to construct.

For source-prepared pointer and reference returns, Clang also discovers direct
raw pointer members whose declared pointee is polymorphic and whose concrete
derived layout is available from the supplied source. At return, a checked RTTI
read selects the matching layout. A field such as `_imp` holds the original
address and `_imp_snapshot` holds the copied fields of the live implementation.
Null pointers, unreadable records, and unknown types remain address-only. The
target must keep the pointee and its captured data live and stable through POST.

A reusable record policy can follow specific raw pointer fields wherever that
record is captured as an argument, receiver, or result. Define the policy in
a C++ header and supply it through the existing `%probe prepare --include`
option alongside the matching application definitions. An explicit policy
selects the member set for that record and can include non-polymorphic records.

```cpp
namespace tracer_snapshot {
template<class Record> struct FollowPointers;

template<> struct FollowPointers<HFW::Item> {
  static constexpr const char* fields[] = {"_imp", "_other"};
};
}
```

Each configured field must refer to a live object with stable fields at the
capture point. The record snapshot keeps its address field and adds an owned
snapshot named `<field>_snapshot` for each captured pointee. For example,
`event.result_snapshot().field("_imp_snapshot")` reports its concrete type
from `cpp_type()`. A null member or a type absent from the prepared package
has only the address. Pointer returns use `--snapshot-return-pointee` at
injection. Reference returns already capture the referenced object.

To choose the captured fields for one implementation, pass a C++ file with
`--callback` to `%probe inject`. Define an overload of `tracer_snapshot::encode`
for that concrete type. The probe encodes the returned record using the same
bounded snapshot codec and reports the original implementation type. Other
implementations use automatic field capture.

```cpp
namespace tracer_snapshot {
struct ItemV2Fields { int revision; };

ItemV2Fields encode(const HFW::ItemV2& item) noexcept {
  return {item.revision};
}
}
```

The encoder executes in the target process and takes the live implementation by
const reference. It must be `noexcept` and return a named record by value. An
encoder-only file can be injected directly. If the file also defines an
invocation-scoped `Interceptor`, it needs a `post()` hook.

Inspect a record without declaring a local C++ type:

```cpp
auto value = event.argument_snapshot(0);
value.cpp_type()
int id = value.field("id").as<int>();
std::string name = value.field("name").as<std::string>();
auto fields = value.fields();
auto samples = value.field("samples").as<std::vector<int>>();
```

Import application headers to generate mappings from Clang's AST, including
nested records and record types inside standard containers:

```text
%cpp options add "-I/path/to/application/include" "-DAPP_FEATURE=1"
%cpp options
%cpp import "application/types.hpp"
```

After collecting events, `event.argument_as<application::Order>(0)` constructs a
local C++ object. Standard strings, vectors, arrays, lists, deques, maps, sets
and optionals decode recursively without registrations. The header is parsed
with the same local compiler options as ordinary C++ input. At startup,
repeatable `--cpp-arg ARG` remains available; for example
`clang-tracer-repl --cpp-arg -I/path/to/include --cpp-arg -DAPP_FEATURE=1`.

Bulk import maps eligible non-system application records visible after the
include. Use `%cpp import HEADER --type application::Order` to select a type;
repeat `--type` for more roots. Nested dependencies are generated automatically,
including named, nondependent template instantiations and aliases. Existing
explicit `EventRecord<T>` and `EventCodec<T>` definitions are preserved.
Structurally unsupported records are reported and skipped; an explicitly
selected unsupported type rejects the import. Generated conversions are checked in the disposable helper
before entering the live interpreter. Header/generation errors retain previous
variables and tracing sessions. An accepted header has normal C++ semantics,
including execution of its static initializers.

If a generated decoder fails to compile (for example, a set element lacks its
required comparison operator), the entire import is rejected without changing
live state. Retry with `--type` to select supported roots; no partial mappings
are installed from the failed import.

Configure options before the first accepted C++ cell. Help/completion alone do
not lock them; `%cpp options clear` restores the default profile at that point.
After C++ input, changing options is rejected without dropping variables.
An explicit `%cpp reset` clears C++ variables and unlocks configuration while
retaining tracing sessions. Local options do not change remote probe settings:
`--include`, `--compile-option` and `--compdb` still refer to the server.

Use `tracer.prepare` with a header path, a braced list, or a
`std::vector<std::string>`. Put the probed function's declaration header
first, followed by headers for types used by the callback.

```cpp
tracer.prepare("demo::compute(int)", {"demo.hpp", "state.hpp"}, "prepared");
```

Successful injection with a typed package prepared in the same REPL imports
eligible mappings from its standalone adapter. For `tracer.inject(...)`,
the mappings become available at the next input after the call. The import
uses the local compiler profile. If the mapping import fails, the probe stays
installed and snapshots remain available. For a package reused from another
session, `%cpp import` can load its exported `generated/adapter.hpp`.

Named `tracer::Context` events expose their state as owned C++ values.
`event.state_snapshot(index)` returns an `EventValue` for field inspection.
`event.state_as<T>(index)` uses the same conversions and record mappings as
`argument_as<T>`. The index defaults to the first state. Select the custom
event with `%collect --name NAME`. See the
[prepared method example](../examples/prepared-injectors/README.md#typed-custom-event)
for a nested record captured from typed arguments.

### Save and reuse generated wrappers

`%cpp import` saves its generated `EventRecord<T>` mappings and prints the
header's absolute path. Independent imports reuse an unchanged mapping across
sessions. The default directory is `$XDG_CACHE_HOME/clang-tracer/repl/mappings`,
or `~/.cache/clang-tracer/repl/mappings` when XDG_CACHE_HOME is unset. Cache keys
include the include name, selected types, compiler profile, working directory,
generator binary and compiler include-path environment settings. Checksums
cover the generated source and all physical
headers seen by the AST, including transitive dependencies. Include-search
lookups are also recorded, so newly created higher-priority headers and changes
to `__has_include` results invalidate reuse. Changed or missing dependencies
cause regeneration; unusual header-map/framework lookups that cannot be
represented reliably are regenerated. Imports after user C++ declarations generate
against the live declaration history and save a session-specific mapping.
Such imports may depend on earlier C++ declarations. Use `generate` for a
self-contained mapping header based on the application include alone.
Profiles with forced includes, precompiled headers, modules or VFS overlays
are regenerated rather than taking a potentially stale automatic cache hit.

Generate an entire include without loading it into the interpreter:

```text
%cpp options add "-I/path/to/application/include" "-DAPP_FEATURE=1"
%cpp generate "application/types.hpp" --output "application-events.hpp"
```

Omit `--type` to generate every eligible application record in the include;
repeat `--type QualifiedType` to select roots. `--output` also works with
`%cpp import`. Generation uses only Clang's syntax-only AST helper: it does not
start a JIT, link application symbols, run static initializers or execute user
constructors. Headers containing declarations of unavailable application
functions can therefore be analyzed. Structural eligibility and generated
decoder bodies are still checked by the compiler.
`generate` ignores live C++ variables and declarations; `import` honors them.

The saved header includes the resolved application header and the local event
API. Reuse it with `#include "/absolute/path/application-events.hpp"` in a
later REPL session with compatible compiler settings. Ordinary C++ loading and
decoding still require any function definitions they actually use. Automatic
cache lookup verifies dependencies; explicitly including a saved header has
normal C++ include semantics. Writes replace generated headers atomically;
unrelated existing files and application dependencies are never overwritten.

### Shell output and compilation databases

```text
%shell "pkg-config --cflags some-library"
%cpp options add `pkg-config --cflags some-library`
%cpp options add `cat ./compiler-flags.txt` "-DLOCAL_FEATURE=1"
%cpp options compdb "/path/to/build/compile_commands.json" --file "/path/to/app/types.hpp"
```

Backticks must occupy a complete argument to `options add`. Their stdout is
split into compiler arguments with quoted paths and escaped characters
preserved; it is never evaluated as a second shell command. Stderr is displayed
separately. `%shell` takes one quoted command and displays captured stdout and
stderr. Commands run locally with closed stdin, a ten-second deadline and a
64-KiB limit per output stream. Cancellation, timeout, malformed flag quoting,
or a nonzero exit leaves compiler options unchanged. Remaining background
children of a shell command are stopped when that command finishes.

`options compdb` accepts a database file or its containing directory and
**replaces** the local user compiler profile. It first selects an exact file
entry. If none exists, it chooses a C++ source entry in the requested file's
same directory, reporting the source selected. Multiple fallback files are
ordered by normalized filename; multiple exact entries use database order.
There is no fallback to another directory. Relative paths use the selected
entry's working directory. Source inputs, compiler launchers, output,
dependency-generation, language-action and linker arguments are removed;
defines, include paths, language-standard and other compile settings remain.
Compiler commands in JSON are parsed, never executed. Response-file arguments
must be expanded before loading the profile. Invalid profiles retain the
previous options; the existing before-first-C++-cell configuration rule applies.

Automatic mappings require accessible default/move construction and destruction,
public assignable supported fields, and no inheritance, unions or bit-fields.
Generation is bounded to 128 records, 128 fields per record and 60 KiB of source;
select fewer roots with `--type` when necessary. Use `%help cpp` for commands.

Manual mappings remain available when you want explicit control:

```cpp
struct Order { int id; std::string name; std::vector<int> samples; };
template<> struct tracer_repl::EventRecord<Order> {
  static constexpr const char* type_name = "Order"; // canonical target type
  static auto fields() {
    return std::make_tuple(
      tracer_repl::event_member("id", &Order::id),
      tracer_repl::event_member("name", &Order::name),
      tracer_repl::event_member("samples", &Order::samples));
  }
};
Order order = event.argument_as<Order>(0);
Order returned = event.result_as<Order>();
```

The mapping checks the remote record name, required field names, field kinds,
and numeric ranges. For classes with constructor-only/const members, specialize
`EventCodec<T>` with `static T decode(const EventValue&)` and return an object
from your constructor or factory. This custom conversion runs locally.

Automatic target capture supports complete ABI-validated records with public
serializable fields, including nested records and supported STL members.
Private/protected fields, unions, bit-fields, volatile data, arbitrary raw
pointees and wide/custom-character strings are explicitly unavailable.
There is a 4-KiB payload limit per value, a 16-KiB thread-local capture arena,
and bounded schema/item/depth limits. Missing, unsupported, truncated, malformed
or over-budget snapshots throw; partial objects are never returned as complete.
Older recordings containing only object addresses need a new capture to supply
contents. The old scalar variant/text APIs remain available.

Snapshots survive detach, target exit, collection clearing and recording replay.
Both native and shared-context event envelopes carry the same owned data.
Use `%help api EventValue` for the loaded inspection API and Tab on
`event.argument_snapshot(0).` for its methods. Install the server and agent
together so their bootstrap protocols match.

The prelude provides `tracer` and a selected-session reference named `session`.
Copying `session` makes a handle that retains that server/session identity;
later selection changes do not redirect it. Detached/stale handles fail before
tracing operations. The facade also provides `attach`, `inject`, and `detach`.
The interpreter initializes only for C++ input, so command-only startup does
not initialize Clang.

`tracer.print(event)` writes the full event immediately. `tracer.print(collection)`
prints its retention/completion summary and every retained event, including a
visible summary for an empty collection. Bare void calls work with or without
a trailing semicolon; the console works around Clang's void value-printing
behavior without executing calls twice. For text returned by `describe()` or
the text accessors, use `std::cout << value << std::endl` to display its contents
consistently across LLVM versions. Typed getters also decode owned string,
container and mapped record snapshots as described above.

Enter continues an unfinished C++ block, parenthesis, bracket, string, or block
comment in the multiline editor. Ctrl+C discards the entire pending input;
earlier variables and tracing sessions remain available. Ctrl+J always inserts
a newline; `%cpp load FILE` submits a multiline source file.

Every submitted C++ cell is checked in a disposable `clang-tracer-repl-check`
process before the live interpreter parses or executes it. The checker parses
the accepted cells in their original order without executing them, preserving
macro state and incremental source context. A rejected cell cannot change live
variables or macros, and completion and expressions remain available without
`%cpp reset`. The checker stops at the first compiler error. Ctrl+C cancels a
pending check; analysis also has a 15-second deadline and a 64-KiB output limit.
Tab completion runs in the same isolated, bounded helper. Install the helper
alongside `clang-tracer-repl` (CMake installs both).

Checks require the session's working directory and previously included headers
to remain unchanged. If either changes, input is rejected before live parsing:
restore those inputs to keep using the existing variables, or explicitly reset
to adopt the new compilation context. After C++ itself changes the working
directory, reset adopts that directory. Other compiler inputs, including header
search results and conditional include availability, must also remain stable;
the file checks detect ordinary size/timestamp changes to loaded headers.

`%cpp reset` explicitly destroys C++ declarations and values while preserving
the tracing controller and sessions; it is no longer routine error recovery.
Executed native C++ still runs in the console's process: runtime side effects
cannot be rolled back, and arbitrary native loops or crashes do not have
guaranteed interruption. Ctrl+C also cancels tracing operations.
Completion covers command names and local C++ declarations/members. Dynamic
completion of remote tracing symbols remains follow-up work.

## Cleanup and scripted use

```text
%probe remove 1
%detach
%server stop
%quit
```

Use the actual returned probe ID. Removing a probe reports restoration status.
EOF/quit detaches sessions this console created and stops only its owned server.
Selecting a pre-existing session does not transfer ownership. `%server stop`
waits for server restoration and is unavailable for an external service. A
shutdown timeout preserves the PID, endpoint and private directory for recovery;
the console never force-kills a restoring server.
On Linux, the owned server also receives a graceful termination signal if its
console dies unexpectedly, allowing it to restore hooks before exiting.

Piped input returns a nonzero status if any command or C++ input fails.
`--fail-fast` stops at the first failure and still performs cleanup. The parser
requires full-input acceptance and rejects malformed quotes, trailing garbage,
duplicate/conflicting options, integer overflow and invalid limits before
dispatch. Console commands use `%`; C++ input uses Clang's parser.

## Regenerate the grammar

The lexer/parser and visitor interfaces are generated from
`src/repl/TracerCommands.g4` with ANTLR 4.13.2. Generated sources ship with the
repository, so neither Java nor Python is needed to run the console.

```sh
scripts/regenerate-repl-parser.sh /path/to/antlr-4.13.2-complete.jar
scripts/regenerate-repl-parser.sh /path/to/antlr-4.13.2-complete.jar --check
```

The script verifies the jar's SHA256
`eae2dfa119a64327444672aff63e9ec35a20180dc5b8090b7a6ab85125df4d76`.
Set `TRACER_REPL_JAVA` to a Java executable when it is not on `PATH`. The
generated parser builds a typed command; semantic validation precedes execution.
Generated grammar actions never perform target operations.

## Validation

The native client was built and tested on macOS with LLVM 22.1.8 and on Linux
x86-64 with LLVM 21.1.8. All six client CTests pass on both hosts. A separate
real Linux integration check covers owned-server startup, PID/name attach,
interceptor deployment, live event collection/inspection, local C++ analysis,
recording/replay, probe restoration and cleanup. Typed-value tests cover bools,
integers, floating values, type/payload errors and range boundaries. The Linux
check also verifies native bool arguments/results and typed recording replay.
Terminal tests exercise the user's bare `tracer.print(data.at(0))` spelling,
batch printing, actual Tab insertion for methods/fields, and in-prompt API help.
Staged installations on both
hosts evaluate C++ through the installed facade header. See
`NATIVE_REPL_PROGRESS.md` for evidence and the exact validation scope.

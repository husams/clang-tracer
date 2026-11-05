# Native C++17 REPL

For runnable examples of structs/classes, strings, optionals, sequence
containers, maps, sets and nested containers, see
[`examples/native-repl-values`](../examples/native-repl-values/README.md).

`clang-tracer-repl` manages a tracing session from one prompt. Commands start
an owned local server, attach to a process, upload C++ interceptor source, and
collect and inspect events. Ordinary C++ input runs locally through Clang
Interpreter. Python is not required to run the console.

## Build

The new console, its controller, generated RPC bindings and command parser are
compiled as C++17. The `c++17` base branch still selects C++20 for the existing
backend; the REPL does not inherit that requirement. Build the client alone on
Linux or macOS:

```sh
cmake -S . -B build-repl -G Ninja -DTRACER_REPL_ONLY=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm \
  -DClang_DIR=/path/to/llvm/lib/cmake/clang
cmake --build build-repl -j2
ctest --test-dir build-repl --output-on-failure
```

Supply a matching LLVM/Clang 21 or 22 development installation and gRPC/Protobuf
development packages and compiler plugin. The implementation has separate
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
choosing a displayed PID. `%processes --filter TEXT` lists local candidates.
Remote process enumeration is not in the service API, so remote attach uses an
explicit PID.

If an attach reply fails or times out, the operation may still finish on the
server. The console reports the endpoint, PID and newly observed candidate
session IDs, blocks automatic retries for that PID, and reconciles again on
`%status` and exit. Inspect `%sessions` and explicitly `%detach SESSION` when
appropriate. Candidates are never assumed to belong to this console on an
external server; recovery information is retained until explicitly resolved.

## Interceptor upload

```text
%server status
%sessions
%modules
%symbols --filter Calculator --limit 20
%probe resolve "Calculator::multiply(int, int) const"
%interceptor load method.cpp --as multiply_hook
%interceptor show multiply_hook
%interceptor upload multiply_hook --target "Calculator::multiply(int, int) const" --stack-depth 16
%probes
```

`load` reads the file on the console machine. `upload` sends its source by value
through ResolveProbe/AddProbe and reports success only for an active probe.
The draft remains available after deployment errors. The short equivalent is:

```text
%probe inject "Calculator::multiply(int, int) const" --callback method.cpp
```

Use `%interceptor edit NAME` for a multiline draft, `.end` to save, and `.cancel`
to discard. `%probe validate SELECTOR --callback FILE` requests validation.
Resolution/deployment options include `--include`, `--compdb`, `--module` and
repeatable `--compile-option`. Headers and compilation-database paths refer to
the server host. Source files are limited to 4 MiB. Uploaded interceptors are
compiled by the server/agent; local REPL variables, pointers and lambdas are not
implicitly transferred into the target.

## Collect and inspect

### C++17 event ranges

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
Synchronous counts and timeouts must be positive. Both modes use C++17, without
coroutines or C++20 ranges.

A range reserves the remaining shared event-count and byte budget. While it is
retained, another capture may have no room; release all copies of the range
(for example, with `%cpp reset`) to free that reservation. Snapshots can retain
their owned events beyond that point. The oldest events are evicted at the
budget limit. Stopping preserves the range and its reservation. The last range
copy's destruction, session detach, disconnection and console shutdown cancel
and join its worker. Workers never execute user C++ or print into the prompt.
The controller/facade remains a foreground API; use these range handles rather
than calling the facade from user-created threads.

Use `%help EventRange` for the range API. Existing `tracer.collect(...)` and
named collection commands retain their behavior.

### C++17 collection adaptors

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
%events calls batch
%events stats batch
%record capture.pb --count 1000
%events open capture.pb --as saved
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
and raw metadata. Shared-state payloads are decoded with their attached schema
when available. Native PRE/POST pairing includes server lifetime, session,
probe, thread and call identity, and marks missing halves. Shared-context
invocation/task identities remain available in event details.
Loss statistics sum the latest observed cumulative agent counter per probe and
server counter per server lifetime, without counting repeated samples twice.
Unknown native phases remain inspectable and are counted outside PRE/POST pairs.

Recordings use the Python client's four-byte little-endian length plus Protobuf
TraceEvent framing, with a 32 MiB per-frame limit. Recording and export create
new files exclusively. Replay rejects truncated headers/bodies and invalid
messages. A failed recording keeps its partial file; offline loading retains
only the configured bounded window. Use recording for long captures.

## Analyze in C++

Discover the API from the prompt before writing analysis:

```text
%help api
%help tracer
%help Event
%help Collection
%complete "tracer."
```

Help prints public fields, method signatures, templates and defaults from the
loaded facade declarations. Tab completes C++ names and members, including
`tracer.`, `data.`, and `data.at(0).`, using Clang's semantic analysis and the
session's semantically accepted inputs. `%complete "data.at(0)."` prints the same candidates
when `data` has been declared. Completion parses code without executing it or
issuing tracing operations. It runs on Tab, rather than on each keystroke;
longer sessions can increase completion time. `%cpp reset` clears that session.
Declarations remain discoverable after an execution exception. After a C++
parse error, use `%cpp reset` to restore synchronized completion and bare
expressions; tracing commands, API help and statements terminated with `;`
remain available. Ambiguous bare input reports recovery guidance instead of
silently dropping a call.

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
Use `%help EventValue` for the loaded inspection API and Tab on
`event.argument_snapshot(0).` for its methods. Backend/agent bootstrap version
20 is required for new captures; install the server and agent together.

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

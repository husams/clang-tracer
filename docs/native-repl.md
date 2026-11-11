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

### Launch a local application

Build the application with the installed C++17 `<tracer/launch.hpp>` header and add
a checkpoint at the start of `main`, before starting threads or calling the
code you want to trace. It returns immediately on an ordinary run. Treat a
failed checkpoint as a startup failure.

```cpp
#include <tracer/launch.hpp>

int main() {
  if (!tracer::launch::wait_for_resume())
    return 125;
  return run_application();
}
```

Use these commands in the native REPL with its private Linux x86-64 server.
`%launch` selects the attached session and holds the application at the
checkpoint while you install probes.

```text
%launch ./app --arg "example input" --env MODE=test --cwd ./work --stdin ./input.txt --timeout 5
%probe inject "app::start()" --callback observer.cpp
%resume
%terminate
```

The earliest traceable work is the first eligible call after the checkpoint.
The loader, global constructors, and any earlier application code have already
run. Repeat `--arg` for arguments and `--env NAME=VALUE` for overrides to the
inherited environment. Relative paths are resolved from the REPL's working
directory. Standard input defaults to `/dev/null`. Use `--stdout NEW_FILE`
and `--stderr NEW_FILE` to choose output files. The REPL reports the paths and
otherwise creates private log files. Existing files are protected from overwrite.

`--timeout` bounds the wait for the checkpoint in seconds. It defaults to
the REPL timeout, which also applies separately to attach. Setup or attach
failure and readiness timeout terminate and reap the child. `%resume [PID]`
and `%terminate [PID]` use the selected target when PID is omitted.
`%terminate` kills and reaps the launched child whether paused or resumed.
The REPL retains ownership after detach and terminates any remaining owned
children on exit. Use `%status` to list owned targets and reap any that have
already exited. Only the launched process is managed.

If an attach reply fails or times out, the operation may still finish on the
server. The console reports the endpoint, PID and newly observed candidate
session IDs, blocks automatic retries for that PID, and reconciles again on
`%status` and exit. Inspect `%sessions` and explicitly `%detach SESSION` when
appropriate. Candidates are never assumed to belong to this console on an
external server; recovery information is retained until explicitly resolved.

### Call a registered function in the paused target

Register each callable at the startup checkpoint. The application compiler
checks the function pointer's argument and result types. The name is an exact
match to the name you register and can distinguish overloads.

```cpp
#include <tracer/launch.hpp>

double scale(int value, double factor) { return value * factor; }

int main() {
  using tracer::launch::named_call;
  if (!tracer::launch::wait_for_resume(named_call("app::scale", &scale)))
    return 125;
  // Continue normal application startup.
}
```

```text
%launch ./app
%call app::scale --arg 6 --arg 1.5
%resume
```

The result line shows the type and value, such as `type=double value=9`.
Registered functions accept up to eight arguments drawn from `bool`,
32-bit and 64-bit signed and unsigned integers, `float`, and `double`.
The same types and `void` are available for results. Enter decimal integers,
finite decimal floating-point values, and `true` or `false`. Wrong argument
counts and invalid values are rejected before execution. Pointers, references,
objects, variadic functions, and functions with an implicit receiver are
outside this interface.

Calls run one at a time on the thread that entered the checkpoint, before
`%resume`. Choose functions that are ready to run at that point in startup.
Their side effects remain in the target, and an escaping C++ exception is
reported as a failed call. Returned scalars are copied into the REPL's reply.
Calls require a target launched by this REPL with a registered checkpoint.

Use `--timeout SECONDS` to bound how long the REPL waits. Timeout or
interruption leaves the call pending and it may continue running. Use
`%call-result [--timeout SECONDS]` to collect its reply. A pending call
blocks another call, resume, or probe changes on that target.
`%terminate` still kills the owned target if it cannot finish.

## Browse a local C++ target

```text
%threads
%modules --filter libdemo.so --details
%symbols --filter Calculator --addresses
%symbols --address 0xADDRESS --mapping TOKEN
```

`%threads` shows thread IDs, names and start times for the selected process.
`%modules` lists application module paths from the selected attached process.
`--filter TEXT` matches a case-sensitive substring of the full path. Add
`--details` for file-backed executable address ranges, file offsets, device
and inode, and the ELF build ID when the backing file can be verified. The
mapping token describes the session's full snapshot, including when the
display is filtered.

Use the C++ API at the prompt or in a REPL script.

```cpp
auto paths = tracer.modules("libdemo.so");
auto module = tracer.module_info(paths.at(0));
for (const auto& entry : module.mappings) { std::cout << entry.begin_address << ' ' << entry.build_id << '\n'; }
```

Call `tracer.modules()` to list all application module paths. Its `ModuleList`
result supports iteration, `size()`, `empty()`, and `at()`. The paths are
also available as a vector in `.paths`. Pass an exact path from the list or a
unique exact filename to `tracer.module_info()`.
Missing or ambiguous names throw `std::invalid_argument`. The result includes
`path`, `mapping_token`, and `mappings`. Each mapping has `begin_address`,
exclusive `end_address`, `file_offset`, `device`, `inode`, and `build_id`.

`%symbols --addresses` shows runtime and ELF entries for supported C++ callables
when there is a unique verified mapping. For reverse lookup, copy a runtime
address and the `mapping=` token from the same session. Addresses inside a
supported callable also work. If the mappings or backing files change, take a
new snapshot before using an address. The token checks observed mapping
identity. An unload and reload of the same image at the same address between
snapshots can produce identical observations.

## Capture local syscalls or CPU samples

Attach to a running Linux x86-64 target using the REPL's own local server,
then choose a capture:

```text
%syscalls --timeout 2 --count 256
%sample cpu --timeout 2 --count 256
```

`%syscalls` reports separate entry and exit records with the syscall number
and the raw kernel return value on exit. `%sample cpu` reports user-space
instruction addresses at roughly one sample per 10 ms of scheduled CPU time.
Each record carries the process ID, thread ID, and a monotonic nanosecond
timestamp on the same clock as probe events. Use a current mapping snapshot
and `%symbols --address` to look up a sample address.

Each capture snapshots and verifies up to 64 existing target threads.
Threads created after that snapshot and child processes are outside the
capture. The command releases its kernel resources before printing records
in timestamp order. It stops on timeout, the record limit, Ctrl+C, or the
exit of all monitored threads. The defaults are two seconds and 256 records,
with ceilings of 60 seconds and 8192 records.

The summary reports kernel loss, throttle notifications, and samples
discarded at the requested limit separately. Kernels with a perf loss counter
include loss still pending at shutdown. On older kernels, `loss_accounting=records`
counts loss notifications the kernel wrote to the ring. Collection boundaries,
loss, and the record limit can leave individual syscall entries or exits
without a partner. These pilot records are printed by their capture command.

Syscall capture reads the raw syscall tracepoint IDs and formats from tracefs
and opens the events through perf for each target thread. It needs tracefs
read access and permission to open those events. CPU sampling uses a user-space
perf task-clock event and can work under a policy that denies syscall tracing.
Both modes report permission failures directly. Perf events allow the target
threads to continue running. A syscall-stop backend based on ptrace would stop
them at each syscall boundary and compete with the injector for ptrace ownership.
[Linux event tracing](https://docs.kernel.org/trace/events.html) ·
[Perf permissions](https://docs.kernel.org/admin-guide/perf-security.html) ·
[perf_event_open](https://man7.org/linux/man-pages/man2/perf_event_open.2.html) ·
[ptrace](https://man7.org/linux/man-pages/man2/ptrace.2.html)

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

### Opt in to scalar overrides

Mark an eligible free-function definition in the application and build it with
Clang and DWARF. The marker declares the ordinary Linux x86-64 SysV ABI.
Use it on functions that follow the standard register preservation rules.
The agent verifies the marked ELF section, mapped entry, calling convention,
and complete scalar signature before installing a detour.

```cpp
#include <tracer/override_target.hpp>

TRACER_SCALAR_OVERRIDE_TARGET int compute(int input) { return input * 2; }
```

In the callback, request an argument change in `pre()` or a return change in
`post()`. Indexes start at zero.

```cpp
struct Interceptor {
  void pre(const Call &call) {
    if (call.arg<int>(0) == 10)
      call.override_argument(0, 12);
  }
  void post(const Call &call) {
    if (call.result<int>() == 24)
      call.override_result(25);
  }
};
```

```text
%probe validate "compute(int)" --callback override.cpp --allow-arg-overrides --allow-result-override
%probe inject "compute(int)" --callback override.cpp --allow-arg-overrides --allow-result-override
```

Each flag enables only its corresponding hook operation. Overrides require a
local Unix control socket and a DWARF binding. Use `--prepared-mode off` if a
prepared package would otherwise be selected. Supported values are `bool`,
signed 32- and 64-bit integers, unsigned 64-bit integers, `float`, and
`double`. Match the slot's type and width exactly. A `void` function can use
argument overrides.

The original function runs with the committed arguments. An escaping hook
exception discards its staged writes. An exception from the original function
propagates to its caller without running `post()`. Overrides apply per call,
including recursion, and preserve the original function's `errno` behavior.
PRE events show the incoming arguments. POST events show the arguments used
by the function and its original return value. Reload retains the permissions
of the active generation. Removing the probe or detaching restores the
function entry.

## Collect and inspect

Missing semicolons inside loops or conditionals are syntax errors. They are
rejected by the isolated checker before the live interpreter parses the cell;
earlier variables, sessions and background collectors remain usable. Bare
top-level void calls retain their optional trailing semicolon. Temporary
collections can be inspected directly, including `live.snapshot().size()`.

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

Use `%help api EventRange` for the range API. Existing `tracer.collect(...)` and
named collection commands retain their behavior.

### C++17 collection adaptors

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
In C++, `event.stack()` returns an owned vector of frames in capture order.
It is empty when the event has no stored frames.

```cpp
auto event = tracer.batch("batch").at(0);
for (const auto& frame : event.stack()) { std::cout << frame.pc << ' ' << frame.function << '\n'; }
```

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

For source-prepared probes, Clang also discovers the raw pointer and reference
members of every captured record (arguments, receivers, and results, including
members declared by public non-virtual bases) and follows them. A field such as
`_imp` holds the original address and `_imp_snapshot` holds the copied fields
of the live pointee; a reference member is captured the same way. For a
polymorphic pointee the admitted layouts are the declared class, when it is
concrete, and concrete derived classes available from the supplied source, and
a checked RTTI read selects the matching layout. A non-polymorphic pointee is
captured by its exact static type, so no policy is needed, provided everything
the capture must read is checkable: scalars, enums, arrays, strings, `vector`,
`array`, `optional`, `pair`, `tuple`, `variant`, smart pointers to such types,
and records of those. A pointee holding anything else (`list`, `map`, `set`,
`unordered_*`, `deque`, `std::any`, `std::function`, `weak_ptr`) is followed
automatically only when it is polymorphic and its RTTI matches a derived class,
or the declared class of an owner-held implementation pointer; otherwise it
stays address-only unless a `FollowPointers` policy names it. Because there is
no RTTI to check, only the pointer's alignment and the readability of the whole
object are verified, and the target must keep the pointee valid and stable
through POST. A `char` pointer is captured as a bounded (`--string-bytes`,
longer text is truncated), readability-checked C string and a pointer to an
arithmetic or enum value as one element; both are stored as `<field>_snapshot`.
The same rules apply to the pointer and reference members of every captured
pointee, of records held by value inside a captured record, and of the pointees
of `std::unique_ptr` and `std::shared_ptr` members, to `--follow-depth` levels.
A link back to an object that is already captured above it is not followed
again, and a link cut by the depth bound stays address-only. Null, unreadable,
unmapped, abstract, incomplete, `std::` and function pointees remain
address-only, and pointers stored inside standard containers are not followed.
All roots are encoded before any followed member, shallowest members first, so
a member that does not fit the per-thread snapshot storage is dropped and never
displaces a root. Smart pointer members (`std::unique_ptr`, `std::shared_ptr`)
are captured inline as their address plus a copy of the pointee. Pass
`--no-automatic-members` to `%probe prepare` or to
`clang-tracer-prepare-injectors-native`, or set
`TRACER_PREPARE_AUTOMATIC_MEMBERS=off` in the environment of the prepare tool,
to follow no members automatically. If a package cannot be built with the fuller
captures, preparation falls back to narrower ones and reports each step on
stderr. The agent, the REPL, and the prepare tool must come from the same
build: an older REPL cannot read nested member snapshots and an older agent
lacks the readability checks a newer package calls. Followed members are
encoded in a checked mode: strings are copied out with a checked read, a
vector's storage and a smart pointer's pointee must be wholly readable first,
and a shared pointer's control block is never read, so a pointer the target
left stale makes that member address-only instead of crashing the target. This
relies on `process_vm_readv`; a seccomp filter that kills that call would kill
the target. Policy-named members get the same checks, but a `FollowPointers`
policy on a type the checks cannot cover (node-based containers, `std::any`,
`std::function`, virtual bases, custom encoders) is your own assertion that the
pointee is valid. Records with virtual bases, pairs and tuples with reference
elements, `vector<bool>` and `weak_ptr` are never followed automatically. A
pointee that needs a custom `tracer_snapshot::encode` is followed automatically
only when its RTTI matches. Every limit on this capture is an option
(see [Followed-member limits](#followed-member-limits)); none is hard-coded. A
pointer or reference to a private nested implementation class
(`class Widget { class Impl; Impl *impl_; };`) is followed too, because the
prepared adapter republishes nested class names. If the capture code that
follows members does not compile, the probe is built without them and the
validation report says so; setting `TRACER_TEST_BREAK_FOLLOWED_MEMBERS` in the
target's environment forces that fallback for testing. In a followed member a
`weak_ptr` is always recorded as expired, and a smart pointer whose pointee was
left out by the checks or the read budget looks the same as one with no
pointee.

A reusable record policy can follow specific raw pointer or reference fields wherever that
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

#### Followed-member limits

Every bound on followed-member capture is an option. Nothing is hard-coded: the
defaults below live in one place (`SnapshotLimits` in `src/snapshot_limits.hpp`),
each option is validated against the same table, and when a limit cuts something
the event or the prepare output says which option to raise.

| Option | Environment variable | Set with | Default | Range | Meaning |
| --- | --- | --- | --- | --- | --- |
| `--follow-depth` | `TRACER_PREPARE_FOLLOW_DEPTH` | prepare tool, `%probe prepare`, `%probe inject` | 3 | 0-40 | Levels of pointees followed below a captured record; `0` follows none. |
| `--follow-members` | `TRACER_PREPARE_FOLLOW_MEMBERS` | prepare tool, `%probe prepare`, `%probe inject` | 48 | 0-4294967295 | Followed members per probe, policy-named ones first. Each adds about 160 bytes of stack to the generated wrapper. |
| `--record-members` | `TRACER_PREPARE_RECORD_MEMBERS` | prepare tool, `%probe prepare` | 32 | 0-4294967295 | Pointer members discovered automatically per record. |
| `--nested-depth` | `TRACER_PREPARE_NESTED_DEPTH` | prepare tool, `%probe prepare` | 2 | 0-4294967295 | Levels of by-value records and smart pointers searched for pointer members. |
| `--string-bytes` | `TRACER_PREPARE_STRING_BYTES` | prepare tool, `%probe prepare`, `%probe inject` | 4000 | 0-65536 | Bytes kept of one string (`char *` or `std::string`) in a followed member. At most the slice minus 96 bytes of framing. |
| `--vector-elements` | `TRACER_PREPARE_VECTOR_ELEMENTS` | prepare tool, `%probe prepare`, `%probe inject` | 4096 | 0-4294967295 | Elements of a `std::vector` in a followed member; a larger vector is left out. The captured bytes must also fit `--snapshot-slice` (5000 `int` elements need `--vector-elements 5000 --snapshot-slice 32768`). |
| `--checked-reads` | `TRACER_PREPARE_CHECKED_READS` | prepare tool, `%probe prepare`, `%probe inject` | 256 | 1-4294967295 | Checked memory reads (system calls) one followed member may spend. |
| `--snapshot-reserve` | `TRACER_PREPARE_SNAPSHOT_RESERVE` | prepare tool, `%probe prepare`, `%probe inject` | 8192 | 0-4294967295 | Bytes of the per-thread storage that members captured at entry leave for the roots encoded later (a result, nested calls). |
| `--snapshot-arena` | `TRACER_PREPARE_SNAPSHOT_ARENA` | prepare tool, `%probe prepare`, `%probe inject` | 16384 | 16384-4294967295 | Per-thread storage for all snapshots of a call. Never shrinks: the largest request of any installed probe wins. At least one slice. |
| `--snapshot-slice` | `TRACER_PREPARE_SNAPSHOT_SLICE` | prepare tool, `%probe prepare`, `%probe inject` | 4096 | 4096-65536 | Bytes of one snapshot payload. Never shrinks: the largest request wins. |

Besides these, `--no-automatic-members` (prepare tool flag, `%probe prepare`
option, or `TRACER_PREPARE_AUTOMATIC_MEMBERS=off`) follows no member
automatically, and `TRACER_REPL_DERIVED_CANDIDATES` (environment of the REPL,
default 15) sets how many derived records are tried per polymorphic pointer
when the REPL builds its local mappings; a pointer with more candidates is
reported in the mapping notes.

A value is a number, `unlimited` (the largest value in the Range column) or,
where the range starts at 0, `off` (0). A value outside the range is refused
with the range, for example `--follow-depth must be between 0 and 40 (or
unlimited): 99`. A prepare-time option shapes the package, so `%probe inject`
can only lower what the package discovered; the other options can be given at
either step, and the one given at `%probe inject` wins. The three maxima that are
not "as large as the option holds" are forced by something else:

- `--snapshot-slice` 65536 and, through it, `--string-bytes`: a payload is one
  string field of the event, which the event writer bounds at 64 KiB
  (`limits.string_bytes` in `src/context_capture.cpp`). The whole event is also
  bounded by the server's `--context-event-bytes` (default 256 KiB).
- `--follow-depth` 40: every followed level nests two protobuf messages in the
  event, and the protobuf parser refuses more than 100 levels when the server
  and the REPL read it. Measured end to end, a chain arrives up to 49 levels in
  a result and 48 in an argument and the event is lost beyond that; 40 keeps
  eight levels of slack for the receiver and state roots that were not measured.
- `--snapshot-arena` and `--snapshot-slice` have the built-in sizes as their
  minimum because the per-thread storage only grows.
- `--snapshot-arena` accepts up to 4294967295 bytes, but the storage is one
  anonymous mapping per thread of the target, so `unlimited` asks every thread
  for 4 GiB of address space. If a mapping fails the storage keeps its size and
  the capture reports `no snapshot storage was left for a followed member`.

Tune them where the data lives. The depth and the `char *` size, for the
handle example (`examples/prepared-injectors/handle`):

```text
%probe prepare "HFW::HfwManager::getItem(int)" --include item.hpp --include item-impl.hpp --output prepared --follow-depth 5
%probe inject "HFW::HfwManager::getItem(int)" --prepared prepared --prepared-mode require --string-bytes 6000 --snapshot-slice 8192 --snapshot-arena 65536
```

or `TRACER_PREPARE_FOLLOW_DEPTH=5` in the environment of the REPL or of
`clang-tracer-prepare-injectors-native`.

What you see when a limit cuts something:

| Cut | Where | Text |
| --- | --- | --- |
| Pointer members of a record are beyond `--follow-depth` (prepare) | prepare stderr, once per record | `prepare: pointer members of NAME are address-only: it is N levels below a captured record (--follow-depth, TRACER_PREPARE_FOLLOW_DEPTH)` |
| More pointer members than `--record-members` (prepare) | prepare stderr, once per record | `prepare: NAME has more followable members than --record-members (N); the rest stay address-only` |
| Pointer members nested deeper than `--nested-depth` (prepare) | prepare stderr, once per record | `prepare: NAME has pointer members nested more than --nested-depth (N) levels inside it; they stay address-only` |
| Derived records beyond `TRACER_REPL_DERIVED_CANDIDATES` | mapping notes of the REPL | `derived records of NAME beyond N are not tried (TRACER_REPL_DERIVED_CANDIDATES)` |
| A member's own pointer members were not followed (`--follow-depth`) | field `<field>_snapshot_cut` of the parent | `its pointer members were not followed: raise --follow-depth` |
| Members dropped by `--follow-members` below a member | field `<field>_snapshot_cut` of the parent | `some of its pointer members were not followed: raise --follow-members` |
| Members dropped by `--follow-members` at a root | field `snapshot_cut` of the root | `N pointer members were not followed: raise --follow-members` |
| String shortened (`--string-bytes`, or the slice bounds it) | field `<field>_snapshot_cut` | `a C string was shortened: raise --string-bytes (and --snapshot-slice, which also bounds it)` (`a string was ...` for a `std::string`) |
| Vector larger than `--vector-elements` | field `<field>_snapshot_cut` | `a vector was left out because it is larger than the limit: raise --vector-elements` |
| Checked reads ran out | field `<field>_snapshot_cut` | `the checked reads of a followed member ran out: raise --checked-reads` |
| Member does not fit the slice | field `<field>_snapshot_cut` | `a followed member does not fit the snapshot payload: raise --snapshot-slice or --snapshot-arena` |
| No storage left for a member | field `<field>_snapshot_cut` | `no snapshot storage was left for a followed member: raise --snapshot-arena or lower --snapshot-reserve` |
| Member does not fit the event (shared-context events) | field `<field>_snapshot_cut` | `the followed member did not fit the event: raise --context-event-bytes of the server` |

`<field>_snapshot_cut` is a string field next to `<field>_snapshot`, so
`event.result_snapshot().field("_chain_snapshot_cut")` reads it; a cut inside a
nested member sits in that member's record.

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

The native REPL applies Clang's `-fno-access-control` to all local C++ input,
checking, and mapping generation. This lets you inspect private and protected
members directly and lets generated decoders include them. Pass
`-fno-access-control` when compiling a generated header with non-public
members outside the REPL.

The REPL can generate a factory when Clang can map every argument of a unique
constructor to a captured field. It recognizes direct member initialization
and forwarding through supported base constructors. The factory constructs
the value and copies any remaining assignable fields. This also covers const
fields initialized by the constructor. A prepared adapter can retain the
verified initializer list. Constructor code that transforms values or depends
on application state needs a custom `EventCodec<T>`. The factory requires
usable local lifetime definitions and does not reconstruct raw-pointer
ownership from captured addresses.

Eligible derived records decode inherited fields as well as their own fields.
Use `DeclaringType::field` for field names shared by multiple base or derived
declarations. A shared virtual base contributes its fields once. Polymorphic
records need locally usable construction, destruction, and virtual function
definitions. With the REPL's own local Linux x86-64 server, a prepared probe
can resolve an out-of-line destructor from a shared library already mapped in
the target. With a server the REPL did not start, the library is used only when
the target is a process on this host, owned by the REPL's user, whose
`/proc/PID/maps` confirms the reported mappings; otherwise the record is
rebuilt field by field, as described below. The REPL checks the mapped ELF identity and requires one
library exporting the required C++ ABI symbols. It loads that library into the
REPL process and binds those symbols before generating the typed decoder. No
additional probe options are required. Library initializers run in the REPL
when the loader opens the library.

A record that cannot be constructed locally is decoded as
`tracer_repl::Reconstructed<T>`; use `->` to reach the record. This covers
records with raw pointer or reference members, records whose constructor,
virtual functions, or destructor are unavailable or cannot be inferred, and
records that contain such a record by value or through a `std::unique_ptr`.
Records that map normally keep doing so. A reconstructed record's fields are
rebuilt without running its constructors or destructor and its vptr is null, so
read fields and call non-virtual members only. A pointer member refers to a
local copy of its captured pointee, which is itself rebuilt the same way, so a
captured chain of pointers can be followed with `->`. It is null when the
pointee was not captured or its captured type is neither the declared pointee
nor one of the imported application records derived from it (at most 15 derived
records are considered); a captured pointee that fails to decode raises the
decode error. A `char` pointer points at a copy of the captured string and a
pointer to a scalar at a copy of the value. A reference member needs its
referent: decoding throws `std::runtime_error` naming the record and field when
it was not captured. Smart pointer members point at the local copy of the
pointee, or are empty when it was not captured. Other raw pointers are null. Do
not move, copy, assign or destroy the reconstructed record itself; it owns the
pointees, so do not move from, reset or assign its `std::unique_ptr` members
either, because those only view the copies. Records with virtual bases cannot
be reconstructed. Define your own `EventCodec<T>` before first use to replace
the generated reconstruction.

Use `result_snapshot()` for field inspection when only the captured layout is
available. If additional definitions are needed for a typed result, prepare
the package in one session, then import the application definitions and
inject the saved package in a fresh session.

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

Automatic target capture uses complete ABI-validated layouts and serializable
fields, including nested records and supported STL members. Prepared layouts
can include private and protected fields. Unions, bit-fields, volatile data,
arbitrary raw pointees and wide/custom-character strings are unavailable for
general automatic record capture.
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

End a `%` command line with `\` between arguments to continue on the next
line. The final line submits the command. The same syntax works in piped input
and `--startup` files.

```text
%module index \
  --file ./app
```

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

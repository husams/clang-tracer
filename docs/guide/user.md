# Clang Tracer user guide

Clang Tracer attaches to a running Linux x86-64 C++ process. A **session** identifies an attachment and a **probe** runs a C++ observer when a selected function is called. PRE runs before the original call and POST runs after a normal return. The injected agent sends events to the server, where clients can stream, retain, or record them.

| Interface | Use it for |
| --- | --- |
| `clang-tracer-repl` | Explore a local process, prepare and install probes, record and analyze events in C++. |
| `clang-tracer` | Run one tracing operation at a time in a shell or script. |
| `clang-tracer-module` | Index and inspect compiled ELF files and their debug information from the shell. |

Start with the first probe below, then use the [tutorials](tutorials.md), [advanced guide](advanced.md), [command appendix](command-reference.md), and [C++ API appendix](api-reference.md).

## Install and check the tools

On the `c++17` branch, the native Linux x86-64 backend builds as C++20 and the native REPL builds as C++17. Use matching LLVM/Clang 21 or 22 development libraries, CMake, Protobuf, gRPC and SQLite development packages. The injected agent needs compatible runtime libraries on the target machine. Build the target with debug info, preserve its ELF or separate debug files, and use a compiler and ABI supported by the agent. The tracer must have permission to attach to the target under the host's Linux tracing policy.

From the repository root, configure, build and install into a user-owned prefix. Replace the toolchain path with the matching LLVM/Clang installation.

```bash
./build-rhel.sh --toolchain-root /opt/llvm-21 --build-dir build-rhel \
  --prefix "$HOME/.local/clang-tracer" --examples
./deploy-rhel.sh --build-dir build-rhel
export PATH="$HOME/.local/clang-tracer/bin:$PATH"
clang-tracer-server --help
clang-tracer-repl --help
clang-tracer-module --help
```

For a split LLVM installation, CMake options, staging, tarball and RPM packaging, see [Build and deploy on RHEL-compatible Linux](../build-rhel.md). For GCC 8.5 SDK targets, use the [SDK compatibility build](../gcc85-compatibility.md). If you already have the native package, put its binary directory on `PATH` and run the help commands. The server finds its matching installed agent automatically. The native REPL starts and owns a private server unless you specify an endpoint.

## Open the interactive guide

Open [the HTML guide](index.html) directly in a browser. It includes a collapsible navigation sidebar, section filtering, and copy buttons. For HTTP access, run the launcher with Python 3.8 or newer. It defaults to `127.0.0.1:8000` and accepts any bind address available on the host, including IPv6. It serves the documentation directory only.

```bash
# From a source checkout
./docs/guide/serve.sh --bind 127.0.0.1 --port 8000
# From a default runtime installation
/usr/local/share/doc/clang_tracer/docs/guide/serve.sh --bind 0.0.0.0 --port 8080
```

Open `http://127.0.0.1:8000/` for the first command. For a wildcard bind, use the host's actual IP address or hostname in the browser. Ctrl+C stops the guide server.

## Create a target and a basic probe

Work in a scratch directory that both terminals can access. Save this as `target.cpp`. The target prints its PID and waits for Enter so you control when the probe fires. When the SDK headers expose `PR_SET_PTRACER`, the demo requests permission for a tracer in the second terminal.

```cpp
#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

__attribute__((noinline)) int add(int a, int b) { return a + b; }

int main() {
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
#endif
  std::printf("PID %ld | press Enter to call add, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("add(20, 22) = %d\n", add(20, 22));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}
```

Save the observer as `hook.cpp`. Specialize `Interceptor<int(int, int)>` to match the return and parameter types of `add`. Clang Tracer supplies the primary template and `Call`, so the callback can read typed arguments and results directly.

```cpp
template <>
class Interceptor<int(int, int)> {
public:
  void pre(const Call& call) {
    call.field("left", call.argument<0>());
    call.field("right", call.argument<1>());
  }

  void post(const Call& call) {
    call.field("sum", call.result());
  }
};
```

Start the target in terminal one with debug information and a predictable function entry. Keep the target running throughout the session.

```bash
c++ -g -O0 -fno-omit-frame-pointer -fno-pie -no-pie -Wl,--build-id target.cpp -o target
./target
```

In terminal two, run `clang-tracer-repl` from the same directory. Replace `12345` with the printed PID. You can also use `%attach` without a PID to open the process picker.

```bash
%attach 12345
%symbols --filter "add("
%probe resolve "add(int, int)"
%probe validate "add(int, int)" --callback hook.cpp
%probe inject "add(int, int)" --callback hook.cpp --print-arguments --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as first
```

Press Enter once in terminal one while `%collect` is waiting. The target prints `42`; the retained events include the arguments and the sum. You can inspect the pair and read the result as a C++ value.

```bash
%events list first
%events show first --index 1
%events calls first
```

```cpp
auto first_call = tracer.batch("first");
first_call.at(1).result_as<int>()
```

```bash
%detach
%quit
```

Stop the target with Ctrl+C. If discovery is empty, confirm the attached PID, use `%modules` to confirm the binary, and check that it still has its debug information. If attachment is refused, confirm the target is yours, its tracing policy permits the attachment, and no debugger already owns it.

## Index debug information and prepare a probe from a binary

A module index is a SQLite file built from a Linux x86-64 executable or shared
library and its matching DWARF debug information. Use it to inspect signatures,
ELF symbols, and known type layouts before attaching to a process. You can also
prepare a probe from the index with just the matching ELF and your callback.

### Build and browse the index

Use `target` and `hook.cpp` from [the first probe](#create-a-target-and-a-basic-probe).
Start a fresh native REPL in that directory. These commands work without a
target attachment.

```bash
%module index --file ./target --output target.sqlite
%module types --filter int
%module symbols --filter "add*"
%module show --symbol "add(int, int)"
%module show --type int
```

The symbol details include the linkage name, ELF address, return type and
parameters when available. The type details report size and, for records,
known fields and their offsets. Use the full signature or copy an ID from a
listing when a name is ambiguous. Use `--exported` on `%module symbols` when
you want only dynamic exports, such as the exported functions of a shared
library. Regular functions in the first-probe executable can be indexed even
when they are absent from that export list.

Omit `--output` to write `<ELF>.tracer.sqlite`. Choose a new filename when
rebuilding an index. The REPL selects the index after creating it. In a new
REPL, use `%module use target.sqlite` to select it again, or pass
`--index target.sqlite` on an individual query.

When DWARF lives in a separate file, let the indexer find a matching debug
companion or pass `--debug-file FILE`. For example, create a separate-debug
copy of the lab binary and index that copy.

```bash
cp target target-split
objcopy --only-keep-debug target-split target-split.debug
objcopy --strip-debug --add-gnu-debuglink=target-split.debug target-split
clang-tracer-module index --file ./target-split --debug-file ./target-split.debug --output target-split.sqlite
clang-tracer-module info --index target-split.sqlite
```

If you already have a local target attached to the REPL's own server, you
can index a loaded image using `%modules --details`. Copy the exact path
and mapping token from its output. The mapped ELF must have a build ID.
If the output says `build_id=unavailable`, use file indexing instead.

```bash
%module index --module /absolute/path/to/target --mapping TOKEN --output live-target.sqlite
```

An ELF without usable DWARF can still provide symbols, including exports, but
preparation needs a supported signature and sufficient type information.
Split DWARF units are reported as unresolved. If you see
`debug_file_mismatch`, use the debug file from the same build.

### Generate declarations and install the indexed probe

Back in the REPL, select the original index. Emitting headers is optional.
It writes `declarations.hpp`, `views.hpp` for supported read-only field access,
and `manifest.json`. The final command below prepares a package using the
generated files and verifies them against the selected index entry.

```bash
%module use target.sqlite
%module emit --symbol "add(int, int)" --output recovered-add
%probe prepare "add(int, int)" --module-index target.sqlite --module ./target --generated recovered-add --output prepared-add
```

Leave out `--generated` to let the preparer generate the same declarations
internally. The package binds to the indexed ELF build and embeds the
declarations it needs. Keep the package with your callback and use the same
binary for the live step. If the application is rebuilt, index the new ELF
and prepare a new package.

Run `./target` in another terminal. Replace `12345` with its printed PID and
use the same `hook.cpp` from the first lab.

```bash
%attach 12345
%probe validate "add(int, int)" --callback hook.cpp --prepared prepared-add --prepared-mode require
%probe inject "add(int, int)" --callback hook.cpp --prepared prepared-add --prepared-mode require --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as indexed
%events list indexed
%detach
```

Confirm that validation reports `prepared_used: true`. Press Enter in the
target terminal while the capture waits. The POST event reports `sum=42`.
The index and debug companion are needed to recover metadata. A completed
prepared package can be reused for injection without either file.

See [module index commands](commands.md#native-repl-module-index-commands) for
all options, [the native REPL walkthrough](../native-repl.md#prepare-a-probe-from-a-binary-without-headers)
for a callback that reads recovered record fields, and the
[native C++ module API](developer.md#native-c-module-index-api)
for a programmatic workflow inside the REPL or a C++ application.

## Trace changes to C++ objects

Use a native REPL method watch to compare a live receiver before and after
a call in a prebuilt executable. Select private members, strings, nested
records, or containers with `--field`. Pair vector elements by a stable ID
and map entries by key using `--key`, then inspect the typed changes from
the SQLite recording. Individual write watches identify the actual store
instruction for supported typed fields inside a selected vector element,
ordered or unordered map entry, or pointee. They also trace verified
libstdc++ string, variant and any storage with original store locations and
before and after bytes. Nested unions also report their affected storage
bytes, with typed samples for verified tagged alternatives. For keyed vectors and maps,
`--mode logical-writes` also records changes at verified mutation and
destruction boundaries. See the [native REPL object watch walkthrough](../native-repl.md#prepare-an-object-watch-for-method-snapshots).

## Observe objects and prepare custom watch plans

Object observation follows lifetimes and state, rather than only function arguments. Choose **individual writes** to observe admitted stores to a field, or **method snapshots** to compare receiver state at entry and return. A snapshot reports the net change during the call; it cannot show a change that was undone before return. Both paths report coverage gaps rather than guaranteeing every write in the process.

Use matching REPL, server, and agent builds and the target's matching ELF/DWARF. The following uses the repository's `repl-object-plan-target` fixture, built with `cmake --build build --target repl-object-plan-target`. Start `./build/repl-object-plan-target` in another terminal, then prepare a reusable plan and attach to its PID:

```text
%object prepare demo::PlanRecord --module ./build/repl-object-plan-target --field value --code "demo::write_value(demo::PlanRecord &)" --backend code-cache --output ./record-watch
%attach PID
%object preview --plan ./record-watch
%object start --plan ./record-watch --stack-depth 4
%record object-writes.db --sqlite --name object.trace --probe WATCH_ID --count 2 --timeout 30
```

Replace `WATCH_ID` with the printed watch ID. Enter `g` in the target while recording: the fixture changes `value` from 17 to 23. Inspect coverage and restore the patches before detaching:

```text
%object status WATCH_ID
%object stop WATCH_ID
%detach
%events open object-writes.db --as objects
%events list objects
%events show objects --index 1
%events stack objects --index 1
```

Check `active=no` and `restoration_pending=no`; retry stop with the same ID if restoration is pending. Use fresh plan and recording paths for another run. Add `--no-values` to start a saved write plan when you need `FIELD_WRITTEN` events, including same-value stores, without value samples. `code-cache` executes supported stores in the target; the default `ptrace` backend steps verified stores for diagnostic use.

### Customize fields, methods, and collection identity

For your own class, supply its original headers and compilation database. This illustrative plan observes `Portfolio::rebalance()` and compares the selected collection fields, using stable element keys:

```text
%object prepare demo::Portfolio --module ./portfolio --include ./portfolio.hpp --policy ./snapshot-policy.hpp --compdb ./compile_commands.json --translation-unit ./portfolio.cpp --on "demo::Portfolio::rebalance()" --field legs --field risk --key legs=id --key risk=key --synchronization thread-confined --output ./portfolio-watch --emit-source ./generated
%object preview --plan ./portfolio-watch
%object start --plan ./portfolio-watch
```

Run preview and start after attaching to the Portfolio process. Replace the example files, method, fields, and keys with your application's definitions. The policy header controls supported snapshot customization. Declare the actual synchronization: `thread-confined` for single-thread access, `caller-lock` when the caller holds the protecting lock across both samples, or `stable-at-boundaries` when the application guarantees safe samples. These declarations do not add locking. Configure `--max-elements`, `--max-string-bytes`, `--max-depth`, `--max-snapshot-bytes`, and `--max-instances` for your capture needs. Record and stop using the returned watch ID as above.

Read [the object observation walkthrough](../native-repl.md#trace-object-lifetimes-and-state-in-a-prebuilt-binary) and [method snapshot details](../native-repl.md#prepare-an-object-watch-for-method-snapshots) for coverage, synchronization, and supported layouts.

## Register custom object probes

Use matching REPL, server, and agent builds. See [the custom object-probe walkthrough](../native-repl.md#register-a-c-type-for-object-probing) and [live-update documentation](../native-repl.md#live-code-replacement-on-linux-x86-64) for complete options and supported target profiles.

A custom object-probe package groups admitted lifetime hooks and typed method callbacks under one registration. Prepare from the original header and compile command; use a fresh output directory. Save this observer as `callbacks.cpp`:

```cpp
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  long long pre(const Call& call) {
    return static_cast<long long>(call.object_id());
  }
  long long post(const Call& call) {
    return static_cast<long long>(call.object_generation());
  }
};
```

For an application defining `demo::Counter::add(int)`:

```text
%object-probe prepare demo::Counter --include ./counter.hpp --compdb ./compile_commands.json --translation-unit ./counter.cpp --method "demo::Counter::add(int)" --callbacks ./callbacks.cpp --output ./counter.object
%attach PID
%object-probe preview --package ./counter.object
%object-probe start --package ./counter.object
%object-probe status REGISTRATION_ID
%object-probe stop REGISTRATION_ID
```

Use the printed registration ID for lifetime/write recording and the printed member probe IDs for ordinary PRE/POST events. A receiver whose lifetime was not verified can have provisional identity: `identity_verified()` is false and its object ID and generation are zero. Do not treat its address as proof of a complete lifetime. Correlate verified observations by registration, object ID, and generation. Check coverage and cleanup status before detaching.

A write-only package can select a field and known writer with `--field value --code "demo::Counter::set(int)" --dwarf-writers`. With the original compilation database, omitting `--code` enables writer discovery. Preview the package before activation; combined method and writer patches must not overlap.

## Apply live updates

On a build providing `%live`, live replacement changes admitted native function bodies in a running Linux x86-64 process. It requires build provenance: preserve original source files in a separate snapshot directory and create `clang-tracer-live.textproto` containing the source root, snapshot root, module path, module and compilation-database SHA-256 hashes, and each input path and original hash. Build affected units with debug information, `-O0 -fno-inline`, `-fpatchable-function-entry=16,0`, and a GNU ELF build ID. See the linked integration documentation for the manifest format and admission limits.

Attach to that build, save a body edit, and select the matching database and manifest:

```text
%live preview ./pricing.cpp --compdb ./compile_commands.json --build ./clang-tracer-live.textproto
%live update ./pricing.cpp --compdb ./compile_commands.json --build ./clang-tracer-live.textproto
%live status
%live revert
```

Preview lists exact entries and blockers; update previews and applies an admitted group. Check the application's behavior in the same process after publication. `%live revert` restores the parent revision and `%live revert 0` restores original build entries. `%live watch ./pricing.cpp` previews and applies saved edits continuously; Ctrl+C returns to the prompt. Watch mode applies changes automatically.

The initial profile checks exact symbols in the main executable, entry bytes, source signatures, and ABI. Unsupported call coverage, static-storage changes, or incompatible layouts can require a rebuild. Reverted JIT generations remain mapped; `--retention-limit N` controls the retained-generation limit. Updating an observer alone uses `%probe reload PROBE_ID --callback hook.cpp`, which is separate from replacing application code.

### Update existing object state

Use explicit discovery to supply owned objects or guarded borrows to an update callback:

```text
%live update-data shared.cpp --discover-file discover.cpp --discover-function discover
%live scan-heap --type example::Cache --type example::Account
```

The discovery function uses `clang_tracer::ObjectSink` from `<clang_tracer/live.hpp>`. Heap scan results are candidates with evidence, not automatically writable objects; unknown ownership remains read-only. Updates from scans reacquire the original owner. Changing layout requires application-managed storage, the exported managed registry, and a migration recipe; ordinary object observation does not enable migration. For a managed publication reporting `RESTORATION_REQUIRED`, use `%live recover` in the same server session and inspect recovery status.

## Mock a native function

On a live-update build, provide a fixture implementation with the supported callback interface and select the exact target signature:

```text
%live mock "ProductStore::load(int)" --file product_fixture.cpp --function load_fixture --fixture products.txt
%live status
%live revert
```

The source file must define `load_fixture` for the target's signature; `products.txt` supplies its fixture data. Follow the linked live-update documentation and matching build examples for callback types. Use `--bind "ALIAS=SIGNATURE"` to bind verified application helpers. Inside the mock, `ctx.original(...)` calls the unmocked implementation, including an active live update. `--recursion original` routes recursive calls through that implementation. Replacing a mock preserves the underlying implementation; a direct source update to the mocked callable replaces the mock and reports that change in preview. Exercise the application while the mock is active, then revert and verify the original behavior returns.

## Extend the client and agent with C++ plugins

Use matching plugin-enabled REPL, server, and agent builds and check `%help` for installed commands. See [the plugin walkthrough](../native-repl.md#load-local-c-plugins). Client plugins execute in the native REPL; agent plugins execute inside the attached target. Load trusted source only.

### Load a client plugin

Save this self-contained counter as `counter-plugin.cpp`. The installed SDK supplies the entry point and dispatches commands to one plugin instance:

```cpp
#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  uint64_t count_ = 0;
public:
  uint64_t tick() noexcept { return ++count_; }
  uint64_t value() const noexcept { return count_; }
  static constexpr Command commands[] = {
      command<&Plugin::tick>("tick", "Increment the count."),
      command<&Plugin::value>("value", "Read the count."),
  };
};
```

At the REPL prompt:

```text
%plugin load counter-plugin.cpp --as counter
%plugin commands counter
%plugin run counter tick
%plugin run counter value
%plugin reload counter
%plugin run counter value
```

The first two runs return 1; the value after successful reload is 0. Reload creates fresh state for that plugin and leaves peer plugins active. A failed reload retains the previous generation. Commands take no arguments, are `noexcept`, and return a value convertible to `uint64_t`. Keep plugin state constant-initializable and trivially destructible. Plugin scopes resolve their own definitions; external runtime or other-plugin dependencies can fail with unresolved symbols, so begin with self-contained code like this example.

Use `%plugin enable counter-plugin.cpp --as counter` to load now and on future launches; `%plugin disable counter` removes startup registration but does not unload the current instance. Enabled sources load before the prompt and startup scripts. Compiler inputs come from the local C++ profile, including `--cpp-arg` or `%cpp options compdb`. First compilation reports `compiled`; compatible later loads report `cache hit`. Cached code is reused, but each load gets fresh state.

### Load an agent plugin

For a local managed server, enable agent plugins before launching the REPL; replace the PID with your target's PID:

```sh
CLANG_TRACER_ALLOW_AGENT_PLUGINS=1 clang-tracer-repl --attach 12345
```

For an external server, set that variable in the server's environment before starting it. This opt-in permits its clients to submit native plugin code to the target. On a supported Linux x86-64 target, use the same counter source:

```text
%plugin agent load counter-plugin.cpp --as target-counter
%plugin agent run target-counter tick
%plugin agent run target-counter value
%plugin agent reload target-counter counter-plugin.cpp
%plugin agent run target-counter value
%detach
```

The counter lives in the target and is separate from the client counter. Successful reload resets its state while preserving peer plugins. Loading reports `REPL object` when it accepts a compatible client-compiled object, `cache hit` for the agent's cached object, or `compiled` for target compilation. The agent verifies compatibility before linking. Each attachment owns its instances; detach releases them while leaving compiled objects cached for reuse.

### Install a package for startup and attachment

For automatic loading, create `~/.local/share/clang-tracer/plugins/counter/` (or `$XDG_DATA_HOME/clang-tracer/plugins/counter/`) on the REPL machine. Copy the counter source to `src/plugin.cpp` and save this `plugin.toml` alongside `src/`:

```toml
id = "counter"
sdk = 1

[repl]
sources = ["src/plugin.cpp"]

[agent]
sources = ["src/plugin.cpp"]

[agent.target]
executable = "/absolute/path/to/application"
```

Set `executable` to the target host's exact executable path. The `[repl]` component loads at startup; with the server opt-in enabled, `[agent]` activates on a new matching attachment. Selecting an existing attachment preserves its state. Load failures identify the package and leave the attachment available. Omit a component section for a client-only or agent-only package.

Each component supports optional `compile_commands`, `translation_unit`, and `extra_flags`. Package source paths are relative to the manifest; external agent compiler inputs must use target-host paths. Keep the package owned by the REPL user, disable group/other write permissions, and keep package symlinks within its directory.

For public client C++ APIs, include declarations under the package's `include/` directory; users can include those headers at the prompt and call the package's definitions. Restart the REPL to replace packages with public headers. For an in-target return observer, see [the agent observer example](https://github.com/husams/clang-tracer/tree/c%2B%2B17/examples/repl-agent-observer): load it on attach before adding native probes, and reload the installed package with `%plugin agent reload NAME`.

Client object caches live under `$XDG_CACHE_HOME/clang-tracer/repl/plugins` and target caches under `$XDG_CACHE_HOME/clang-tracer/agent/plugins`, defaulting to the corresponding paths below `~/.cache`. Enabled single-file client registrations live under `$XDG_CONFIG_HOME/clang-tracer/repl/enabled-plugins`, defaulting to `~/.config/clang-tracer/repl/enabled-plugins`.

## Related documentation

- [Command reference](commands.md)
- [Basic and advanced tutorials](tutorials.md)
- [Native C++ REPL walkthrough](../native-repl.md)
- [Event retention and SQLite storage](../event-storage.md)
- [Developer guide and API reference](developer.md)
- [Optional Python client and console](python-client.md)

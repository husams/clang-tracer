# Command reference

See [the C++17 branch reference](cpp17.md) for additional commands and options. Use the [first probe](user.md#create-a-target-and-a-basic-probe) for the examples below. Substitute its PID for `12345` and use actual session IDs, probe IDs, addresses, and tokens from your output.

## Native REPL startup

```bash
clang-tracer-repl --attach 12345 --buffer-events 8192 --cpp-arg "-I."
```

| Option | Meaning |
| --- | --- |
| `--help` | Print startup usage and exit. |
| `--attach PID`, `--process FILTER` | Attach by PID, or find a local process by filter. Choose one. |
| `--endpoint ENDPOINT` | Use an existing server. Overrides `CLANG_TRACER_ENDPOINT`. Without either, the REPL manages a private server. |
| `--server PATH`, `--agent PATH` | Override the executable and agent for the managed server. |
| `--timeout SEC` | Default client deadline in seconds. The default is 30, with a maximum of 300. |
| `--buffer-events N`, `--buffer-bytes N` | Total retained collection capacity. Defaults are 4096 events and 64 MiB. |
| `--context-event-bytes N`, `--context-transport-memory-bytes N` | Managed server's shared event and transport memory budgets. |
| `--cpp-arg ARG` | Append one local interpreter compiler argument. Repeat for additional arguments. Use separate tokens, e.g. `--cpp-arg "-I."`. |
| `--extra-arg=ARG`, `--extra-args FLAGS` | Append one probe compiler argument or a shell-quoted list to the probe defaults. Repeat as needed. |
| `--startup FILE` | Read commands and C++ from a local script. |
| `--fail-fast` | Exit after the first input failure. Useful with redirected scripts or `--startup`. |

Inside the REPL, quote selectors and paths containing spaces. Commands start with `%`. Other input is evaluated as C++. Use `%help probe inject`, `%help api Event`, and Tab completion to look up commands and C++ names.

## Native REPL session and discovery commands

| Command and example | Options and behavior |
| --- | --- |
| `%server start --agent /opt/tracer/lib64/libclang_tracer_agent.so` | Start and own a private server. `--executable PATH` and `--agent PATH` override the paths. `--context-event-bytes N` and `--context-transport-memory-bytes N` override managed transport budgets. |
| `%server connect 127.0.0.1:50051` | Switch to an existing endpoint after cleaning up the current connection. |
| `%server status` | Show current endpoint, readiness, and ownership. |
| `%server stop` | Stop the owned server and restore its targets. |
| `%processes --filter target` | List local processes. `--filter TEXT` narrows the picker candidates. |
| `%attach 12345 --start-time 98765` | Attach by PID. Optional `--start-time TICKS` verifies Linux process identity. Omit PID to use the local picker. |
| `%sessions` | List known attachments and their probes. |
| `%use s1` | Select an attached session for subsequent commands. |
| `%status` | Show current connection, session, and retained collections. |
| `%detach s1` | Detach and restore that session. Omit the ID for the selected session. |
| `%modules --details` | List eligible application modules. `--details` adds executable mappings, ELF identity, and a mapping token. |
| `%threads` | List selected target threads, including TIDs, names, and start times. |
| `%symbols --filter add --limit 20 --addresses` | Browse supported DWARF callables. `--filter TEXT` matches a substring and `--module TEXT` narrows module paths. `--limit N` is a page size, with 0 meaning unbounded. `--page TOKEN` continues the query. `--addresses` adds verified live entries. `--mapping TOKEN` supplies a module mapping snapshot. `--address PC --mapping TOKEN` looks up a live PC and disables paging. |

## Native REPL module index commands

| Command | Purpose |
| --- | --- |
| `%module index --file ELF [--debug-file FILE] [--output INDEX]` | Index ELF symbols and DWARF and select the index. The default output is `<ELF>.tracer.sqlite`. |
| `%module index --module PATH --mapping TOKEN [--output INDEX]` | Index an exact mapped ELF using the REPL's local server. Get the path and token from `%modules --details`. |
| `%module use INDEX` | Select a saved index for queries, emission and probe preparation. |
| `%module types [--filter GLOB] [--index INDEX]` | List named types and known layout sizes. |
| `%module symbols [--exported] [--filter GLOB] [--index INDEX]` | List symbols and their IDs. Use `--exported` to select dynamic exports. |
| `%module show (--type NAME \| --symbol SELECTOR \| --id ID)` | Inspect a type or symbol. Add `--index INDEX` to select a saved index explicitly. |
| `%module emit [--type NAME] [--symbol SELECTOR] [--id ID] --output DIR` | Generate declarations, read-only field views and a manifest for selected entries. |

List commands accept `--format json`, `--limit N` and `--page TOKEN`.

## Native REPL probe and bulk trace commands

| Command and example | Options and behavior |
| --- | --- |
| `%probe prepare "add(int, int)" --include add.hpp --output prepared` | Generate a reusable local package. The first `--include HEADER` supplies the declaration and additional includes supply types. `--output NEW_DIR` must be new. `--compdb PATH` selects the compilation database, defaulting to the command's directory. `--translation-unit FILE` selects an explicit command. `--module ELF` pins ELF identity. `--metadata-only` skips adapter files. Accepts extra compiler arguments and `--cwd`. |
| `%probe prepare SELECTOR --output prepared [--module-index INDEX] [--module ELF] [--generated DIR]` | Prepare from DWARF and ELF metadata in a module index. Omit `--module-index` to use the default. Use `--id ID` to select a specific indexed symbol. |
| `%probe resolve "add(int, int)"` | Show resolved signature, ABI and target metadata. Accepts resolution options below. |
| `%probe validate "add(int, int)" --callback hook.cpp` | Compile and link the local callback without installing it. Accepts resolution options. |
| `%probe inject "add(int, int)" --callback hook.cpp --stack-depth 16` | Install or replace the probe. Accepts resolution and deployment options. |
| `%probe entry "add(int, int)" --mapping TOKEN` | Inspect verified machine-code entry, patch window and control flow. Add `--module PATH` to disambiguate. For explicit entry use `%probe entry --module PATH --elf-address 0x401000 --abi linux-x86-64-sysv --mapping TOKEN`. The address is ELF-relative, before ASLR. Both forms accept `--cwd`. |
| `%probe reload PROBE_ID --callback updated.cpp` | Compile a new observer and switch the active generation while existing calls finish on their original one. Accepts `--cwd`. |
| `%probe remove PROBE_ID` | Restore original code for the probe ID. Accepts `--cwd`. |
| `%probes` | List probes in the selected session. Accepts `--cwd`. |
| `%interceptor load hook.cpp --as observer` | Read a local callback into a named draft. |
| `%interceptor edit observer` | Enter multiline source. Finish with `.end` to save or `.cancel` to discard. |
| `%interceptor show observer` | Print the draft source. |
| `%interceptor validate observer --target "add(int, int)"` | Check a draft against the target selector. Accepts resolution options except `--cwd`. |
| `%interceptor upload observer --target "add(int, int)"` | Install a draft. Accepts resolution and deployment options except `--cwd`. |
| `%trace preview --exact "add(int, int)" --save trace.textproto` | Report eligibility and capacity before installing anything. Select using repeatable `--module GLOB`, `--include GLOB`, `--exclude GLOB`, and `--exact SELECTOR`. Require an include or exact rule. Alternatively `--manifest FILE` loads rules, and `--save NEW_FILE` writes the manifest. |
| `%trace apply --manifest trace.textproto --callback hook.cpp` | Install eligible probes in a batch. `--callback FILE` overrides the default observer. The server attempts to restore newly installed probes on partial failure. |
| `%trace remove batch-1` | Restore the probes owned by the returned batch ID, or retry restoration. |

| Resolution and deployment option | Meaning |
| --- | --- |
| `--include HEADER`, `--compdb PATH` | Repeat `--include` for supplemental headers. Use `--compdb` for the flags used to compile the target. |
| `--translation-unit FILE`, `--module ELF` | Select the exact compilation command and narrow ELF resolution respectively. |
| `--compile-option ARG` | Append one target compilation argument. Repeat in order. |
| `--extra-arg ARG`, `--extra-args FLAGS` | Pass one extra argument or a list. Lists can use backtick shell expansion. Probe compiler settings are separate from local C++ interpreter settings. |
| `--cwd DIR` | Resolve supported relative paths against a per-command working directory. |
| `--prepared DIR`, `--prepared-mode off\|prefer\|require` | Use a local prepared package. The default is `prefer` when a package is specified or was prepared in this REPL, and `off` otherwise. `off` uses live resolution and `require` fails if the package cannot be used. |
| `--stack-depth N\|full`, `--stack-application-only true\|false` | Deployment only. Request entry stack capture. Zero disables automatic capture. `full` follows the supported unwind chain. The default frame filter shows application frames. |
| `--print-arguments`, `--print-result` | Deployment only. Include automatically captured arguments and result in events. Explicit callback fields remain independently available. |

## Native REPL memory and C++ commands

| Command and example | Options and behavior |
| --- | --- |
| `%memory maps` | List memory ranges and permissions and obtain a memory snapshot token. |
| `%memory read 0x404000 --mapping TOKEN --type u32` | Read from a verified readable mapping. Choose `--count BYTES` for hex output (up to 65,536 bytes), or `--type` with `u8/u16/u32/u64`, `i8/i16/i32/i64`, `f32`, or `f64` for one little-endian value. |
| `%memory scan 0x404000 --mapping TOKEN --count 64 --pattern "2a ??" --limit 4` | Scan a readable range of up to 1 MiB for hex bytes. `?` matches a wildcard nibble. `--limit N` defaults to 64 results and is capped at 1024. |
| `%cpp load analysis.cpp` | Evaluate a local C++ file in the current interpreter. |
| `%cpp reset` | Reset C++ declarations, variables and background capture handles. Active tracing sessions stay attached. |
| `%cpp import model.hpp --type app::Order` | Generate, cache and load record mappings from the header's AST. Repeat `--type TYPE` to select records. `--output FILE` writes the generated header. |
| `%cpp generate model.hpp --output mappings.hpp` | Generate reusable mappings. Repeat `--type TYPE` to select records. Use `--output FILE` to save. |
| `%cpp options` | Print the effective local compiler flags. |
| `%cpp options add "-I/opt/app/include" "-DFEATURE=1"` | Add flags before first C++ input. Quote flags containing spaces. Backticks can add arguments from shell output. |
| `%cpp options clear` | Clear custom flags. Reset the interpreter first if it has already evaluated C++ input. |
| `%cpp options compdb build/compile_commands.json --file src/target.cpp` | Load flags from the selected database and source entry, with same-directory fallback. |
| `%shell "pwd"` | Run a local shell command and display output. |
| `%complete "tracer."` | List semantic completions for a C++ prefix without evaluating it. |
| `%type tracer.events_async` | Show the C++ type or available signatures. |
| `%help api Event` | Show a command or API topic. `%help` lists commands and `%help api` lists API topics. |
| `%quit` or `%exit` | Exit, detach sessions owned by this REPL, and stop its private server. |

## Native REPL capture and recording commands

| Command and example | Options and behavior |
| --- | --- |
| `%events --count 2 --as live` | Print events as they arrive and retain the `live` collection by default. Accepts stream options. |
| `%collect --count 2 --as sample` | Retain events with a capture summary. Defaults to collection `batch`. Accepts stream options. |
| `%record capture.db --sqlite --count 2` | Record to a new framed Protobuf file by default. With `--sqlite`, create or append to SQLite and save available prepared C++ types. Accepts stream options except `--as`. |
| `%events list sample --limit 10` | List indexed summaries. `--limit N` defaults to 20. |
| `%events show sample --index 1` | Print one event and its values by zero-based index. |
| `%events stack sample --index 0` | Print one event's full retained stack and capture status. |
| `%events calls sample` | Pair PRE with POST or EXCEPTION, report durations and identify missing events. |
| `%events stats sample` | Aggregate call counts, duration buckets, caller edges and loss per probe. |
| `%events clear sample` | Release the named collection's buffer allocation. |
| `%events open capture.db --as replayed` | Replay framed Protobuf or SQLite into a new named bounded collection. SQLite can load stored C++ types and mappings. |
| `%events export replayed --format json --output capture.json` | Write a new JSON export. JSON is the supported export format. |

| Stream option | Meaning |
| --- | --- |
| `--count N`, `--timeout SEC` | Stop after N matching events or at the deadline. Without a count, streams normally run until Ctrl+C. Counted streams default to a 30-second deadline. |
| `--name NAME`, `--after CURSOR` | Match an exact selector, linkage name or custom event name and begin strictly after the cursor. Repeat names for an OR filter. |
| `--session ID`, `--all` | Use one named session or every session. Omit both to use the selected session. |
| `--probe ID`, `--phase PRE\|POST\|EXCEPTION` | Narrow by probe ID or phase in the client. |
| `--as NAME` | Name the retained live collection. Valid on `%events` and `%collect`. |

Ctrl+C cancels active capture and preserves partial results. Trigger new calls from the target terminal while a synchronous capture waits. The [native REPL walkthrough](../native-repl.md) explains replay filters, stack attribution, retention and mapping details.

## Native server

```bash
clang-tracer-server --listen 127.0.0.1:50051 --event-history-limit 8192
```

Without `--listen`, the server creates a private Unix endpoint. Use the reported endpoint to connect. Keep TCP tracing endpoints on trusted local interfaces because the native transport uses insecure gRPC. Stop the server with Ctrl+C after detaching.

| Option | Meaning |
| --- | --- |
| `--help` | Show server usage. |
| `--agent LIBRARY` | Use this injected agent. Lookup then falls back to `CLANG_TRACER_AGENT`, the installed library, or the adjacent build library. |
| `--listen ENDPOINT` | Bind a Unix socket (`unix:///path/to/socket`) or loopback host and port. |
| `--timeout SEC` | Native operation timeout, default 10 seconds. |
| `--context-durable-dir DIR` | Keep resumable shared-context journals in this directory. |
| `--event-history-limit N` | Retain up to N events, default 4096. |
| `--context-history-limit N` | Retain up to N context events, default 128. |
| `--context-history-bytes N` | Retained serialized context bytes, default 16 MiB. |
| `--context-event-bytes N` | Shared event byte budget, default 256 KiB. |
| `--context-transport-memory-bytes N` | Total shared transport memory budget, default 64 MiB. |

Set `CLANG_TRACER_AGENT_LOG` in the target application's environment to enable per-session agent logs. See [callback diagnostics](developer.md#agent-callback-api).

## Native command-line client

Use `--endpoint ENDPOINT` to select the server. The default is `CLANG_TRACER_ENDPOINT` or `127.0.0.1:50051`. `--help` and `-h` print usage. Direct commands support `--format text|protobuf` and `--output FILE` where applicable. Text is the default and output normally goes to stdout. The binary format uses a four-byte little-endian length before each Protobuf message. SQLite output has its own file option.

| Command and example | Behavior |
| --- | --- |
| `clang-tracer attach 12345` | Attach by PID. |
| `clang-tracer list` | List sessions and probes. |
| `clang-tracer resolve 12345 "add(int, int)"` | Resolve a callable. Accepts resolution options below. |
| `clang-tracer validate 12345 "add(int, int)" --callback hook.cpp --level compile` | Check callback source. `--level syntax` parses the callback and `compile` also checks code generation and symbols. Accepts resolution options. |
| `clang-tracer trace 12345 "add(int, int)" --callback hook.cpp --stack 16` | Install a callback using DWARF. Accepts resolution and trace options. |
| `clang-tracer trace 12345 --function _Z3addii --return int32 --arg int32 --arg int32 --callback legacy.cpp` | Trace a raw ELF symbol with an explicit ABI. Use `--method NAME` for a supported non-static method or `--function NAME` for a free function. Omit explicit types to resolve the named symbol. |
| `clang-tracer events 12345 --count 2 --timeout 30` | Stream events for the attached PID. Use `--sqlite capture.db` to save while preserving server history. |
| `clang-tracer drain 12345 --sqlite capture.db` | Commit retained events to SQLite and then acknowledge them. Omit PID for all sessions. Add `--listen` to keep draining future events. |
| `clang-tracer remove 12345 1` | Restore one probe by ID. |
| `clang-tracer detach 12345` | Restore all probes and detach the process. |

| Option | Meaning |
| --- | --- |
| `--include HEADER` | Repeat for server-side headers. |
| `--compdb FILE_OR_DIR` | Use a compilation database for the headers. Requires `--include`. |
| `--module PATH` | Constrain the image used for resolution. |
| `--debug-max-entries N`, `--debug-max-depth N` | Bound DWARF traversal. Zero means unlimited. |
| `--compile-option ARG` | Append one compiler flag. Repeat to pass more flags. |
| `--callback FILE` | Read C++ source from the client filesystem. |
| `--return TYPE`, `--arg TYPE` | Specify raw ABI types. Arguments are ordered, up to eight. Valid types are `bool`, `int32`, `int64`, `uint64`, `float32`, `float64`, and `pointer`. Return type can also be `void`. |
| `--stack N\|full` | Capture entry stacks to the given depth or through the supported unwind chain. |
| `--stack-application-only true\|false` | Filter frames to application code by default. |
| `--callback-print-arguments true\|false`, `--callback-print-result true\|false` | Include automatic argument or result snapshots. Both default to false. |
| `--count N`, `--after SEQUENCE`, `--timeout SEC` | Limit event count, resume strictly after a cursor, or set a deadline. Unary operations default to 30 seconds and streams are open-ended unless a deadline is supplied. |
| `--context-field path[,path...]` | Select shared-state field paths for text rendering. Repeat or supply a comma-separated list. Omit to show the union of retained fields. |

Legacy mode reads a request from a Protobuf text file with `--request FILE`. Output is framed Protobuf and can be written to `--output FILE`. Use `--timeout SEC` to override the 30-second deadline. Supply `--count N` for `watch`. The RPC reference lists the request and response types.

```bash
printf 'pid: 12345\n' > attach.textproto
clang-tracer --request attach.textproto --output session.pb attach
clang-tracer --request add.textproto --output probe.pb add
clang-tracer --request remove.textproto --output removed.pb remove
clang-tracer --request detach.textproto --output detached.pb detach
clang-tracer --request list.textproto --output sessions.pb list
clang-tracer --request watch.textproto --count 2 --output events.pb watch
```

## Python console startup and server commands

```bash
clang-tracer-console --endpoint 127.0.0.1:50051 --timeout 45 --color auto
python -m clang_tracer --help
```

`--endpoint` specifies a server and defaults to the environment or loopback. Omit it and use `server start` to create an owned server. `--timeout` sets a unary RPC deadline (default 30 seconds). `--color auto|always|never` selects a color policy. Auto respects terminal detection and `NO_COLOR`. `--help` and `-h` show startup options.

| Command and example | Options and behavior |
| --- | --- |
| `server start --timeout 45 --event-history-limit 8192` | Start a private local server. `--executable PATH` and `--agent PATH` select binaries. `--timeout SEC` sets the readiness deadline (default 30) and `--operation-timeout SEC` sets the native deadline (default 10, range 1–300). `--durable-directory PATH` enables durable context recording. Supports the five history and transport budget flags in the native server reference. |
| `server status` | Show endpoint readiness and ownership. |
| `server stop --timeout 45` | Gracefully stop the owned server. The shutdown deadline defaults to 30 seconds. Retry if the child is still shutting down. |
| `attach 12345` | Attach and select the process's session. |
| `sessions` | List sessions and probes. |
| `use s1` | Switch to an existing session. |
| `modules` | List eligible application ELF paths. |
| `symbols --filter "add(" --limit 20` | List DWARF callables. `--filter TEXT` and `--module TEXT` match case-sensitive substrings. `--limit N` sets the page size (zero is unbounded) and `--page TOKEN` resumes it. |
| `detach s1` | Restore and detach this session. Omit the session to detach the selected one. |
| `probe resolve "add(int, int)"` | Resolve using DWARF. Accepts Python resolution options. |
| `probe validate "add(int, int)" --callback hook.cpp --level compile` | Compile and check a local callback for the target. `--level syntax` selects syntax-only validation. Accepts Python resolution options. |
| `probe inject "add(int, int)" --callback hook.cpp --stack full --print-result` | Resolve and install a callback. `--stack N\|full` captures entry stacks. `--internal-frames` includes runtime and JIT frames. `--print-arguments` and `--print-result` include snapshots. `--package SERVER_PATH` selects a shared-context package lineage. Accepts Python resolution options. |
| `probe add probe.textproto` | Install an explicit `ProbeSpec` from a local Protobuf text file. |
| `probe remove 1` | Restore the original target entry for this probe. |
| `events --count 2 --timeout 30` | Stream events until a count, deadline, or Ctrl+C. Accepts console stream options. |
| `collect 2 --name "add(int, int)"` | Stream and display events. Optional positional count limits the capture. Accepts console stream options. |
| `record capture.db 2 --sqlite` | Save a required positive count to a new framed Protobuf file or append to SQLite. Accepts console stream options. Default deadline 30 seconds. |
| `drain capture.db --listen --count 10` | Commit to SQLite then acknowledge shared history. `--listen` includes future events. Omit it to drain the initial snapshot. `--count N` limits writes and acknowledgements. Accepts console stream options. |
| `help probe inject` | Show help for a topic. Omit topic for the index. `help colors` describes color options. |
| `! pwd` | Run the rest of the line in a local `/bin/sh`. Working-directory changes stay in that shell. |
| `exit` or `quit` | Exit and stop any server this console owns. |

Python resolution options are repeatable `--include HEADER` and `--compile-option ARG`, plus `--compdb PATH`, `--module TEXT`, `--debug-max-entries N`, and `--debug-max-depth N`. The headers and database are on the server. Zero DWARF budgets mean unlimited traversal.

Console stream options are repeatable `--session ID` and `--name NAME`, plus `--pid PID`, `--after SEQUENCE`, and `--timeout SECONDS`. The default session filter is the selected session, or all sessions when none is selected. Names match an exact selector, linkage name or emitted context event name. Ctrl+C interrupts the stream and returns to the prompt. An expired cursor reports `OUT_OF_RANGE`.

## Offline probe preparation

```bash
clang-tracer-prepare-injectors-native --selector "add(int, int)" \
  --include add.hpp --compdb build/compile_commands.json \
  --translation-unit target.cpp --output prepared
```

| Option | Meaning |
| --- | --- |
| `--help` | Show usage. |
| `--selector SELECTOR` | Required exact C++ selector. |
| `--include HEADER` | Required declaration header. Repeat for supporting types. |
| `--compdb FILE_OR_DIR` | Required compilation database. |
| `--translation-unit FILE` | Select an explicit entry. Otherwise preparation tries the first include's recorded command, then an adjacent source. |
| `--output NEW_DIRECTORY` | Required new output directory. |
| `--module ELF` | Pin the package to an ELF build. |
| `--metadata-only` | Skip separate C++ adapter files. |
| `--extra-arg ARG` | Append one compiler argument. Repeat as needed. |

The REPL starts `clang-tracer-repl-check` internally for syntax checks, completion, reflection, and replay-type validation. The REPL's `%complete`, `%type`, `%cpp generate`, and recording commands manage that protocol.

## Build and deployment scripts

```bash
./build-rhel.sh --build-dir build-rhel --toolchain-root /opt/llvm-21 \
  --prefix /opt/clang-tracer --examples --package TGZ
./deploy-rhel.sh --build-dir build-rhel --destdir "$PWD/stage"
```

| Script and option | Meaning |
| --- | --- |
| Both scripts `--help`, `-h` | Print usage. |
| Both scripts `--build-dir DIR`, `--prefix DIR` | Select a build directory (default `build-rhel`) or installation root. Build defaults to `/usr/local`. Deploy defaults to the configured prefix. |
| Build `--toolchain-root DIR`, `--llvm-root DIR` | Choose compiler and LLVM packages together, or only the LLVM/Clang package prefix. |
| Build `--llvm-dir DIR`, `--clang-dir DIR` | Locate the respective CMake packages explicitly. |
| Build `--cc FILE`, `--cxx FILE`, `--toolchain-file FILE` | Set native compilers or a CMake toolchain file. |
| Build `--bindir DIR`, `--libdir DIR` | Override installed binary and library directories (defaults `bin` and `lib64`). |
| Build `--build-type TYPE`, `--jobs N` | Select CMake build type (default `RelWithDebInfo`) and parallel build jobs (default 2). |
| Build `--examples`, `--with-tests` | Build runnable example targets or test fixtures. Run tests separately with CTest. |
| Build `--package TGZ\|RPM` | Package the configured install tree. RPM packaging requires `rpmbuild`. |
| Build `-- CMAKE_ARGS` | Forward remaining arguments to CMake configuration. |
| Deploy `--destdir DIR` | Stage installation under a separate filesystem root. |

See [Build and deploy](../build-rhel.md) for resource headers, custom layouts, and verification.

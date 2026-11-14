# Command appendix

Choose a command for its syntax, behavior, and an example. Run percent commands in the native REPL. Replace uppercase placeholders with values from your run. Examples that change the target require a test process you control and the matching setup; follow the linked walkthroughs first. The syntax follows the C++17 REPL help. Use `%help "events diff"` for a multiword topic in the running REPL.

- **Connections and target control:** [`%server`](#command-server) · [`%server start`](#command-server-start) · [`%server connect`](#command-server-connect) · [`%server status`](#command-server-status) · [`%server stop`](#command-server-stop) · [`%attach`](#command-attach) · [`%launch`](#command-launch) · [`%children`](#command-children) · [`%resume`](#command-resume) · [`%terminate`](#command-terminate) · [`%call`](#command-call) · [`%call-result`](#command-call-result) · [`%processes`](#command-processes) · [`%sessions`](#command-sessions) · [`%use`](#command-use) · [`%status`](#command-status) · [`%detach`](#command-detach) · [`%quit`](#command-quit)
- **Modules and memory:** [`%modules`](#command-modules) · [`%library load`](#command-library-load) · [`%memory write`](#command-memory-write) · [`%memory allocate`](#command-memory-allocate) · [`%memory free`](#command-memory-free) · [`%memory patch`](#command-memory-patch) · [`%memory import-hook`](#command-memory-import-hook) · [`%memory restore`](#command-memory-restore) · [`%memory monitor-start`](#command-memory-monitor-start) · [`%memory monitor-stop`](#command-memory-monitor-stop) · [`%module`](#command-module) · [`%module index`](#command-module-index) · [`%module use`](#command-module-use) · [`%module types`](#command-module-types) · [`%module symbols`](#command-module-symbols) · [`%module show`](#command-module-show) · [`%module emit`](#command-module-emit) · [`%threads`](#command-threads) · [`%symbols`](#command-symbols) · [`%memory`](#command-memory) · [`%memory maps`](#command-memory-maps) · [`%memory read`](#command-memory-read) · [`%memory scan`](#command-memory-scan)
- **Probes and object watches:** [`%object`](#command-object) · [`%object prepare`](#command-object-prepare) · [`%object preview`](#command-object-preview) · [`%object start`](#command-object-start) · [`%object status`](#command-object-status) · [`%object stop`](#command-object-stop) · [`%trace`](#command-trace) · [`%trace preview`](#command-trace-preview) · [`%trace apply`](#command-trace-apply) · [`%trace remove`](#command-trace-remove) · [`%probes`](#command-probes) · [`%probe`](#command-probe) · [`%probe entry`](#command-probe-entry) · [`%probe prepare`](#command-probe-prepare) · [`%probe resolve`](#command-probe-resolve) · [`%probe validate`](#command-probe-validate) · [`%probe inject`](#command-probe-inject) · [`%probe remove`](#command-probe-remove) · [`%probe reload`](#command-probe-reload) · [`%interceptor`](#command-interceptor) · [`%interceptor load`](#command-interceptor-load) · [`%interceptor edit`](#command-interceptor-edit) · [`%interceptor show`](#command-interceptor-show) · [`%interceptor validate`](#command-interceptor-validate) · [`%interceptor upload`](#command-interceptor-upload)
- **Events and recordings:** [`%events`](#command-events) · [`%events list`](#command-events-list) · [`%events show`](#command-events-show) · [`%events diff`](#command-events-diff) · [`%events stack`](#command-events-stack) · [`%events calls`](#command-events-calls) · [`%events stats`](#command-events-stats) · [`%events clear`](#command-events-clear) · [`%events open`](#command-events-open) · [`%events export`](#command-events-export) · [`%collect`](#command-collect) · [`%record`](#command-record)
- **Local diagnostics:** [`%syscalls`](#command-syscalls) · [`%faults`](#command-faults) · [`%sample allocations`](#command-sample-allocations) · [`%sample cpu`](#command-sample-cpu) · [`%watchpoint`](#command-watchpoint) · [`%instructions start`](#command-instructions-start) · [`%instructions replay`](#command-instructions-replay) · [`%instructions stop`](#command-instructions-stop)
- **C++ and console tools:** [`%cpp`](#command-cpp) · [`%cpp load`](#command-cpp-load) · [`%cpp reset`](#command-cpp-reset) · [`%cpp import`](#command-cpp-import) · [`%cpp generate`](#command-cpp-generate) · [`%cpp options`](#command-cpp-options) · [`%cpp options add`](#command-cpp-options-add) · [`%cpp options clear`](#command-cpp-options-clear) · [`%cpp options compdb`](#command-cpp-options-compdb) · [`%shell`](#command-shell) · [`%complete`](#command-complete) · [`%type`](#command-type) · [`%help`](#command-help)
- **[Shell commands](#shell-commands):** [REPL](#start-the-repl) · [server](#start-the-server) · [client](#native-client) · [module index](#standalone-module-index) · [probe preparer](#offline-probe-preparer) · [object watch](#standalone-object-watch) · [build scripts](#build-and-deploy)

## Connections and target control

### Command server

Start, connect to, inspect, or stop a tracing server.

**Subcommands:** [`%server start`](#command-server-start) · [`%server connect`](#command-server-connect) · [`%server status`](#command-server-status) · [`%server stop`](#command-server-stop)

**Example**

```plain
%server start
```

### Command server start

Start a private local server and connect to it.

**Usage**

```plain
%server start [--executable PATH] [--agent PATH] [--context-event-bytes N] [--context-transport-memory-bytes N]
```

**Example**

```plain
%server start
```

### Command server connect

Connect to an existing server after detaching and stopping any server you own.

**Usage**

```plain
%server connect ENDPOINT
```

**Example**

```plain
%server connect 127.0.0.1:50051
```

### Command server status

Show the endpoint, ownership, and readiness.

**Usage**

```plain
%server status
```

**Example**

```plain
%server status
```

### Command server stop

Stop the server started by this REPL and restore its targets.

**Usage**

```plain
%server stop
```

**Example**

```plain
%server stop
```

### Command attach

Attach to a target. Omit PID for the local picker, or supply the observed start time with a PID.

**Usage**

```plain
%attach [PID [--start-time TICKS]]
```

**Example**

```plain
%attach 12345 --start-time 98765
```

### Command launch

Launch a local C++ application and attach at its startup checkpoint. Repeat --arg and --env.

**Usage**

```plain
%launch EXECUTABLE [--arg VALUE] [--env NAME=VALUE] [--cwd DIR]
  [--stdin FILE] [--stdout NEW_FILE] [--stderr NEW_FILE] [--timeout SECONDS] [--preload GATE.so] [--supervise] [--gate-children]
```

**Example**

```plain
%launch ./target --stdout target.log
```

[Owned launch setup](../native-repl.md#launch-a-local-application).

### Command children

List paused exec descendants of owned launches with --gate-children.

**Usage**

```plain
%children
```

**Example**

```plain
%children
```

### Command resume

Release the startup checkpoint of an attached owned target or a gated exec child.

**Usage**

```plain
%resume [PID]
```

**Example**

```plain
%resume
```

### Command terminate

Kill and reap a target launched by this REPL.

**Usage**

```plain
%terminate [PID]
```

**Example**

```plain
%terminate
```

### Command call

Invoke one explicitly registered function on a paused target's startup thread. Repeat --arg.

**Usage**

```plain
%call NAME [--arg SCALAR] [--timeout SECONDS]
```

**Example**

```plain
%call add --arg 19 --arg 23
```

[Registered target calls](api-reference.md#launch-checkpoint).

### Command call-result

Collect the result of a target call after the wait timed out or was interrupted.

**Usage**

```plain
%call-result [--timeout SECONDS]
```

**Example**

```plain
%call-result --timeout 5
```

### Command processes

List local target processes that match the filter.

**Usage**

```plain
%processes [--filter TEXT]
```

**Example**

```plain
%processes --filter target
```

### Command sessions

List sessions known to the connected server.

**Usage**

```plain
%sessions
```

**Example**

```plain
%sessions
```

### Command use

Select an attached session for subsequent commands.

**Usage**

```plain
%use SESSION
```

**Example**

```plain
%use s1
```

### Command status

Show connection details, the selected session, and retained collections.

**Usage**

```plain
%status
```

**Example**

```plain
%status
```

### Command detach

Detach the selected or named session and remove its probes.

**Usage**

```plain
%detach [SESSION]
```

**Example**

```plain
%detach s1
```

### Command quit

Exit, detach sessions owned by this REPL, and stop its private server.

**Usage**

```plain
%quit | %exit
```

**Example**

```plain
%quit
```

Alias: `%exit`.

## Modules and memory

### Command modules

List application modules in the selected process. The filter matches a case-sensitive path substring. Details show executable mappings, file identity, and a mapping token. Use --elf to inspect ELF imports, exports and sections or --watch to report mapping changes.

**Usage**

```plain
%modules [[--filter TEXT] [--details] | --elf PATH | --watch [--count N] [--timeout SECONDS]]
```

**Example**

```plain
%modules --filter libdemo.so --details
```

### Command library load

Load a trusted Linux x86-64 shared object into the selected target until it exits.

**Usage**

```plain
%library load PATH
```

**Example**

```plain
%library load ./trusted-observer.so
```

### Command memory write

Write up to 256 verified bytes to a private writable mapping. Restore with %memory restore.

**Usage**

```plain
%memory write ADDRESS --mapping TOKEN --expect HEX --replace HEX
```

**Example**

```plain
%memory write ADDRESS --mapping TOKEN --expect 00 --replace 2a
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory allocate

Allocate up to 1 MiB of zeroed writable memory owned by the session.

**Usage**

```plain
%memory allocate BYTES
```

**Example**

```plain
%memory allocate 4096
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory free

Free a session allocation after restoring its patches.

**Usage**

```plain
%memory free ID
```

**Example**

```plain
%memory free ALLOCATION_ID
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory patch

Patch verified equal-width instructions at a function entry and retain original bytes for restoration.

**Usage**

```plain
%memory patch --module PATH --elf-address ADDRESS --mapping TOKEN --expect HEX --replace HEX
```

**Example**

```plain
%memory patch --module ./target --elf-address ELF_ADDRESS --mapping TOKEN --expect EXPECTED_HEX --replace REPLACEMENT_HEX
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory import-hook

Redirect one resolved ELF import slot to an ABI-compatible function in the main executable.

**Usage**

```plain
%memory import-hook --module PATH --symbol NAME --original ADDRESS --address ADDRESS --mapping TOKEN --abi linux-x86-64-sysv
```

**Example**

```plain
%memory import-hook --module ./target --symbol puts --original ORIGINAL --address REPLACEMENT --mapping TOKEN --abi linux-x86-64-sysv
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory restore

Restore original executable or import-slot bytes.

**Usage**

```plain
%memory restore ID
```

**Example**

```plain
%memory restore PATCH_ID
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory monitor-start

Record the nominated thread's first read or write to each page of a dedicated mapping.

**Usage**

```plain
%memory monitor-start --tid TID --address ADDRESS --size BYTES --mapping TOKEN [--timeout SECONDS]
```

**Example**

```plain
%memory monitor-start --tid TID --address ADDRESS --size 4096 --mapping TOKEN --timeout 5
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory monitor-stop

Restore page protections and print first accesses.

**Usage**

```plain
%memory monitor-stop
```

**Example**

```plain
%memory monitor-stop
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command module

Index ELF modules and browse or export recovered types and symbols.

**Subcommands:** [`%module index`](#command-module-index) · [`%module use`](#command-module-use) · [`%module types`](#command-module-types) · [`%module symbols`](#command-module-symbols) · [`%module show`](#command-module-show) · [`%module emit`](#command-module-emit)

**Example**

```plain
%module index --file ./target --output target.sqlite
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command module index

Create a reusable module index and select it for the current REPL. File input works offline.

**Usage**

```plain
%module index (--file ELF | --module PATH --mapping TOKEN) [--output INDEX] [--debug-file FILE] [--max-entries N] [--max-depth N]
```

**Example**

```plain
%module index --file ./target --output target.sqlite
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command module use

Select an existing index for module commands and probe preparation.

**Usage**

```plain
%module use INDEX
```

**Example**

```plain
%module use target.sqlite
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command module types

Find indexed types and report their sizes when available.

**Usage**

```plain
%module types [--index INDEX] [--filter GLOB] [--limit N] [--page TOKEN] [--format json]
```

**Example**

```plain
%module types --index target.sqlite --filter "demo::*" --limit 10
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command module symbols

Find ELF symbols and distinguish exported definitions.

**Usage**

```plain
%module symbols [--index INDEX] [--filter GLOB] [--exported] [--limit N] [--page TOKEN] [--format json]
```

**Example**

```plain
%module symbols --index target.sqlite --filter "demo::*" --exported
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command module show

Show detailed metadata for a type or symbol.

**Usage**

```plain
%module show [--index INDEX] (--type NAME | --symbol SELECTOR | --id ID)
```

**Example**

```plain
%module show --symbol "add(int, int)"
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command module emit

Emit recovered C++ declarations, read-only views and a manifest.

**Usage**

```plain
%module emit [--index INDEX] [--type NAME | --symbol SELECTOR | --id ID]... --output DIR
```

**Example**

```plain
%module emit --symbol "add(int, int)" --output recovered-add
```

[Module-index lesson](user.md#index-debug-information-and-prepare-a-probe-from-a-binary).

### Command threads

List the selected target's thread IDs, names, and Linux start times.

**Usage**

```plain
%threads
```

**Example**

```plain
%threads
```

### Command symbols

Find supported C++ callables. Use --addresses for verified entries or --address with the mapping token to look up a PC.

**Usage**

```plain
%symbols [--filter TEXT] [--module TEXT] [--limit N] [--page TOKEN]
  [--addresses] [--mapping TOKEN] [--address PC]
```

**Example**

```plain
%symbols --filter add --limit 20 --addresses
```

### Command memory

List, read, or scan mapped memory.

**Subcommands:** [`%memory write`](#command-memory-write) · [`%memory allocate`](#command-memory-allocate) · [`%memory free`](#command-memory-free) · [`%memory patch`](#command-memory-patch) · [`%memory import-hook`](#command-memory-import-hook) · [`%memory restore`](#command-memory-restore) · [`%memory monitor-start`](#command-memory-monitor-start) · [`%memory monitor-stop`](#command-memory-monitor-stop) · [`%memory maps`](#command-memory-maps) · [`%memory read`](#command-memory-read) · [`%memory scan`](#command-memory-scan)

**Example**

```plain
%memory write ADDRESS --mapping TOKEN --expect 00 --replace 2a
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory maps

List all mapped ranges and permissions in the selected session and issue a memory snapshot token.

**Usage**

```plain
%memory maps
```

**Example**

```plain
%memory maps
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory read

Read up to 65536 bytes as a hex dump, or decode one little-endian u8/u16/u32/u64, i8/i16/i32/i64, f32 or f64 value.

**Usage**

```plain
%memory read ADDRESS --mapping TOKEN (--count BYTES | --type TYPE)
```

**Example**

```plain
%memory read 0x404000 --mapping TOKEN --type u32
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

### Command memory scan

Find masked byte patterns in a readable range up to 1 MiB. Use ? for wildcard nibbles. The default result limit is 64 and the maximum is 1024.

**Usage**

```plain
%memory scan ADDRESS --mapping TOKEN --count BYTES --pattern PATTERN [--limit N]
```

**Example**

```plain
%memory scan 0x404000 --mapping TOKEN --count 64 --pattern "2a ??" --limit 4
```

[Memory controls and restrictions](cpp17.md). Obtain fresh mapping tokens before operating on live addresses.

## Probes and object watches

### Command object

Watch object lifetimes, ownership, method snapshots and stores on Linux.

**Subcommands:** [`%object prepare`](#command-object-prepare) · [`%object preview`](#command-object-preview) · [`%object start`](#command-object-start) · [`%object status`](#command-object-status) · [`%object stop`](#command-object-stop)

**Example**

```plain
%object prepare demo::PlanRecord --module ./build/repl-object-plan-target --field value --code "demo::write_value(demo::PlanRecord &)" --backend code-cache --output record-watch
```

[Object-watch lesson](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) and [method snapshots](advanced.md#watch-logical-state-at-method-boundaries).

### Command object prepare

Save a reusable object definition and verified ELF bindings without attaching. Use --on METHOD, --include HEADER, and --synchronization for a typed method watch.

**Usage**

```plain
%object prepare TYPE --module ELF --field FIELD --code SELECTOR --output DIRECTORY
```

**Example**

```plain
%object prepare demo::PlanRecord --module ./build/repl-object-plan-target --field value --code "demo::write_value(demo::PlanRecord &)" --backend code-cache --output record-watch
```

Use a fresh plan directory for prepare. Preview and start accept a saved plan; method plans use `--on METHOD --include HEADER --policy HEADER --synchronization MODE` at preparation.

[Object-watch lesson](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) and [method snapshots](advanced.md#watch-logical-state-at-method-boundaries).

### Command object preview

Resolve the plan against the selected process and check live code bytes without installing patches.

**Usage**

```plain
%object preview --plan DIRECTORY
```

**Example**

```plain
%object preview --plan record-watch
```

Use a fresh plan directory for prepare. Preview and start accept a saved plan; method plans use `--on METHOD --include HEADER --policy HEADER --synchronization MODE` at preparation.

[Object-watch lesson](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) and [method snapshots](advanced.md#watch-logical-state-at-method-boundaries).

### Command object start

Start a native watch with matching DWARF. Ptrace is the diagnostic default. Code cache translates supported writers and lifecycle fragments. Repeat --code to select multiple writers for a standalone main-image field. Use --follow-calls to resolve callees at runtime from the selected scope, with --runtime-limit N to bound translations. Use --shared-file PATH to correlate shared storage across live processes. Use --pointee PATH with a verified --mutation to follow a DWARF-resolved pointer path. On AArch64, --rendezvous selects an existing single-threaded userspace checkpoint. On x86-64, use --module for a shared writer and --caller to verify an Image::get borrow followed by an indirect call. Use --writer-entry for all entries into an admitted standalone leaf. Use --vector with --element-key FIELD=UINT32 or --map with --map-key KEY to track collection storage generations. Select a verified --mutation SYMBOL and declare --synchronization for native collection capture. Use --owner-add, --owner-get, --owner-remove and --owner-snapshot to select ownership boundaries. Use --no-values to record every completed write and omit field samples. Record observations with %record FILE --sqlite --name object.trace.

**Usage**

```plain
%object start --type TYPE --field FIELD --code SELECTOR [--base BASE --owner OWNER --collection FIELD] [--snapshot-field FIELD] [--module MODULE] [--caller SELECTOR] [--rendezvous SYMBOL] [--backend ptrace|code-cache] [--no-values] [--stack-depth N] [--max-elements N] [--max-string-bytes N]
```

**Example**

```plain
%object start --plan record-watch --stack-depth 4
```

Use a fresh plan directory for prepare. Preview and start accept a saved plan; method plans use `--on METHOD --include HEADER --policy HEADER --synchronization MODE` at preparation.

[Object-watch lesson](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) and [method snapshots](advanced.md#watch-logical-state-at-method-boundaries).

### Command object status

Show admitted store sites, observations and restoration status.

**Usage**

```plain
%object status [WATCH_ID]
```

**Example**

```plain
%object status WATCH_ID
```

Check restoration status before detaching. Retry stop with the same ID if restoration is pending.

[Object-watch lesson](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) and [method snapshots](advanced.md#watch-logical-state-at-method-boundaries).

### Command object stop

Restore the watch's patches and release execution control.

**Usage**

```plain
%object stop [WATCH_ID]
```

**Example**

```plain
%object stop WATCH_ID
```

Check restoration status before detaching. Retry stop with the same ID if restoration is pending.

[Object-watch lesson](tutorials.md#lesson-17-trace-writes-without-rebuilding-the-target) and [method snapshots](advanced.md#watch-logical-state-at-method-boundaries).

### Command trace

Preview, apply, or remove a bulk C++ trace.

**Subcommands:** [`%trace preview`](#command-trace-preview) · [`%trace apply`](#command-trace-apply) · [`%trace remove`](#command-trace-remove)

**Example**

```plain
%trace preview --exact "add(int, int)" --save trace.textproto
```

### Command trace preview

Preview bulk C++ matches, ABI status and retained probe capacity without installing probes.

**Usage**

```plain
%trace preview [--module GLOB] [--include GLOB] [--exclude GLOB] [--exact SIGNATURE]
  or %trace preview --manifest FILE
  [--save NEW_FILE]
```

**Example**

```plain
%trace preview --exact "add(int, int)" --save trace.textproto
```

### Command trace apply

Install a typed observer for each eligible manifest match. Restore newly installed probes on partial failure.

**Usage**

```plain
%trace apply --manifest FILE [--callback FILE]
```

**Example**

```plain
%trace apply --manifest trace.textproto --callback hook.cpp
```

### Command trace remove

Remove or retry restoration of the probes owned by a batch in the selected session.

**Usage**

```plain
%trace remove BATCH_ID
```

**Example**

```plain
%trace remove batch-1
```

### Command probes

List the probes in the selected session.

**Usage**

```plain
%probes [--cwd DIRECTORY]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Example**

```plain
%probes
```

### Command probe

Prepare, resolve, validate, install, or remove a probe.

**Subcommands:** [`%probe entry`](#command-probe-entry) · [`%probe prepare`](#command-probe-prepare) · [`%probe resolve`](#command-probe-resolve) · [`%probe validate`](#command-probe-validate) · [`%probe inject`](#command-probe-inject) · [`%probe remove`](#command-probe-remove) · [`%probe reload`](#command-probe-reload)

**Example**

```plain
%probe entry "add(int, int)" --mapping TOKEN
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe entry

Verify and disassemble a function entry and show the copy-only detour window and bounded control flow.

**Usage**

```plain
%probe entry SELECTOR --mapping TOKEN [--module MODULE] [--cwd DIRECTORY]
  or %probe entry --module PATH --elf-address ADDRESS --abi linux-x86-64-sysv --mapping TOKEN [--cwd DIRECTORY]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Example**

```plain
%probe entry "add(int, int)" --mapping TOKEN
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe prepare

Build a reusable probe package from source, an explicit signature, or the module index. Without headers or an explicit signature, preparation uses the default index. Repeat --include for extra headers. Preparation uses compile_commands.json in the command's working directory by default. It prefers the first include's recorded compile command and falls back to the first source beside it. Use --module to pin the package to a specific build. Prepared headers can declare tracer_snapshot::FollowPointers<Record> specializations listing direct raw pointer fields to follow; pointee types and polymorphic implementations are discovered from the included source inputs. This policy applies to prepared argument, receiver and result records. Use --snapshot-return-pointee when injecting a raw pointer-return probe. Use --metadata-only to skip the separate C++ adapter files. Pointer and reference members are followed automatically; --no-automatic-members follows none. Capture limits, each visible in the event when it cuts something: --follow-depth N (default 3, 0-40, prepare): levels of pointees followed below a captured record (0 follows none); --follow-members N (default 48, 0-unlimited, prepare): followed members per probe, policy-named ones first (about 160 bytes of wrapper stack each); --record-members N (default 32, 0-unlimited, prepare): automatically discovered pointer members per record; --nested-depth N (default 2, 0-unlimited, prepare): levels of by-value records and smart pointers searched for pointer members; --string-bytes N (default 4000, 0-65536, run time): bytes kept of one string in a followed member; bounded by the slice; --vector-elements N (default 4096, 0-unlimited, run time): elements of a vector in a followed member; a larger vector is not followed; --checked-reads N (default 256, 1-unlimited, run time): checked memory reads one followed member may spend (each is a system call); --snapshot-reserve N (default 8192, 0-unlimited, run time): bytes of the per-thread storage that members captured at entry leave for later roots; --snapshot-arena N (default 16384, 16384-unlimited, run time): per-thread storage for snapshots; only grows, the largest request of any probe wins; --snapshot-slice N (default 4096, 4096-65536, run time): bytes of one snapshot payload; only grows, the largest request of any probe wins; 'unlimited' selects the largest value (for --follow-depth, --string-bytes and --snapshot-slice that is a fixed maximum the tracer imposes), 'off' selects 0 where 0 is allowed. Prepare-time options shape the package; %probe inject accepts all of them and its value wins, but members that were not discovered at prepare time cannot be added. Each option is also read from the environment variable TRACER_PREPARE_<NAME> by the prepare tool; docs/native-repl.md lists them. With --signature, supply the C++ result and explicit parameter types, for example 'const int&()'. Qualified methods receive an opaque receiver. Pass --member for an unqualified instance method.

**Usage**

```plain
%probe prepare SELECTOR --include HEADER --output NEW_DIRECTORY
  [--compdb PATH] [--translation-unit FILE] [--module ELF] [--metadata-only] [--no-automatic-members] [--cwd DIRECTORY] [EXTRA COMPILER ARGUMENTS]
  [--follow-depth N] [--follow-members N] [--record-members N] [--nested-depth N]
  [--string-bytes N] [--vector-elements N] [--checked-reads N] [--snapshot-reserve N] [--snapshot-arena N] [--snapshot-slice N]
  or %probe prepare SELECTOR --signature TYPE --module ELF --output NEW_DIRECTORY [--member] [--metadata-only] [--cwd DIRECTORY]
  or %probe prepare SELECTOR [--module-index INDEX] [--module ELF] [--id ID] [--generated DIR] --output NEW_DIRECTORY
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Extra compiler arguments**

```plain
--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`
```

Repeat these options to add fallback include paths and new macro definitions. Compilation database settings take priority.
Set startup defaults with --extra-arg=ARG or --extra-args "$(my-conf --cxxflags)".

**Example**

```plain
%probe prepare "add(int, int)" --include add.hpp --output prepared
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe resolve

Resolve a selector in the selected target and show the available metadata.

**Usage**

```plain
%probe resolve SELECTOR [RESOLUTION OPTIONS] [--cwd DIRECTORY]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Resolution options**

```plain
--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]
--module TEXT --compile-option FLAG
--prepared DIRECTORY --prepared-mode off|prefer|require
[EXTRA COMPILER ARGUMENTS]
```

The default mode, prefer, uses an available prepared package and falls back to DWARF.
With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.
Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.

**Extra compiler arguments**

```plain
--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`
```

Repeat these options to add fallback include paths and new macro definitions. Compilation database settings take priority.
Set startup defaults with --extra-arg=ARG or --extra-args "$(my-conf --cxxflags)".

**Example**

```plain
%probe resolve "add(int, int)"
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe validate

Check whether the callback can be deployed for the selected function.

**Usage**

```plain
%probe validate SELECTOR --callback FILE [RESOLUTION OPTIONS] [--cwd DIRECTORY]
  [--allow-arg-overrides] [--allow-result-override] [--allow-replacement] [--allow-call-original] [--trace-exceptions] [--reclaimable]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Resolution options**

```plain
--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]
--module TEXT --compile-option FLAG
--prepared DIRECTORY --prepared-mode off|prefer|require
[EXTRA COMPILER ARGUMENTS]
```

The default mode, prefer, uses an available prepared package and falls back to DWARF.
With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.
Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.

**Extra compiler arguments**

```plain
--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`
```

Repeat these options to add fallback include paths and new macro definitions. Compilation database settings take priority.
Set startup defaults with --extra-arg=ARG or --extra-args "$(my-conf --cxxflags)".

**Example**

```plain
%probe validate "add(int, int)" --callback hook.cpp
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe inject

Use `--snapshot-receiver-at-return` on a supported prepared method to sample the receiver independently at POST for [%events diff](#command-events-diff). Omit --callback to capture events with the built-in observer. For prepared implementation member snapshots, a callback file can define tracer_snapshot::encode(const Implementation&) noexcept to return a custom record by value. Snapshot returned record pointees only when the live layout matches and the pointee remains stable through POST; result() gives its address and result_as<T>() copies the record.

**Usage**

```plain
%probe inject SELECTOR [--callback FILE] [RESOLUTION OPTIONS] [DEPLOYMENT OPTIONS] [--cwd DIRECTORY]
  [--allow-arg-overrides] [--allow-result-override] [--allow-replacement] [--allow-call-original] [--trace-exceptions] [--reclaimable] [--snapshot-return-pointee] [--backend auto|entry] [--caller SELECTOR]
  [--follow-depth N] [--follow-members N] [--record-members N] [--nested-depth N]
  [--string-bytes N] [--vector-elements N] [--checked-reads N] [--snapshot-reserve N] [--snapshot-arena N] [--snapshot-slice N]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Resolution options**

```plain
--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]
--module TEXT --compile-option FLAG
--prepared DIRECTORY --prepared-mode off|prefer|require
[EXTRA COMPILER ARGUMENTS]
```

The default mode, prefer, uses an available prepared package and falls back to DWARF.
With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.
Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.

**Extra compiler arguments**

```plain
--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`
```

Repeat these options to add fallback include paths and new macro definitions. Compilation database settings take priority.
Set startup defaults with --extra-arg=ARG or --extra-args "$(my-conf --cxxflags)".

**Deployment options**

```plain
--stack-depth N|full --stack-application-only true|false
--print-arguments --print-result --snapshot-return-pointee
```

**Example**

```plain
%probe inject "add(int, int)" --callback hook.cpp --stack-depth 16
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe remove

Remove a probe from the selected session and restore the original code.

**Usage**

```plain
%probe remove ID [--cwd DIRECTORY]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Example**

```plain
%probe remove PROBE_ID
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command probe reload

Switch an active probe to a new observer generation while existing calls finish on their original generation.

**Usage**

```plain
%probe reload ID --callback FILE [--cwd DIRECTORY] [--watch [--count N] [--timeout SECONDS]]
```

--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.
Server-side inputs must be available at the resulting paths on the server host.

**Example**

```plain
%probe reload PROBE_ID --callback updated.cpp
```

[Probe options and prerequisites](commands.md#native-repl-probe-and-bulk-trace-commands) and [advanced adapters](advanced.md).

### Command interceptor

Manage named C++ callback drafts.

**Subcommands:** [`%interceptor load`](#command-interceptor-load) · [`%interceptor edit`](#command-interceptor-edit) · [`%interceptor show`](#command-interceptor-show) · [`%interceptor validate`](#command-interceptor-validate) · [`%interceptor upload`](#command-interceptor-upload)

**Example**

```plain
%interceptor load hook.cpp --as observer
```

[Typed callback API](api-reference.md#interceptor-and-resolved-signature).

### Command interceptor load

Load a local C++ file into a named draft.

**Usage**

```plain
%interceptor load FILE --as NAME
```

**Example**

```plain
%interceptor load hook.cpp --as observer
```

[Typed callback API](api-reference.md#interceptor-and-resolved-signature).

### Command interceptor edit

Enter C++ source interactively. Use .end to save or .cancel to discard.

**Usage**

```plain
%interceptor edit NAME
```

**Example**

```plain
%interceptor edit observer
```

[Typed callback API](api-reference.md#interceptor-and-resolved-signature).

### Command interceptor show

Print the source of a named draft.

**Usage**

```plain
%interceptor show NAME
```

**Example**

```plain
%interceptor show observer
```

[Typed callback API](api-reference.md#interceptor-and-resolved-signature).

### Command interceptor validate

Compile and link the named draft for the selected target without activating a probe.

**Usage**

```plain
%interceptor validate NAME --target SELECTOR [RESOLUTION OPTIONS]
```

**Resolution options**

```plain
--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]
--module TEXT --compile-option FLAG
--prepared DIRECTORY --prepared-mode off|prefer|require
[EXTRA COMPILER ARGUMENTS]
```

The default mode, prefer, uses an available prepared package and falls back to DWARF.
With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.
Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.

**Extra compiler arguments**

```plain
--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`
```

Repeat these options to add fallback include paths and new macro definitions. Compilation database settings take priority.
Set startup defaults with --extra-arg=ARG or --extra-args "$(my-conf --cxxflags)".

**Example**

```plain
%interceptor validate observer --target "add(int, int)"
```

[Typed callback API](api-reference.md#interceptor-and-resolved-signature).

### Command interceptor upload

Install the named draft on the selected target.

**Usage**

```plain
%interceptor upload NAME --target SELECTOR [RESOLUTION OPTIONS] [DEPLOYMENT OPTIONS]
```

**Resolution options**

```plain
--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]
--module TEXT --compile-option FLAG
--prepared DIRECTORY --prepared-mode off|prefer|require
[EXTRA COMPILER ARGUMENTS]
```

The default mode, prefer, uses an available prepared package and falls back to DWARF.
With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.
Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.

**Extra compiler arguments**

```plain
--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`
```

Repeat these options to add fallback include paths and new macro definitions. Compilation database settings take priority.
Set startup defaults with --extra-arg=ARG or --extra-args "$(my-conf --cxxflags)".

**Deployment options**

```plain
--stack-depth N|full --stack-application-only true|false
--print-arguments --print-result --snapshot-return-pointee
```

**Example**

```plain
%interceptor upload observer --target "add(int, int)"
```

[Typed callback API](api-reference.md#interceptor-and-resolved-signature).

## Events and recordings

### Command events

Print live events and keep a bounded collection named live by default.

**Usage**

```plain
%events [STREAM OPTIONS] [--as NAME]
```

**Subcommands:** [`%events list`](#command-events-list) · [`%events show`](#command-events-show) · [`%events diff`](#command-events-diff) · [`%events stack`](#command-events-stack) · [`%events calls`](#command-events-calls) · [`%events stats`](#command-events-stats) · [`%events clear`](#command-events-clear) · [`%events open`](#command-events-open) · [`%events export`](#command-events-export)

**Stream options**

```plain
--count N --timeout SECONDS --after CURSOR --probe ID
--phase PRE|POST|EXCEPTION --name SELECTOR --session ID | --all
```

The selected session is the default. Without a count or timeout, press Ctrl+C to stop.

**Example**

```plain
%events --count 2 --as live
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events list

List retained events with their zero-based indexes. The default limit is 20.

**Usage**

```plain
%events list NAME [--limit N]
```

**Example**

```plain
%events list sample --limit 10
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events show

Print one retained event and its captured values.

**Usage**

```plain
%events show NAME --index N
```

**Example**

```plain
%events show sample --index 1
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events diff

Compare independently sampled receiver snapshots from PRE and POST of the same invocation. Inject the prepared method with `--snapshot-receiver-at-return` and collect both phases. Use `--key PATH=MEMBER` for a verified element identity.

**Usage**

```plain
%events diff NAME --pre N --post N [--key PATH=MEMBER]...
```

**Example**

```plain
%events diff sample --pre 0 --post 1 --key legs=id
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events stack

Print all captured stack frames for one retained event.

**Usage**

```plain
%events stack NAME --index N
```

**Example**

```plain
%events stack sample --index 0
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events calls

Pair PRE with POST or EXCEPTION and show call durations and missing events.

**Usage**

```plain
%events calls NAME
```

**Example**

```plain
%events calls sample
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events stats

Summarize live or replayed calls per probe, with duration histograms, caller edges and loss.

**Usage**

```plain
%events stats NAME
```

**Example**

```plain
%events stats sample
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events clear

Remove a named collection and release its buffer space.

**Usage**

```plain
%events clear NAME
```

**Example**

```plain
%events clear sample
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events open

Open a Protobuf or SQLite recording offline as a bounded, named collection. Compatible C++ types stored in SQLite load automatically.

**Usage**

```plain
%events open FILE --as NAME
```

**Example**

```plain
%events open capture.db --as replayed
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command events export

Write the retained events to a new JSON file.

**Usage**

```plain
%events export NAME --format json --output NEW_FILE.json
```

**Example**

```plain
%events export replayed --format json --output capture.json
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

### Command collect

Capture events into a bounded collection named batch by default.

**Usage**

```plain
%collect [STREAM OPTIONS] [--as NAME]
```

**Stream options**

```plain
--count N --timeout SECONDS --after CURSOR --probe ID
--phase PRE|POST|EXCEPTION --name SELECTOR --session ID | --all
```

The selected session is the default. Without a count or timeout, press Ctrl+C to stop.

**Example**

```plain
%collect --count 2 --as sample
```

### Command record

Record framed Protobuf by default. With --sqlite, create or append to a SQLite recording and save available prepared C++ types for replay.

**Usage**

```plain
%record FILE [--sqlite] [STREAM OPTIONS]
```

**Stream options**

```plain
--count N --timeout SECONDS --after CURSOR --probe ID
--phase PRE|POST|EXCEPTION --name SELECTOR --session ID | --all
```

The selected session is the default. Without a count or timeout, press Ctrl+C to stop.

**Example**

```plain
%record capture.db --sqlite --count 2
```

[Capture and replay options](commands.md#native-repl-capture-and-recording-commands).

## Local diagnostics

### Command syscalls

Capture Linux syscall entry and exit records on the selected local target's existing threads.

**Usage**

```plain
%syscalls [--count N] [--timeout SECONDS]
```

**Example**

```plain
%syscalls --count 8 --timeout 5
```

### Command faults

Observe delivered native fault signals without changing the target's handlers. Requires access to Linux tracefs.

**Usage**

```plain
%faults [--count N] [--timeout SECONDS]
```

**Example**

```plain
%faults --count 2 --timeout 10
```

### Command sample allocations

Profile glibc malloc, calloc and realloc requests on existing target threads with Linux perf uprobes.

**Usage**

```plain
%sample allocations [--count N] [--timeout SECONDS]
```

**Example**

```plain
%sample allocations --count 10 --timeout 5
```

### Command sample cpu

Sample user-space instruction addresses on the selected local target's existing threads.

**Usage**

```plain
%sample cpu [--count N] [--timeout SECONDS]
```

**Example**

```plain
%sample cpu --count 10 --timeout 5
```

### Command watchpoint

Collect one thread's hardware watchpoint hits on a naturally aligned 1, 2, 4 or 8 byte range.

**Usage**

```plain
%watchpoint --tid TID --address ADDRESS --size BYTES --mapping TOKEN --access write|read-write [--count N] [--timeout SECONDS]
```

**Example**

```plain
%watchpoint --tid TID --address ADDRESS --size 4 --mapping TOKEN --access write --count 2 --timeout 5
```

### Command instructions start

Trace one thread with optional general-purpose snapshots, a trusted native callback, or scalar memory-access filtering. --discover records executed call destinations instead. Use %memory maps for the token.

**Usage**

```plain
%instructions start --tid TID --address ADDRESS --size BYTES --mapping TOKEN [--count N] [--timeout SECONDS] [--registers] [--callback-library PATH] [--memory-address ADDRESS --memory-size BYTES] [--discover]
```

**Example**

```plain
%instructions start --tid TID --address ADDRESS --size 32 --mapping TOKEN --count 64 --timeout 5
```

[Instruction capture walkthrough](cpp17.md#capture-instructions-and-replay-them).

### Command instructions replay

Replay a saved bounded instruction capture without a target.

**Usage**

```plain
%instructions replay FILE
```

**Example**

```plain
%instructions replay instructions.pb
```

[Instruction capture walkthrough](cpp17.md#capture-instructions-and-replay-them).

### Command instructions stop

Stop and drain the selected session's capture, then print original instruction addresses and selected details.

**Usage**

```plain
%instructions stop [--save NEW_FILE]
```

**Example**

```plain
%instructions stop --save instructions.pb
```

[Instruction capture walkthrough](cpp17.md#capture-instructions-and-replay-them).

## C++ and console tools

### Command cpp

Load C++, configure the local compiler, or generate mappings for captured values.

**Subcommands:** [`%cpp load`](#command-cpp-load) · [`%cpp reset`](#command-cpp-reset) · [`%cpp import`](#command-cpp-import) · [`%cpp generate`](#command-cpp-generate) · [`%cpp options`](#command-cpp-options) · [`%cpp options add`](#command-cpp-options-add) · [`%cpp options clear`](#command-cpp-options-clear) · [`%cpp options compdb`](#command-cpp-options-compdb)

**Example**

```plain
%cpp load analysis.cpp
```

[Client API appendix](api-reference.md#client-api).

### Command cpp load

Evaluate a local C++ source file in the current interpreter.

**Usage**

```plain
%cpp load FILE
```

**Example**

```plain
%cpp load analysis.cpp
```

[Client API appendix](api-reference.md#client-api).

### Command cpp reset

Clear C++ variables, declarations, and background range handles. Tracing sessions stay attached.

**Usage**

```plain
%cpp reset
```

**Example**

```plain
%cpp reset
```

[Client API appendix](api-reference.md#client-api).

### Command cpp import

Generate, cache, and load mappings for typed event decoding. Repeat --type to select several records. Nested dependencies are included automatically. Import failures preserve earlier variables.

**Usage**

```plain
%cpp import HEADER [--type TYPE] [--output FILE]
```

**Example**

```plain
%cpp import model.hpp --type app::Order
```

[Client API appendix](api-reference.md#client-api).

### Command cpp generate

Generate mappings from the header's AST. Use --output to save a header for #include or %cpp load. Automatic mappings require supported public fields and accessible default construction.

**Usage**

```plain
%cpp generate HEADER [--type TYPE] [--output FILE]
```

**Example**

```plain
%cpp generate model.hpp --output mappings.hpp
```

[Client API appendix](api-reference.md#client-api).

### Command cpp options

List the effective local compiler arguments in order. Use --cpp-arg ARG at startup. Probe compilation uses separate options on the server.

**Usage**

```plain
%cpp options
```

**Subcommands:** [`%cpp options add`](#command-cpp-options-add) · [`%cpp options clear`](#command-cpp-options-clear) · [`%cpp options compdb`](#command-cpp-options-compdb)

**Example**

```plain
%cpp options
```

[Client API appendix](api-reference.md#client-api).

### Command cpp options add

Add compiler arguments before the first C++ input. Quote flags with spaces. Backticks run a shell command and add flags from its output.

**Usage**

```plain
%cpp options add ARG...
```

**Example**

```plain
%cpp options add "-I/opt/app/include" "-DFEATURE=1"
```

[Client API appendix](api-reference.md#client-api).

### Command cpp options clear

Clear custom local compiler arguments. Use %cpp reset first after C++ input.

**Usage**

```plain
%cpp options clear
```

**Example**

```plain
%cpp options clear
```

[Client API appendix](api-reference.md#client-api).

### Command cpp options compdb

Load flags from an exact compilation database entry or a C++ source in the same directory.

**Usage**

```plain
%cpp options compdb DATABASE --file FILE
```

**Example**

```plain
%cpp options compdb build/compile_commands.json --file src/target.cpp
```

[Client API appendix](api-reference.md#client-api).

### Command shell

Run a local shell command and print its output.

**Usage**

```plain
%shell "COMMAND"
```

**Example**

```plain
%shell "pwd"
```

### Command complete

List semantic C++ completions without evaluating the expression.

**Usage**

```plain
%complete "C++ PREFIX"
```

**Example**

```plain
%complete "tracer."
```

### Command type

Show the type or signatures of a symbol in the current C++ session.

**Usage**

```plain
%type SYMBOL
```

**Example**

```plain
%type tracer.events_async
```

### Command help

Browse commands and C++ APIs or look up a specific topic.

**Usage**

```plain
%help [COMMAND | api [NAME]]
```

**Example**

```plain
%help api Event
```

## Shell commands

### Start the REPL

Start a native session and attach to a PID. See [startup options](commands.md#native-repl-startup).

```bash
clang-tracer-repl --attach 12345 --buffer-events 8192
```

### Start the server

Run a standalone server on a trusted local endpoint; see [server options](commands.md#native-server).

```bash
clang-tracer-server --listen 127.0.0.1:50051 --event-history-limit 8192
```

### Native client

Use an existing server. For options, direct commands, and legacy request mode, see the [native CLI reference](commands.md#native-command-line-client).

[`attach`](#cli-attach) · [`list`](#cli-list) · [`resolve`](#cli-resolve) · [`validate`](#cli-validate) · [`trace`](#cli-trace) · [`events`](#cli-events) · [`drain`](#cli-drain) · [`remove`](#cli-remove) · [`detach`](#cli-detach) · [`add`](#cli-add) · [`watch`](#cli-watch)

#### CLI attach

Attach the server to a Linux process.

```bash
clang-tracer attach 12345
```

#### CLI list

List sessions and installed probes on the selected server.

```bash
clang-tracer list
```

#### CLI resolve

Resolve a C++ selector using the target’s debug data.

```bash
clang-tracer resolve 12345 "add(int, int)"
```

#### CLI validate

Check syntax or compile an observer without installing it.

```bash
clang-tracer validate 12345 "add(int, int)" --callback hook.cpp --level compile
```

#### CLI trace

Install an observer by a resolved selector or raw ABI.

```bash
clang-tracer trace 12345 "add(int, int)" --callback hook.cpp --stack 16
```

#### CLI events

Stream live events. SQLite output preserves server history.

```bash
clang-tracer events 12345 --count 2 --timeout 30 --sqlite capture.db
```

#### CLI drain

Commit retained events to SQLite before acknowledging them.

```bash
clang-tracer drain 12345 --sqlite capture.db
```

#### CLI remove

Restore original code for a probe ID.

```bash
clang-tracer remove 12345 1
```

#### CLI detach

Remove probes and detach from a target.

```bash
clang-tracer detach 12345
```

#### CLI add

Install a probe from a legacy AddProbe request.

```bash
clang-tracer --request add.textproto --output probe.pb add
```

#### CLI watch

Stream events with a legacy WatchRequest.

```bash
clang-tracer --request watch.textproto --count 2 --output events.pb watch
```

### Standalone module index

Run ELF/DWARF operations without an attached process. See [module tool options](commands.md#standalone-module-index-cli).

[`index`](#module-cli-index) · [`info`](#module-cli-info) · [`types`](#module-cli-types) · [`symbols`](#module-cli-symbols) · [`show`](#module-cli-show) · [`emit`](#module-cli-emit)

#### Module CLI index

Build an index at a new SQLite path.

```bash
clang-tracer-module index --file ./target --output target.sqlite
```

#### Module CLI info

Read build ID, input identity, and diagnostics.

```bash
clang-tracer-module info --index target.sqlite
```

#### Module CLI types

Browse recovered types.

```bash
clang-tracer-module types --index target.sqlite --filter int
```

#### Module CLI symbols

Browse symbols and optionally restrict to dynamic exports.

```bash
clang-tracer-module symbols --index target.sqlite --exported --limit 20
```

#### Module CLI show

Read a specific symbol or type record.

```bash
clang-tracer-module show --index target.sqlite --symbol "add(int, int)"
```

#### Module CLI emit

Write recovered headers and a manifest to a new path. Use an ID from the index.

```bash
clang-tracer-module emit --index target.sqlite --symbol SYMBOL_ID --output recovered
```

### Offline probe preparer

Create a package using source/compile flags, a verified index, or an explicit signature. See [preparer options](commands.md#offline-probe-preparation).

```bash
clang-tracer-prepare-injectors-native --selector "add(int, int)" --include add.hpp --compdb compile_commands.json --output prepared
```

### Standalone object watch

Takes a JSON definition and uses matching DWARF. See the [startup and CLI notes](../native-repl.md#observe-startup-before-main) for plan and target prerequisites.

[`preview`](#watch-cli-preview) · [`watch`](#watch-cli-watch) · [`launch`](#watch-cli-launch)

#### Watch CLI preview

Resolve the definition for an existing PID without installing a watch.

```bash
clang-tracer-object-watch preview 12345 definition.json
```

#### Watch CLI watch

Watch the already-running process.

```bash
clang-tracer-object-watch watch 12345 definition.json
```

#### Watch CLI launch

Launch from earliest supported startup boundaries. Agent and loader options depend on the selected backend.

```bash
clang-tracer-object-watch launch definition.json ./target
```

### Build and deploy

Configure and install a Linux build, or stage deployment locally. See the [build script flags](commands.md#build-and-deployment-scripts).

```bash
./build-rhel.sh --build-dir build-rhel --examples --with-tests
./deploy-rhel.sh --build-dir build-rhel --destdir "$PWD/stage"
```

# Clang Tracer Python client

The console supports `drain capture.db` to save and consume retained history,
or `drain capture.db --listen` to keep collecting generated events until Ctrl-C.
Use `--count N` or `--timeout SECONDS` to bound it. Existing stream filters apply;
the default is the selected session, or all sessions when none is selected.
`record capture.db 100 --sqlite` saves without consuming server history.

For larger history, launch with `server start --event-history-limit 16384
--context-history-limit 512 --context-history-bytes 67108864` on one line. Defaults
remain 4096 events, 128 context events, and 16 MiB of serialized context data.

The API equivalents are `Client.drain_traces(path, listen=True, timeout=60)` and
`Client.record_sqlite(path, count=100)`. `read_events()` snapshots a bounded batch;
`acknowledge_events(server_instance_id, sequences)` consumes exact stored IDs.
SQLite appends and deduplicates full Protobuf payloads. Writes commit before
drain acknowledgement; retry a failed drain into the same database. Consumption
affects all readers, and overflow can still lose events before storage. See
[event storage](../docs/event-storage.md) for the schema and failure semantics.

The Python API covers every public `Tracer` gRPC operation. The interactive
console uses that same API, a Lark LALR command grammar, and prompt-toolkit for
live command highlighting, completion, history and input validation. All network
traffic and recorded traces use binary Protobuf.

Python requires 3.12 or newer. Clients can run on macOS, Linux or Windows and
connect to a reachable tracing server. Launching the native server and tracing
processes require Linux x86_64 with the existing Clang Tracer installation and
attachment permissions. The Python wheel does not bundle the native server or
agent. See the [native build instructions](../README.md).

## Install and open the console

From the repository root:

```sh
python3.12 -m venv .venv
.venv/bin/python -m pip install ./python
.venv/bin/clang-tracer-console --endpoint 127.0.0.1:50051
# Equivalent:
.venv/bin/python -m clang_tracer --endpoint 127.0.0.1:50051
```

`CLANG_TRACER_ENDPOINT` supplies the endpoint when no explicit endpoint is given.
The default is `127.0.0.1:50051`, matching the existing C++ client. A server
launched by Python is always passed this endpoint explicitly. Native servers
launched separately default to a private Unix socket; use the same explicit
endpoint on both sides. `unix:///absolute/path/control.sock` is supported.

On `linux-dev`, the normal startup needs no server or agent paths:

```sh
/home/husam/.local/bin/clang-tracer-console --color always
```

Then enter `server start` at the prompt. The installed server is found on PATH,
and it automatically finds its installed agent library. `--agent` is an optional
override for a custom installation.

For an isolated Unix endpoint, create its directory directly in the launch
command, avoiding an unset shell variable:

```sh
/home/husam/.local/bin/clang-tracer-console --color always --endpoint "unix://$(mktemp -d /tmp/clang-tracer-console.XXXXXX)/control.sock"
```

An empty variable in `unix://$TRACER_RUN/control.sock` produces
`unix:///control.sock`, which attempts to create a socket under `/`.

The default channel uses the existing service's local insecure transport.
Programmatic clients can supply a preconfigured gRPC channel, including a secure
channel when connecting through a TLS endpoint. The supplied channel remains
owned by its caller.

## Programmatic tracing

The repository's `examples/python/README.md` describes runnable programs for
attachment, probes, live streams, binary recordings, and server lifecycle,
including commands for the native example target.

```python
from pathlib import Path
from clang_tracer import Client

with Client("127.0.0.1:50051") as tracer:
    session = tracer.attach(12345)
    probe = tracer.inject(
        session.id,
        "Calculator::add(int, int)",
        Path("interceptor.cpp").read_text(),
        stack_depth=24,
        print_arguments=True,
        print_result=True,
    )
    try:
        for event in tracer.collect_traces(20, session_ids=[session.id], timeout=10):
            print(event.sequence, event.phase, event.call_id, event.stack)
    finally:
        tracer.remove_probe(session.id, probe.id)
        tracer.detach(session.id)
```

`inject()` resolves the C++ selector on the server using DWARF, then submits the
resulting `ProbeSpec` with the interceptor source. It requires the additive
`ResolveProbe` RPC introduced with this client. Rebuild/reinstall the native
server from this revision before using it; an older server returns gRPC
`UNIMPLEMENTED`. Existing RPCs remain compatible with older servers.

Callback source is transmitted by value. Optional `include_headers`,
`compilation_database` and `callback_package_path` refer to paths **on the server
host**. Package location is optional; pass it when the source belongs to a
retained shared-context package. Omitting it follows the native service's
isolated source-lineage behavior. The client never assumes a local callback
path exists remotely.

`compile_options` is an ordered list of compiler arguments for resolving and
compiling the callback. Pass it to either `resolve_probe()` or `inject()`;
`inject()` carries the same options through resolution and `AddProbe`, and
`add_probe()` preserves options already stored in the `ProbeSpec`. Include
directories and any header/compilation-database paths are interpreted on the
server host. For example, use `compile_options=["-std=c++17", "-I/opt/app/include"]`
when the target headers live under `/opt/app/include` on that host.
Supported options cover include paths (`-I`, `-isystem`, `-iquote`, `-include`),
defines and undefines (`-D`, `-U`), language standard (`-std=`), common warning,
optimization and debug flags. A compilation database can also supply its
matching `--sysroot` and `--gcc-toolchain` settings; target triples are checked
against the engine. Unsupported compiler flags are rejected before preflight
or installation.
Forced `-include` headers are processed in source order during preparation;
preflight does not execute their global initializers.
Standard-library types such as `std::string` use the real target headers, and
their ABI macros (including `_GLIBCXX_USE_CXX11_ABI`) must match the target
build. For example, a `std::string` callback validated against a target built
with the new libstdc++ ABI must keep `_GLIBCXX_USE_CXX11_ABI=1`.
Object arguments and by-value results are exposed as borrowed references during
the callback; a returned object is available only during POST.

Use `validate_probe()` to check callback source without installing a probe:

```python
report = tracer.validate_probe(
    session.id,
    "Calculator::add(int, int)",
    Path("interceptor.cpp").read_text(),
    compile_options=["-std=c++17", "-I/opt/app/include"],
    level="compile",
)
if not report.success:
    for diagnostic in report.diagnostics:
        print(diagnostic.stage, diagnostic.file, diagnostic.line,
              diagnostic.column, diagnostic.message)
```

`level="syntax"` checks parsing and reports source diagnostics. The default,
`level="compile"`, also generates and checks the complete callback wrapper and
its method bindings. Validation does not install or execute callbacks and
does not run callback global initializers. The report includes `success`,
diagnostics with severity, stage and source location, and the ordered compiler
options supplied for the callback. The console exposes the same operation as
`probe validate SESSION SELECTOR --callback FILE --level compile`; add repeated
`--compile-option ARG` flags for ordered compiler arguments. Validation requires
the server's additive `ValidateProbe` RPC.

`resolve_probe(session_id, selector, ...)` returns the complete inferred spec
without installing it. `add_probe(session_id, spec)` accepts a generated
`clang_tracer.messages.ProbeSpec`, preserving access to every advanced field
and the explicit low-level ABI mode. `list_sessions()`, `remove_probe()` and
`detach()` return generated messages too. The generated stubs and all context
transport messages are available under `clang_tracer.proto`; `ContextTransport`
is the native producer/collector protocol, rather than an additional public
tracing control API.

Unary RPCs default to a 30-second deadline, configured with `Client(timeout=...)`.
Errors retain their original `grpc.RpcError` status and details. The client
performs no implicit control retries because operations can mutate targets.

## Discover functions and methods

For an attached `session_id`, list callable selectors on the tracing host:

```python
with Client(endpoint) as tracer:
    page = tracer.list_symbols(session_id, query="Calculator", page_size=20)
    for symbol in page.symbols:
        print(symbol.selector, symbol.linkage_symbol, symbol.module_path)
    if page.next_page_token:
        following = tracer.list_symbols(
            session_id,
            query="Calculator",
            page_size=20,
            page_token=page.next_page_token,
        )
```

Returned selectors can be passed to `resolve_probe()` or `inject()`. Discovery
reads mapped application ELF/DWARF images on the server; it requires no target
source files on the client. Functions without supported debug signatures are
omitted. Listing a function does not bypass probe admission checks.

`query` matches a case-sensitive substring of the C++ selector or linkage name;
`module` matches a substring of the module path. Listing returns all matches by
default (`page_size=None` or `0`). A positive `page_size` opts into pagination;
the server imposes no page-size ceiling or discovery work budget. Keep the same
filters and page size when continuing with
`next_page_token`. Tokens are tied to the attached session and mapped images;
restart the query if the process or its modules change. This operation requires
a server supporting the `ListSymbols` RPC.

`tracer.list_modules(session_id)` and console `modules` list the current mapped
application image paths eligible for discovery. By default, probe resolution
reads DWARF only in images whose ELF symbol tables list the requested symbol;
unrelated images are not traversed. `module="/mapped/image"` (console
`--module /mapped/image`) explicitly selects an exact absolute path, or use a
path substring. Explicit selection also permits DWARF lookup without an ELF
symbol-table match. The injected agent revalidates only the selected image.

Debug traversal is unlimited by default. `resolve_probe()` and `inject()` accept
`debug_max_entries` and `debug_max_depth`; zero means unlimited. The console
accepts `--debug-max-entries N` and `--debug-max-depth N` on `probe resolve` and
`probe inject`. If an opted-in limit is reached, increase it or pass zero;
the server returns `RESOURCE_EXHAUSTED` with the relevant option name.

## Receiving and recording traces

```python
with Client() as tracer:
    with tracer.watch_events([session_id], after_sequence=last_sequence) as stream:
        for event in stream:
            consume(event)
            last_sequence = stream.after_sequence
```

Live streams have no deadline unless `timeout` is supplied. Call `stream.close()`
from another thread to cancel a blocked read, or leave its context manager.
`Client.close()` cancels streams on its owned channel. Empty session filters
subscribe to every session; `pid` optionally filters by process. Every event
retains stack frames, shared-context schema/payload bytes, drop counters and
PRE/POST correlation fields.

The optional `names` filter matches emitted event names and full C++ function
signatures. For example:

```python
with Client(endpoint) as tracer:
    with tracer.watch_events(
        session_ids=[session_id],
        names=["snapshot", "Calculator::multiply(int, int) const"],
        timeout=10,
    ) as stream:
        for event in stream:
            print(event.function_name, event.context.event.name)
```

Names match exactly, including case, spaces and signature qualifiers. A match
against either name is sufficient; session and PID filters must also match.
The displayed `function_name` normally comes from the resolved C++ signature;
when that signature exceeds 4096 UTF-8 bytes, the server uses its linkage symbol.
Omit `names` to receive all names. `collect_traces()` and `record_traces()` accept
the same option and count matching events. Name filtering requires an updated
server; the client reports an error if a server returns events outside the
requested names. Events retain their global sequence numbers, so filtered
results can have expected sequence gaps.

The server's history is bounded. A stale cursor produces `OUT_OF_RANGE`; the
client does not silently reset it or reconnect. Watch the event drop counters
and per-probe sequence gaps when assessing completeness. Stream deadlines also
raise their gRPC error, including during collection.

`collect_traces(count, ..., timeout=30)` retains at most `count` messages in a
list. For larger recordings use:

```python
with Client() as tracer:
    written = tracer.record_traces(
        "capture.pb",
        100_000,
        session_ids=[session_id],
        timeout=120,
    )
```

Recordings contain repeated four-byte unsigned little-endian lengths followed
by serialized `TraceEvent` bytes. Recording creates the file exclusively and
streams directly to disk. On interruption or RPC failure it preserves completed
records in the partial file and cancels the subscription. The exception still
propagates; partial data is not reported as a successful complete recording.

## Launch, status and shutdown

Both the API and console can launch and own a local native server:

```python
with Client() as tracer:
    tracer.start_server()
    print(tracer.status())
    # Attach processes, install probes, and receive events here.
    tracer.shutdown_server(timeout=30)
```

`ServerProcess` is also exported for standalone process management. Startup
checks that the endpoint is unused, launches without a shell, and waits for a
successful `ListSessions` RPC. Startup output is captured for failure diagnostics.
`status()` distinguishes service readiness from ownership of a child process.
Owned channels keep their connection state separate and use bounded reconnect
delays, so failed status checks before launch cannot leave startup waiting on
another client's connection backoff. Readiness waits for the RPC to succeed;
printing a listening address alone does not establish readiness. A startup
timeout includes the last readiness RPC status. Caller-supplied channels retain
their own transport settings, and mutating RPCs are not automatically replayed.
The startup timeout covers both readiness phases; graceful target restoration
after a failure can take additional time. If the caller's channel cannot become
ready after launch, the client retains ownership so it can still shut down the
managed server.

Shutdown sends SIGTERM to the owned child and waits for the native server to
detach sessions and restore probes. A nonzero exit is an error. If graceful
shutdown times out, ownership remains available for a later retry; the manager
does not force-kill a server while it may still be restoring targets. Closing
the client or exiting the console also shuts down its managed server.
The child has its own process session, so terminal Ctrl-C only interrupts the
console. Shutdown retries wait for the same cleanup without sending another signal.

An independently running or remote server can be queried with `status()`, but
cannot be terminated through this manager. There is no remote shutdown RPC in
the existing service. A console connection does not adopt an existing server.
Sessions on an external server remain attached until explicitly detached.

## Console commands

```text
server start
server status
attach 12345
sessions
use SESSION_ID
symbols --filter Calculator --limit 20
symbols --module my_application
probe resolve "Calculator::add(int, int)"
probe inject "Calculator::add(int, int)" --callback interceptor.cpp --stack 24
events --count 10 --after 0 --timeout 20
events --name "Calculator::multiply(int, int) const" --count 4 --timeout 10
events --name snapshot --name finished --timeout 20
collect
record capture.pb 1000 --timeout 60
probe remove PROBE_ID
detach
server stop
help probe inject
help events
help server start
help colors
help !
! pwd
! ls -l /usr/local/share/clang-tracer/examples/cpp-interceptors
exit
```

`attach` selects the returned session. Probe commands operate on that selected
session. Stream commands default to the selected session, or all sessions if
none is selected; repeated `--session ID` overrides the selection. `help` lists
commands; `help COMMAND` shows detailed usage, options, defaults and examples.
Use `help probe` or `help server` for a command group, and Tab to complete help
topics. Quote selectors and paths containing spaces; both quote styles
and escaped quotes are supported. `#` begins a comment outside a quoted value.
`symbols` lists functions and methods for the selected session. Use `--filter`,
`--module`, and `--limit` to narrow a page; the console prints a complete next-page
command when more results are available. `help symbols` explains all options.
Use repeatable `--name NAME` on `events`, `collect`, or `record` to select emitted
event names or full C++ function signatures with the same matching rules as the
Python API. The function name is displayed with each trace event.
`events` and bare `collect` have no default deadline and run until Ctrl-C.
`collect COUNT` runs until COUNT matching events arrive or Ctrl-C. Set
`--timeout SECONDS` to add a total deadline. Ctrl-C clears input or cancels a
live operation and returns to the prompt. EOF, `exit` and `quit` leave the
console.

An explicit `!` prefix executes the rest of the line with `/bin/sh` **on the
console's host**, using its current directory, environment and terminal streams.
Shell quoting, pipes, redirects, variable expansion and shell comments retain
their usual meaning. For example:

```text
! printf '%s\n' 'hello console' | tr '[:lower:]' '[:upper:]'
```

The shell runs as a child process, so `! cd /tmp` and `! export NAME=value` do not
change later console commands. A failed shell command reports its exit status
and returns to the prompt. Ctrl-C interrupts the shell command while a server
managed by the console remains running. Invalid tracer commands produce a
command error; they are never implicitly executed by a shell.

Commands, help text and trace results use colors; shell input also receives
syntax highlighting. Configure console colors at launch:

```sh
clang-tracer-console --color auto    # detect terminal support; honor NO_COLOR
clang-tracer-console --color always  # explicitly enable colors
clang-tracer-console --color never   # explicitly disable colors
```

Shell programs control their own output colors independently.

Console results use readable summaries for server status, sessions and probes.
Trace events show PRE/POST phase and call identifiers, named arguments, captured
object fields, return values when present, and stack frames. Drop counters and
capture problems remain visible. The Python API still returns complete generated
Protobuf messages; this display formatting changes neither the API nor recordings.

## Development

```sh
.venv/bin/python -m pip install -e './python[dev]'
.venv/bin/python python/scripts/generate_proto.py
.venv/bin/python -m pytest -c python/pyproject.toml python/tests
.venv/bin/python -m ruff check python
.venv/bin/python -m mypy --config-file python/pyproject.toml --strict python/src/clang_tracer
.venv/bin/python -m build python
# Also run the native Python integration tests after building the server/fixtures:
CLANG_TRACER_NATIVE_BUILD="$PWD/build" \
  .venv/bin/python -m pytest -c python/pyproject.toml python/tests
```

Generated `.py` and `.pyi` bindings are checked in so installation does not need
`protoc` or a native build. Regenerate with the pinned `grpcio-tools` version
after changing either schema. The grammar and typing marker are wheel data.

The Python tests use a real gRPC transport peer to verify control requests,
binary events, cancellation, deadlines and console behavior. They do not prove
native ptrace injection or target restoration unless `CLANG_TRACER_NATIVE_BUILD`
is set. That opt-in adds actual server launch, free-function and method
injection, stack/event capture, recording, removal, detach, and restoration of a
live probe during server shutdown followed by reattachment. Native server tests
require the Linux toolchain and a rebuilt server. Process error paths also use
controlled child-process doubles.

Implementation references: [Python gRPC](https://grpc.io/docs/languages/python/basics/),
[Lark API](https://lark-parser.readthedocs.io/en/stable/classes.html), and
[prompt-toolkit](https://python-prompt-toolkit.readthedocs.io/en/stable/pages/asking_for_input.html).

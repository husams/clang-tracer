# Client examples

For a complete runnable example with a free function, a C++ method, and separate
`pre`/`post` classes that capture both arguments, see
[cpp-interceptors](cpp-interceptors/README.md). After building the tracer on
Linux x86-64, run `./examples/cpp-interceptors/run.sh` from the repository root.

Create the socket directory and start the server in one terminal. It stays in
the foreground; use a separate terminal for the client commands below:

```sh
mkdir -m 700 /tmp/my-clang-tracer
./build/clang-tracer-server --agent "$PWD/build/libclang_tracer_agent.so" \
  --listen unix:///tmp/my-clang-tracer/control.sock
```

In the separate client terminal, set the endpoint once:

```sh
export CLANG_TRACER_ENDPOINT=unix:///tmp/my-clang-tracer/control.sock
```

Start `./build/trace-target`; it prints its PID. From the repository root,
attach and select the C++ method by name:

```sh
./build/clang-tracer attach PID
./build/clang-tracer trace PID 'Fixture::overloaded(int)' \
  --source examples/target.cpp --callback examples/interceptor.cpp
```

Clang reads the original source with `build/compile_commands.json`, selects the
`int` overload, and derives its symbol and signature. No handwritten class,
mangled name, or ABI flags are required. `--source` names an original translation
unit; declarations in its included headers are also available. Use `--compdb
/path/to/build` for another database. Without `--source`, all database files are
parsed; source/build settings must match the running target.

The callback file defines a C++ `Interceptor` class with `pre(const Call&)` and
`post(const Call&)`. Pre records the first argument multiplied by 100, plus 17;
post records the original method result. Events display `phase=pre` or
`phase=post`, their shared `call` ID, and the typed result in post. No linkage
declaration or registration is needed. The original result remains unchanged.
The existing entry-only callback file can still trace the other overload or a
free function:

```sh
./build/clang-tracer trace PID 'Fixture::overloaded(long)' \
  --source examples/target.cpp --callback examples/trace_callback.cpp
./build/clang-tracer trace PID 'traced_work(int)' \
  --source examples/target.cpp --callback examples/trace_callback.cpp
```

A bare `Fixture::overloaded` is ambiguous and lists candidates. Unsupported
signatures and parser errors are rejected before tracing. The old explicit
`--method SYMBOL --return TYPE --arg TYPE` form remains available for known raw
symbols; it cannot be mixed with the source-resolution options.

Events stream until Ctrl-C unless a count or timeout is provided. `--after N`
resumes after a server event cursor:

```sh
./build/clang-tracer events PID
./build/clang-tracer events PID --count 10 --timeout 15
./build/clang-tracer events PID --after 100 --count 10
```

Manage probes with their printed probe IDs:

```sh
./build/clang-tracer remove PID PROBE_ID
./build/clang-tracer detach PID
./build/clang-tracer list
```

Set `CLANG_TRACER_ENDPOINT` to avoid repeating `--endpoint`; it is optional
when the server listens on the CLI default, `127.0.0.1:50051`. When the
environment variable is unset, the server endpoint must match that default.
Output is readable text by default. For machine-readable consumers,
`--format protobuf` writes the same
four-byte little-endian framed Protobuf messages as the legacy `--request`
interface; add `--output FILE` to save them instead of writing to the terminal.
The wire protocol stays unchanged.

## Concurrent C++ calls

Run `./build/trace-threaded-target` before attaching. It starts four persistent
workers plus a thread that repeatedly creates and joins temporary workers. The
leader prints each worker's TID, object address, completed call count, and failure
count while sleeping between snapshots. Both exported callables take and return
`INT32`:

| Symbol | Kind | Callable |
| --- | --- | --- |
| `_ZN8threaded15namespaced_workEi` | function | `threaded::namespaced_work(int)` |
| `_ZN8threaded6Worker4workEi` | method | `threaded::Worker::work(int)` |

Create one probe for each symbol using the same PID. Method events carry the
real `this` address in `object_address`; each worker owns a different object.
Callbacks execute on calling threads and must be thread-safe.

`ctest --test-dir build -R grpc_threaded_integration --output-on-failure`
exercises three add/remove cycles, thread creation and exit, exact entry-byte
restoration, worker progress, and clean tracing ownership after detach. It also
holds four native callbacks at a test gate: detach restores an entry but must
stay pending until the test releases the callbacks. This gate is fixture-only
test synchronization, not part of the tracing API. Binary events and the report
are saved under `build/threaded-test-results/`.

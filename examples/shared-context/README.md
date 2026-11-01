# Shared context: one request, two probes, one worker

`process_request(id)` starts a worker that calls `calculate_total(id)`, waits for
it, then returns the worker's result plus one. Both functions use the same
interceptor file, compiled separately for each probe. Their hooks see the same
request state, including on the worker thread.

| File | Purpose |
| --- | --- |
| [target.cpp](target.cpp) | Ordinary application code; no tracer dependency. |
| [interceptor.cpp](interceptor.cpp) | Commented state declaration and PRE/POST hooks. |

The outer PRE starts the state with `calls = 1`. The worker PRE changes it to
`2`. Each POST publishes a snapshot; the outer POST finishes the request scope.
The next request starts with fresh state. `std::thread` uses `pthread_create`
on this Linux profile, so propagation happens automatically.

## Run manually

Use **one shell** in the repository root on Linux x86-64. The tracer must already
be installed (`clang-tracer` and `clang-tracer-server` on `PATH`), and the
repository must have a configured build directory. See the
[build instructions](../../README.md#build-and-run) if needed. This example uses
the supported matching C++ toolchain and requires the host to permit attachment.

### 1. Build and start

Set `build_dir` to your existing configured build directory:

```bash
build_dir=build
cmake --build "$build_dir" --target shared-context-target -j2

run_dir=$(mktemp -d /tmp/clang-tracer-shared.XXXXXX)
export CLANG_TRACER_ENDPOINT="unix://$run_dir/control.sock"

clang-tracer-server --listen "$CLANG_TRACER_ENDPOINT" --timeout 60 \
  >"$run_dir/server.log" 2>&1 &
server_pid=$!

"$build_dir/shared-context-target" >"$run_dir/target.log" 2>&1 &
target_pid=$!

sleep 1
clang-tracer list
```

Wait for `list` to succeed before continuing. The installed server finds its
agent and private shared-context runtime automatically.

### 2. Attach and install both probes

```bash
clang-tracer attach "$target_pid"

clang-tracer trace "$target_pid" 'process_request(int)' \
  --callback "$PWD/examples/shared-context/interceptor.cpp"

clang-tracer trace "$target_pid" 'calculate_total(int)' \
  --callback "$PWD/examples/shared-context/interceptor.cpp"
```

The target was built with debug information and without optimization. The CLI
finds the functions by their C++ names; no target header or compilation database
is needed for these scalar signatures.

### 3. Read state snapshots

```bash
clang-tracer events "$target_pid" --count 24 --timeout 20 \
  --context-field request_id,calls,last_result

tail -n 5 "$run_dir/target.log"
```

Look for `request.step` events. For request `2`, the worker snapshot has
`calls = 2, last_result = 4`; the outer snapshot has
`calls = 2, last_result = 5`. The target prints `request=2 result=5`.
Early snapshots may have `calls = 1` while only the first probe was installed.
After both are active, each complete request visits both hooks.

The engine guards the injected shared state during each hook. Keep state
references inside the hook; the target's own result remains unchanged.

### 4. Detach and stop

```bash
clang-tracer detach "$target_pid"
kill -0 "$target_pid" && echo "Detached; target is still running."

kill "$target_pid" "$server_pid"
wait "$target_pid" "$server_pid" 2>/dev/null || true
echo "Logs: $run_dir"
```

The [API guide](../../docs/shared-context.md) covers `get()`, `with()`, container
snapshots, scope control and durable recording once this basic example works.

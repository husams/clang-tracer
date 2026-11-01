# Native threading and stack test workloads

These are integration-test fixtures. User-facing demonstrations are grouped
under [`examples`](../../../examples/README.md).

`target.cpp` builds `trace-threaded-target`. Its default mode starts four
persistent workers plus a thread that repeatedly creates and joins temporary
workers. The leader prints worker TIDs, object addresses, completed calls and
failure counts. The same executable provides busy, nested and shared-library
main-thread modes used by the native integration tests.

| Callable | Purpose |
| --- | --- |
| `threaded::namespaced_work(int)` | Function calls from multiple threads. |
| `threaded::Worker::work(int)` | Method calls on distinct worker objects. |

`shared_stack.hpp`, `shared_stack_a.cpp` and `shared_stack_b.cpp` provide the
shared-library stacks and arithmetic checks used by admission and concurrent
attachment tests. Their busy loops are intentional.

Run from a configured Linux repository build:

```bash
ctest --test-dir build -R 'grpc_(threaded|busy|nested|shared)_.*integration' \
  --output-on-failure
```

The tests exercise repeated probe installation/removal, temporary thread exit,
entry-byte restoration and worker progress after detach. Test-only callback
gates also verify that detach waits for active callbacks to finish. Each test
writes its own logs and report beneath its build directory.

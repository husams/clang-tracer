# Examples by concept

Run these examples on Linux x86-64. Each directory explains its own files and
commands; start with basic callbacks, then shared context.

| Concept | What to try |
| --- | --- |
| [Basic callbacks](basic-callbacks/README.md) | Read one argument in PRE and the original result in POST. |
| [C++ objects](cpp-interceptors/README.md) | Trace functions and methods; read arguments and object fields. |
| [Object arguments](order-arguments/README.md) | Read an Order passed by reference, pointer or value. |
| [Native REPL typed values](native-repl-values/README.md) | Read structs/classes, strings, optionals, sequences, maps, sets, and nested containers as C++17 values. |
| [Shared context](shared-context/README.md) | Share one request's state across two probes and a worker thread. |
| [Call stacks](call-stacks/README.md) | Capture and display symbolized native stacks. |
| [Legacy callbacks](legacy-callbacks/README.md) | Use the older entry-only TraceContext interface. |
| [Python API](python/README.md) | Launch a server, attach, install probes, stream events, and record/replay binary traces. |

The shared-context example has commented source and manual commands for the
complete build → attach → trace → view → detach lifecycle.

Test-only workloads and ABI/stack stress helpers live in
[`tests/fixtures`](../tests/fixtures/), including its `threaded/` directory.

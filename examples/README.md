# Examples by concept

Run these examples on Linux x86-64. Each directory explains its own files and
commands; start with basic callbacks, then shared context.

| Concept | What to try |
| --- | --- |
| [Basic callbacks](basic-callbacks/README.md) | Read one argument in PRE and the original result in POST. |
| [C++ objects](cpp-interceptors/README.md) | Trace functions and methods; read arguments and object fields. |
| [Object arguments](order-arguments/README.md) | Read an Order passed by reference, pointer or value. |
| [Native REPL local JIT](native-repl-jit/README.md) | Preview and apply bulk traces, inspect mapped memory, reload an observer, and record and replay SQLite. |
| [Prepared injection](prepared-injectors/README.md) | Prepare typed probes from headers, inspect returned object pointers, methods, and custom state. |
| [Native REPL typed values](native-repl-values/README.md) | Read structs/classes, strings, optionals, sequences, maps, sets, and nested containers as typed C++ values. |
| [Shared context](shared-context/README.md) | Share one request's state across two probes and a worker thread. |
| [Call stacks](call-stacks/README.md) | Capture and display symbolized native stacks. |
| [Legacy callbacks](legacy-callbacks/README.md) | Use the older entry-only TraceContext interface. |
| [Python API](python/README.md) | Launch a server, attach, install probes, stream events, and record/replay binary traces. |

## Lessons

Fifteen short lessons in [`lessons/`](lessons/) take you from the first probe to
a complete request trace. Each has its own target, observer and README with the
exact commands and expected output.

| Lesson | Topic |
| --- | --- |
| [1](lessons/01-first-probe/README.md) | Attach, inject a probe, read arguments and result. |
| [2](lessons/02-inspect-process/README.md) | Threads, modules, symbols, memory and function entry. |
| [3](lessons/03-edit-and-reload/README.md) | Draft, validate, upload, reload and remove an observer. |
| [4](lessons/04-templates/README.md) | Trace template instantiations. |
| [5](lessons/05-bulk-tracing/README.md) | Preview, save and apply a batch of probes. |
| [6](lessons/06-native-stacks/README.md) | Capture and read native call stacks. |
| [7](lessons/07-background-capture/README.md) | Capture in the background and query events in C++. |
| [8](lessons/08-record-replay/README.md) | Record to Protobuf or SQLite and replay offline. |
| [9](lessons/09-prepared-probe/README.md) | Prepare a reusable probe package from headers. |
| [10](lessons/10-typed-values/README.md) | Decode structs, strings and containers as C++ values. |
| [11](lessons/11-method-receivers/README.md) | Read the receiver of a method. |
| [12](lessons/12-shared-context/README.md) | Share state across probes and threads. |
| [13](lessons/13-headerless-module/README.md) | Probe a library with no headers using a module index. |
| [14](lessons/14-automate-with-python/README.md) | Drive the tracer from Python. |
| [15](lessons/15-request-calls/README.md) | Collect every call of one request. |

The shared-context example has commented source and manual commands for the
complete build → attach → trace → view → detach lifecycle.

Test-only workloads and ABI/stack stress helpers live in
[`tests/fixtures`](../tests/fixtures/), including its `threaded/` directory.

# Lessons: ready-made projects for the user guide

Each directory holds the files one section of the [user guide](../../docs/guide/user.md)
or the [tutorials](../../docs/guide/tutorials.md) asks you to create, so you can
run the section's commands without typing the sources first.

| Lesson | Guide section |
| --- | --- |
| [01-first-probe](01-first-probe/README.md) | [Create a target and a basic probe](../../docs/guide/user.md#create-a-target-and-a-basic-probe) |
| [02-module-index](02-module-index/README.md) | [Index debug information and prepare a probe from a binary](../../docs/guide/user.md#index-debug-information-and-prepare-a-probe-from-a-binary) |
| [03-inspect-process](03-inspect-process/README.md) | [Inspect a process and a function entry](../../docs/guide/tutorials.md#inspect-a-process-and-a-function-entry) |
| [04-edit-reload](04-edit-reload/README.md) | [Edit, validate, reload, and restore](../../docs/guide/tutorials.md#edit-validate-reload-and-restore) |
| [05-template-instantiation](05-template-instantiation/README.md) | [Trace a function template instantiation](../../docs/guide/tutorials.md#trace-a-function-template-instantiation) |
| [06-bulk-tracing](06-bulk-tracing/README.md) | [Preview and apply bulk tracing](../../docs/guide/tutorials.md#preview-and-apply-bulk-tracing) |
| [07-native-stacks](07-native-stacks/README.md) | [Capture and inspect complete native stacks](../../docs/guide/tutorials.md#capture-and-inspect-complete-native-stacks) |
| [08-background-capture](08-background-capture/README.md) | [Capture in the background and query in C++](../../docs/guide/tutorials.md#capture-in-the-background-and-query-in-c) |
| [09-record-replay](09-record-replay/README.md) | [Record and replay offline](../../docs/guide/tutorials.md#record-and-replay-offline) |
| [10-prepared-probe](10-prepared-probe/README.md) | [Prepare a reusable probe](../../docs/guide/tutorials.md#prepare-a-reusable-probe) |
| [11-typed-values](11-typed-values/README.md) | [Decode objects, containers, and method receivers](../../docs/guide/tutorials.md#decode-objects-containers-and-method-receivers) |
| [12-shared-context](12-shared-context/README.md) | [Observe shared context and thread propagation](../../docs/guide/tutorials.md#observe-shared-context-and-thread-propagation) |
| [13-native-cli](13-native-cli/README.md) | [Trace from the native CLI](../../docs/guide/tutorials.md#trace-from-the-native-cli) |
| [14-launch](14-launch/README.md) | [Launch a local application](../../docs/native-repl.md#launch-a-local-application) (REPL walkthrough) |
| [15-request-calls](15-request-calls/README.md) | [Collect every call of one request](../../docs/guide/tutorials.md#collect-every-call-of-one-request) |

The remaining sections need no project. "Install and check the tools" and
"Open the interactive guide" only run installed commands, and "Run the
installed end-to-end JIT demo" uses [native-repl-jit](../native-repl-jit/README.md).

## How to use a lesson

You need the Clang Tracer tools on `PATH`, a C++ compiler, and `make`.

1. Terminal one: `cd` into the lesson, run `make`, then start `./target`. It
   prints its PID and calls the traced function each time you press Enter.
2. Terminal two: `cd` into the same lesson and start `clang-tracer-repl`.
3. Enter the lesson's commands. Replace `12345` with the printed PID. Press
   Enter in terminal one whenever a `%collect` or `%record` is waiting.
4. Finish with `%detach` and `%quit`, then stop the target with Ctrl+C.

`make clean` removes the binary and everything the lesson generated. Lessons
are independent: each has its own copy of the target and observer, so you can
do them in any order after the first.

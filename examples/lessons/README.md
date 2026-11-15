# Clang Tracer hands-on lessons

Use a Linux x86-64 host with `make`, a C++17 compiler, and matching
`clang-tracer-repl`, server, and agent binaries. Open two terminals in the
lesson directory. Build the application with `make` and run it in one
terminal. Use its printed PID to attach the native REPL in the other.
Run `make clean` before switching compilers or build flags.

After `%attach`, `%object start` can resolve a live write watch from the
main executable. When you have the application's source and compile commands,
use `%probe prepare` for typed method callbacks or `%object prepare` to find
direct writers and build typed snapshot plans. Object preparation takes the
matching executable to verify the saved bindings.

| Try this | What you can observe |
| --- | --- |
| [15 Collect every call of one request](15-request-calls/README.md) | Use shared context to group interleaved calls into request events. |
| [24 Prepared method callbacks](24-object-method-callbacks/README.md) | Attach, prepare a typed interceptor from source, and inspect an ordinary method and an override called through a base reference. |
| [25 Lifetimes and state](25-object-lifetimes-and-state/README.md) | Discover an object automatically, trace a store, and distinguish two lifetimes at the same address. Compare the store with a method snapshot delta. |
| [26 Union writes and keyed collections](26-object-unions-and-collections/README.md) | Compare tagged and untagged views of a nested union. Watch a keyed entry move, disappear, and reappear across collection mutations. |

The object lessons use the shipped `%probe` and `%object` commands. A single
type-wide callback registration and automatic receiver classification are part
of the [custom object probe proposal](https://chatgpt.com/space/page_fc052ad1306481919d26962eb86a984c).

See the [native REPL guide](../../docs/native-repl.md) for attachment,
coverage reporting, and commands for inspecting or replaying events.

# Clang Tracer user guide

Clang Tracer attaches to a running Linux x86-64 C++ process. A **session** identifies an attachment and a **probe** runs a C++ observer when a selected function is called. PRE runs before the original call and POST runs after a normal return. The injected agent sends events to the server, where clients can stream, retain, or record them.

| Interface | Use it for |
| --- | --- |
| `clang-tracer-repl` | Explore a local process, prepare and install probes, record and analyze events in C++. |
| `clang-tracer` | Run one tracing operation at a time in a shell or script. |
| `clang-tracer-module` | Index and inspect compiled ELF files and their debug information from the shell. |

Start with the first probe below, then use the [tutorials](tutorials.md), [advanced guide](advanced.md), [command appendix](command-reference.md), and [C++ API appendix](api-reference.md).

## Install and check the tools

On the `c++17` branch, the native Linux x86-64 backend builds as C++20 and the native REPL builds as C++17. Use matching LLVM/Clang 21 or 22 development libraries, CMake, Protobuf, gRPC and SQLite development packages. The injected agent needs compatible runtime libraries on the target machine. Build the target with debug info, preserve its ELF or separate debug files, and use a compiler and ABI supported by the agent. The tracer must have permission to attach to the target under the host's Linux tracing policy.

From the repository root, configure, build and install into a user-owned prefix. Replace the toolchain path with the matching LLVM/Clang installation.

```bash
./build-rhel.sh --toolchain-root /opt/llvm-21 --build-dir build-rhel \
  --prefix "$HOME/.local/clang-tracer" --examples
./deploy-rhel.sh --build-dir build-rhel
export PATH="$HOME/.local/clang-tracer/bin:$PATH"
clang-tracer-server --help
clang-tracer-repl --help
clang-tracer-module --help
```

For a split LLVM installation, CMake options, staging, tarball and RPM packaging, see [Build and deploy on RHEL-compatible Linux](../build-rhel.md). For GCC 8.5 SDK targets, use the [SDK compatibility build](../gcc85-compatibility.md). If you already have the native package, put its binary directory on `PATH` and run the help commands. The server finds its matching installed agent automatically. The native REPL starts and owns a private server unless you specify an endpoint.

## Open the interactive guide

Open [the HTML guide](index.html) directly in a browser. It includes a collapsible navigation sidebar, section filtering, and copy buttons. For HTTP access, run the launcher with Python 3.8 or newer. It defaults to `127.0.0.1:8000` and accepts any bind address available on the host, including IPv6. It serves the documentation directory only.

```bash
# From a source checkout
./docs/guide/serve.sh --bind 127.0.0.1 --port 8000
# From a default runtime installation
/usr/local/share/doc/clang_tracer/docs/guide/serve.sh --bind 0.0.0.0 --port 8080
```

Open `http://127.0.0.1:8000/` for the first command. For a wildcard bind, use the host's actual IP address or hostname in the browser. Ctrl+C stops the guide server.

## Create a target and a basic probe

Work in a scratch directory that both terminals can access. Save this as `target.cpp`. The target prints its PID and waits for Enter so you control when the probe fires. When the SDK headers expose `PR_SET_PTRACER`, the demo requests permission for a tracer in the second terminal.

```cpp
#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

__attribute__((noinline)) int add(int a, int b) { return a + b; }

int main() {
#ifdef PR_SET_PTRACER
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
#endif
  std::printf("PID %ld | press Enter to call add, Ctrl+C to exit\n",
              static_cast<long>(getpid()));
  std::fflush(stdout);

  pollfd input{STDIN_FILENO, POLLIN, 0};
  while (true) {
    // Check stdin without blocking in a read while the tracer attaches.
    if (poll(&input, 1, 0) > 0) {
      char byte;
      if (read(STDIN_FILENO, &byte, 1) != 1) break;
      if (byte == '\n') {
        std::printf("add(20, 22) = %d\n", add(20, 22));
        std::fflush(stdout);
      }
    }
    usleep(20'000);
  }
}
```

Save the observer as `hook.cpp`. Specialize `Interceptor<int(int, int)>` to match the return and parameter types of `add`. Clang Tracer supplies the primary template and `Call`, so the callback can read typed arguments and results directly.

```cpp
template <>
class Interceptor<int(int, int)> {
public:
  void pre(const Call& call) {
    call.field("left", call.argument<0>());
    call.field("right", call.argument<1>());
  }

  void post(const Call& call) {
    call.field("sum", call.result());
  }
};
```

Start the target in terminal one with debug information and a predictable function entry. Keep the target running throughout the session.

```bash
c++ -g -O0 -fno-omit-frame-pointer -fno-pie -no-pie -Wl,--build-id target.cpp -o target
./target
```

In terminal two, run `clang-tracer-repl` from the same directory. Replace `12345` with the printed PID. You can also use `%attach` without a PID to open the process picker.

```bash
%attach 12345
%symbols --filter "add("
%probe resolve "add(int, int)"
%probe validate "add(int, int)" --callback hook.cpp
%probe inject "add(int, int)" --callback hook.cpp --print-arguments --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as first
```

Press Enter once in terminal one while `%collect` is waiting. The target prints `42`; the retained events include the arguments and the sum. You can inspect the pair and read the result as a C++ value.

```bash
%events list first
%events show first --index 1
%events calls first
```

```cpp
auto first_call = tracer.batch("first");
first_call.at(1).result_as<int>()
```

```bash
%detach
%quit
```

Stop the target with Ctrl+C. If discovery is empty, confirm the attached PID, use `%modules` to confirm the binary, and check that it still has its debug information. If attachment is refused, confirm the target is yours, its tracing policy permits the attachment, and no debugger already owns it.

## Index debug information and prepare a probe from a binary

A module index is a SQLite file built from a Linux x86-64 executable or shared
library and its matching DWARF debug information. Use it to inspect signatures,
ELF symbols, and known type layouts before attaching to a process. You can also
prepare a probe from the index with just the matching ELF and your callback.

### Build and browse the index

Use `target` and `hook.cpp` from [the first probe](#create-a-target-and-a-basic-probe).
Start a fresh native REPL in that directory. These commands work without a
target attachment.

```bash
%module index --file ./target --output target.sqlite
%module types --filter int
%module symbols --filter "add*"
%module show --symbol "add(int, int)"
%module show --type int
```

The symbol details include the linkage name, ELF address, return type and
parameters when available. The type details report size and, for records,
known fields and their offsets. Use the full signature or copy an ID from a
listing when a name is ambiguous. Use `--exported` on `%module symbols` when
you want only dynamic exports, such as the exported functions of a shared
library. Regular functions in the first-probe executable can be indexed even
when they are absent from that export list.

Omit `--output` to write `<ELF>.tracer.sqlite`. Choose a new filename when
rebuilding an index. The REPL selects the index after creating it. In a new
REPL, use `%module use target.sqlite` to select it again, or pass
`--index target.sqlite` on an individual query.

When DWARF lives in a separate file, let the indexer find a matching debug
companion or pass `--debug-file FILE`. For example, create a separate-debug
copy of the lab binary and index that copy.

```bash
cp target target-split
objcopy --only-keep-debug target-split target-split.debug
objcopy --strip-debug --add-gnu-debuglink=target-split.debug target-split
clang-tracer-module index --file ./target-split --debug-file ./target-split.debug --output target-split.sqlite
clang-tracer-module info --index target-split.sqlite
```

If you already have a local target attached to the REPL's own server, you
can index a loaded image using `%modules --details`. Copy the exact path
and mapping token from its output. The mapped ELF must have a build ID.
If the output says `build_id=unavailable`, use file indexing instead.

```bash
%module index --module /absolute/path/to/target --mapping TOKEN --output live-target.sqlite
```

An ELF without usable DWARF can still provide symbols, including exports, but
preparation needs a supported signature and sufficient type information.
Split DWARF units are reported as unresolved. If you see
`debug_file_mismatch`, use the debug file from the same build.

### Generate declarations and install the indexed probe

Back in the REPL, select the original index. Emitting headers is optional.
It writes `declarations.hpp`, `views.hpp` for supported read-only field access,
and `manifest.json`. The final command below prepares a package using the
generated files and verifies them against the selected index entry.

```bash
%module use target.sqlite
%module emit --symbol "add(int, int)" --output recovered-add
%probe prepare "add(int, int)" --module-index target.sqlite --module ./target --generated recovered-add --output prepared-add
```

Leave out `--generated` to let the preparer generate the same declarations
internally. The package binds to the indexed ELF build and embeds the
declarations it needs. Keep the package with your callback and use the same
binary for the live step. If the application is rebuilt, index the new ELF
and prepare a new package.

Run `./target` in another terminal. Replace `12345` with its printed PID and
use the same `hook.cpp` from the first lab.

```bash
%attach 12345
%probe validate "add(int, int)" --callback hook.cpp --prepared prepared-add --prepared-mode require
%probe inject "add(int, int)" --callback hook.cpp --prepared prepared-add --prepared-mode require --print-result
%collect --name "add(int, int)" --count 2 --timeout 30 --as indexed
%events list indexed
%detach
```

Confirm that validation reports `prepared_used: true`. Press Enter in the
target terminal while the capture waits. The POST event reports `sum=42`.
The index and debug companion are needed to recover metadata. A completed
prepared package can be reused for injection without either file.

See [module index commands](commands.md#native-repl-module-index-commands) for
all options, [the native REPL walkthrough](../native-repl.md#prepare-a-probe-from-a-binary-without-headers)
for a callback that reads recovered record fields, and the
[native C++ module API](developer.md#native-c-module-index-api)
for a programmatic workflow inside the REPL or a C++ application.

## Trace changes to C++ objects

Use a native REPL method watch to compare a live receiver before and after
a call in a prebuilt executable. Select private members, strings, nested
records, or containers with `--field`. Pair vector elements by a stable ID
and map entries by key using `--key`, then inspect the typed changes from
the SQLite recording. Supported integer write watches identify the actual
store instruction, including writes to fields inside a selected vector
element, map entry, or pointee. See the [native REPL object watch walkthrough](../native-repl.md#prepare-an-object-watch-for-method-snapshots).

## Related documentation

- [Command reference](commands.md)
- [Basic and advanced tutorials](tutorials.md)
- [Native C++ REPL walkthrough](../native-repl.md)
- [Event retention and SQLite storage](../event-storage.md)
- [Developer guide and API reference](developer.md)
- [Optional Python client and console](python-client.md)

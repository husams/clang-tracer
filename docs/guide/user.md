# Clang Tracer user guide

Clang Tracer attaches to a running Linux x86-64 C++ process. A **session** identifies an attachment and a **probe** runs a C++ observer when a selected function is called. PRE runs before the original call and POST runs after a normal return. The injected agent sends events to the server, where clients can stream, retain, or record them.

| Interface | Use it for |
| --- | --- |
| `clang-tracer-repl` | Explore a local process, prepare and install probes, record and analyze events in C++. |
| `clang-tracer-console` | Control a server interactively through the Python client. |
| `clang-tracer` | Run one tracing operation at a time in a shell or script. |
| `clang_tracer.Client` | Automate tracing from Python. See the [developer guide](developer.md). |

Start with the first probe below, then use the [command reference](commands.md) and [tutorials](tutorials.md).

## Install and check the tools

The main-branch native build requires Linux x86-64, a C++23 compiler, matching LLVM/Clang 21 or 22 development libraries, CMake, Protobuf, gRPC and SQLite development packages. The injected agent needs compatible runtime libraries on the target machine. Build the target with debug info, preserve its ELF or separate debug files, and use a compiler and ABI supported by the agent. The tracer must have permission to attach to the target under the host's Linux tracing policy.

From the repository root, configure, build and install into a user-owned prefix. Replace the toolchain path with the matching LLVM/Clang installation.

```bash
./build-rhel.sh --toolchain-root /opt/llvm-21 --build-dir build-rhel \
  --prefix "$HOME/.local/clang-tracer" --examples
./deploy-rhel.sh --build-dir build-rhel
export PATH="$HOME/.local/clang-tracer/bin:$PATH"
clang-tracer-server --help
clang-tracer-repl --help
```

For a split LLVM installation, CMake options, staging, tarball and RPM packaging, see [Build and deploy on RHEL-compatible Linux](../build-rhel.md). If you already have the native package, put its binary directory on `PATH` and run the two help commands. The server finds its matching installed agent automatically. The native REPL starts and owns a private server unless you specify an endpoint.

The Python client is installed separately. Use Python 3.12 or newer. Install from the repository or replace `./python` with your distribution's wheel.

```bash
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install ./python
clang-tracer-console --help
```

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

Work in a scratch directory that both terminals can access. Save this as `target.cpp`. The target prints its PID and waits for Enter so you control when the probe fires. It permits tracing from the second terminal for this demo process.

```cpp
#include <cstdio>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>

__attribute__((noinline)) int add(int a, int b) { return a + b; }

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) == -1) {
    perror("PR_SET_PTRACER");
    return 1;
  }
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
c++ -g -O0 -fno-omit-frame-pointer -fno-pie -no-pie target.cpp -o target
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

## Prepare a probe when you only have a binary

The native REPL can index a Linux ELF executable or shared library, browse its
DWARF types and ELF exports, and build a probe package without application
headers. For a library with matching DWARF, start with:

```text
%module index --file ./libdemo.so
%module symbols --exported --filter "demo::*"
%module show --type demo::Packet
%probe prepare "demo::score(demo::Packet const&)" --output prepared-score
```

The index becomes the session default. Use `%module use INDEX` to reuse it in a
new REPL, or pass `--module-index INDEX` for one preparation. Use
`%module emit` to write C++ declarations and read-only accessors for selected
types and functions. See [the native REPL walkthrough](../native-repl.md#prepare-a-probe-from-a-binary-without-headers)
for the callback, injection commands, and C++ API, or the
[Python guide](../../python/README.md#index-a-module-and-prepare-a-probe-without-application-headers)
for the programmatic workflow.

## Related documentation

- [Command reference](commands.md)
- [Basic and advanced tutorials](tutorials.md)
- [Native C++ REPL walkthrough](../native-repl.md)
- [Python console and client walkthrough](../../python/README.md)
- [Event retention and SQLite storage](../event-storage.md)
- [Developer guide and API reference](developer.md)

# Build and deploy on RHEL-compatible Linux

The native backend requires Linux x86-64, C++20 and matching LLVM/Clang 21 or 22
development libraries. The scripts have been verified on Rocky Linux 9.8 with
LLVM/Clang 21.1.8. Red Hat Enterprise Linux itself has not been tested.

## Dependencies

Install dependencies using your organization's approved package repositories.
Typical RHEL-compatible package names are:

```sh
sudo dnf install cmake ninja-build gcc-c++ clang clang-devel clang-libs llvm-devel \
  protobuf-compiler protobuf-devel grpc-devel grpc-plugins sqlite-devel
```

Repository availability and LLVM versions vary. Confirm the selected packages
provide LLVM/Clang 21 or 22; an older distribution default is insufficient.
`rpm-build` is additionally needed for RPM packaging. Neither script installs
packages, changes repositories, modifies tracing policy, or creates services.

## Select the compiler and LLVM installation

A distribution installation with split CMake package directories can use:

```sh
./build-rhel.sh --build-dir build-rhel \
  --cc /usr/bin/clang --cxx /usr/bin/clang++ \
  --llvm-dir /usr/lib64/llvm21/lib64/cmake/llvm \
  --clang-dir /usr/lib64/cmake/clang \
  --prefix /opt/clang-tracer --jobs 2 --examples
```

For a self-contained custom LLVM installation:

```sh
./build-rhel.sh --build-dir build-custom \
  --toolchain-root /opt/llvm-21 \
  --prefix /opt/clang-tracer --bindir tools --libdir runtime/lib64 \
  -- -DCMAKE_INSTALL_INCLUDEDIR=sdk/include
```

Use the real additional CMake options you need after `--`; for example:

```sh
./build-rhel.sh --build-dir build-sdk --toolchain-root /opt/llvm-21 \
  --prefix /opt/clang-tracer \
  -- -DCMAKE_PREFIX_PATH='/opt/grpc;/opt/protobuf' \
     '-DTRACER_JIT_FLAGS=--gcc-toolchain=/opt/gcc-14;--sysroot=/opt/native-sdk'
```

`--toolchain-root` selects `PREFIX/bin/clang`, `PREFIX/bin/clang++` (unless
`CC`/`CXX` or explicit compiler options select others), and LLVM/Clang CMake
packages under the prefix. `--llvm-root` selects only the library packages;
`--cc` and `--cxx` independently select the build compilers. Explicit `--llvm-dir`
and `--clang-dir` handle distributions that split those packages across prefixes.
An unavailable LLVM root fails configuration rather than selecting system LLVM.

Alternatively, pass `--toolchain-file /opt/sdk/native-toolchain.cmake`. Standard
`CC`/`CXX`, `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, `CMAKE_SYSROOT`, compiler
flags and `CMAKE_PREFIX_PATH` remain supported. Use a fresh build directory when
changing compiler or SDK. This supports native Linux x86-64 SDK configurations;
cross-compiling the embedded LLVM toolchain is not covered by these scripts.

The selected LLVM package's Clang driver discovers its builtin header directory
and is checked for the matching LLVM major. Override discovery with
`-DTRACER_CLANG_DRIVER=/opt/llvm-21/bin/clang`, or explicitly set
`-DTRACER_CLANG_RESOURCE_DIR=/opt/llvm-21/lib/clang/21`. That directory must
contain `include/stddef.h`; an explicit resource override must match the linked
Clang version.

The embedded callback compiler is independent of the compiler that built the
project. Set `TRACER_JIT_FLAGS` to a CMake semicolon-separated list of compiler
arguments when callbacks need a custom sysroot, GCC installation, include paths,
or `-stdlib=libc++`. These defaults apply to callback JIT compilation, callback
syntax checks and optional target-header parsing. Per-request compilation
database options follow those defaults for target-header parsing. Supply the
corresponding build compiler flags separately, for example
`-DCMAKE_CXX_FLAGS=--gcc-toolchain=/opt/gcc-14`. Use absolute SDK/include paths:
the target's working directory may differ from the build directory.

Keep the target and callback ABI compatible, including standard-library versions
when exchanging library types. Linking the tracer against LLVM/Clang does not
bundle a C++ SDK or alter the target's loaded standard library.

## Installation layout

```sh
./deploy-rhel.sh --build-dir build-rhel
```

The install tree contains:

| CMake variable | Default in `build-rhel.sh` | Contents |
| --- | --- | --- |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Install root |
| `CMAKE_INSTALL_BINDIR` | `bin` | `clang-tracer`, `clang-tracer-server` |
| `CMAKE_INSTALL_LIBDIR` | `lib64` | `libclang_tracer_agent.so` |
| `CMAKE_INSTALL_INCLUDEDIR` | `include` | Public `tracer/` headers |
| `CMAKE_INSTALL_DATADIR` | `share` | Protocol schema, example sources, Clang builtin headers |
| `CMAKE_INSTALL_DOCDIR` | `share/doc/clang_tracer` | README, documentation and licenses |

`--prefix`, `--bindir` and `--libdir` configure the common choices in the build
script. Pass other GNUInstallDirs variables after `--`, for example
`-DCMAKE_INSTALL_INCLUDEDIR=sdk/include -DCMAKE_INSTALL_DATADIR=assets/share`.
Relative directories resolve under the prefix. CMake also accepts absolute
directories; configure their final locations and prefix before building.
For layouts mixing absolute and relative directories, do not override the
prefix after configuration or move the installation.

Stage a package tree without touching the live filesystem:

```sh
./deploy-rhel.sh --build-dir build-rhel --destdir "$PWD/stage"
```

With prefix `/opt/clang-tracer`, files appear under
`stage/opt/clang-tracer`. `DESTDIR` is a staging root, not a runtime path.
For layouts using relative directories, `--prefix /another/prefix` overrides
the install prefix and the complete installed prefix can be moved together.
Do not independently move its binaries, agent and resource headers.

The server finds the agent relative to its executable, including when launched
through `PATH` from another directory. `--agent /path/to/library.so` overrides
that lookup; `CLANG_TRACER_AGENT` is the next priority. A build-tree server also
finds an adjacent agent library.

Matching Clang builtin headers are installed by default and found relative to
the executable or injected agent. No build-tree resource directory is needed
after installation. Disable copying with `-DTRACER_BUNDLE_CLANG_HEADERS=OFF`
only if a compatible resource directory will remain available on the target
host. `CLANG_TRACER_CLANG_RESOURCE_DIR=/absolute/resource/directory` overrides
resource lookup **in the process doing compilation**. For injected callbacks,
set it in the target's environment before starting the target; setting it only
on the tracing server does not change an existing target's environment.

Shared LLVM, Clang, gRPC, Protobuf and other system dependencies must be available
on the deployment host. The install adds a relative agent-library search path
and CMake's external link paths; it does not bundle dependency libraries. For
nonstandard dependency locations, configure their deployment locations using
`CMAKE_INSTALL_RPATH` or arrange the loader environment on the host, including
in the target process for the injected agent. Keep deployment ABI versions
compatible with the build host.

## Archives and RPMs

```sh
./build-rhel.sh --build-dir build-rhel --prefix /opt/clang-tracer \
  --llvm-dir /usr/lib64/llvm21/lib64/cmake/llvm \
  --clang-dir /usr/lib64/cmake/clang --package TGZ
./build-rhel.sh --build-dir build-rhel --prefix /opt/clang-tracer \
  --llvm-dir /usr/lib64/llvm21/lib64/cmake/llvm \
  --clang-dir /usr/lib64/cmake/clang --package RPM
```

Packages are placed under `build-rhel/packages/`. TGZ archives contain a common
top-level directory. Both formats preserve the configured installation prefix.
RPM installation is a separate administrator action; review
the generated package and its dependency list before installing it on a host.
Neither format bundles dependency DSOs. `--with-tests` builds integration
fixtures but never runs them; ordinary builds omit those fixtures and example
targets unless `--examples` is requested.

Start the installed runtime with your selected paths:

```sh
/opt/clang-tracer/bin/clang-tracer-server \
  --listen unix:///tmp/my-private-directory/control.sock
CLANG_TRACER_ENDPOINT=unix:///tmp/my-private-directory/control.sock \
  /opt/clang-tracer/bin/clang-tracer list
```

Create that private directory first with mode `0700`. Attachment still requires
the host's usual process tracing permissions; deployment does not change them.

## Verification

On Rocky Linux 9.8 with LLVM/Clang 21.1.8, verification covered a distribution
build and a separate build using a native CMake toolchain file, compiler wrappers
around the same Clang installation, an explicit LLVM root and custom callback
compiler flags. This verifies path selection, rather than a second LLVM version.

The custom build installed into `tools`, `runtime/lib64`, `sdk/include` and
`assets/share` beneath a staged prefix. After overriding that prefix at install
time and moving the complete tree again, the server launched through `PATH`
from another directory and found its agent automatically. With the private
build-time resource copy temporarily unavailable, the installed agent compiled
a callback requiring both builtin headers and the configured JIT macro.
Public CLI tracing recorded 12 PRE and 12 POST events, 60 callback stack frames
in each phase, zero reported agent/server losses and zero target failures;
normal target calls continued after detach. Optional callback value printing
remained off and application-only stack filtering remained on.

Both `--package TGZ` and `--package RPM` produced packages retaining the selected
prefix. Extracted copies of each package started and answered the `list` RPC
with automatic agent discovery. Missing LLVM roots and missing Clang resource
headers were rejected during configuration. No package was installed into the
live operating system, and the integration suite was not run.

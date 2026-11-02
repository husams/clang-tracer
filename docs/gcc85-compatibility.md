# GCC 8.5 SDK compatibility build

This recipe builds clang-tracer against an explicitly supplied SDK directory.
Clang provides the C++20 compiler while
the target remains compatible with the SDK's glibc 2.23 and libstdc++ 8.5
runtime. It installs all generated dependencies and the application into a
scratch directory; it does not use `sudo` or install into `/usr/local`.

Validation and measured overhead are recorded in
[gcc85-runtime-results.md](gcc85-runtime-results.md).

## Build

Run the script on the Linux build host with the SDK installed and an absolute,
writable scratch path:

```sh
scripts/gcc85-build.sh /path/to/toolchain /var/tmp/clang-tracer-gcc85 3
# Optional explicit runtime paths:
scripts/gcc85-build.sh /path/to/toolchain /var/tmp/clang-tracer-custom 3 \
  --sysroot /path/to/sysroot --dynamic-loader /path/to/ld-linux-x86-64.so.2
```

The first argument is the absolute SDK path; the second is the absolute scratch
path. The third argument is optional and defaults to three jobs. CMake runs from the
build host, not the SDK; set `TRACER_CMAKE` if it is not available under the
name `cmake`. The script selects Ninja when `ninja` is installed and otherwise
uses the host make generator. The SDK path has no hardcoded or environment
default. Direct CMake users must pass `-DTRACER_SDK_ROOT=/path/to/toolchain`
alongside the toolchain file; compiler-check subprojects inherit that selection.
`--sysroot` overrides the SDK-layout default for dependency compilation, SQLite,
the tracer and embedded JIT flags. `--dynamic-loader` overrides the executable
interpreter for dependency tools and tracer binaries. The loader defaults to
`lib64/ld-linux-x86-64.so.2` beneath the selected sysroot. Both effective paths
are recorded in `build-environment.txt`.

Direct CMake builds accept `-DCMAKE_SYSROOT=/path/to/sysroot` and
`-DTRACER_DYNAMIC_LOADER=/path/to/loader`. Use a fresh scratch/build directory
when changing SDK, sysroot or loader so cached compiler/linker settings cannot
silently retain the previous profile.

Binaries and debug files are fixed for each tracing session. Rebuilding or
replacing one requires a new session. Module lookup uses canonical mapped paths
and ELF load offsets/ranges, without comparing filesystem device/inode numbers
or polling binary timestamps. OverlayFS builds do not require a bind-mounted
filesystem to satisfy file identity checks.

The script preserves the scratch tree and refuses to replace an existing gRPC
checkout if it does not match the pin. It rejects dirty root and recursive
submodule sources, allowing only its exact protobuf RPATH patch and the pinned
zlib CMake configuration's exact `zconf.h` to `zconf.h.included` rename.
Its dependency pins are gRPC tag
`v1.46.7` at
`02384e39185f109bd299eb8482306229967dc970`, with recursive shallow submodules
(including protobuf 3.19.5), and SQLite 3.34.1 from the official SQLite
archive. It records the checked-out gRPC/submodule revisions in
`dependency-revisions.txt`, SQLite's downloaded archive checksum and origin in
`sqlite3-checksum.txt`, the chosen host build tools and inputs in
`build-environment.txt`, and command output under `logs/`. The official
SQLite archive is checked against its pinned SHA-256;
`SQLITE_EXPECTED_SHA256` can explicitly override that expected value.

The script uses SDK `clang`/`clang++`, builds gRPC and SQLite as C++17/PIC
Release dependencies, enables only the gRPC C++ plugin, and disables their
tests. It patches protobuf's bundled install-RPATH declarations idempotently
so the installed executables retain the configured dependency and SDK runtime
paths. The SDK already provides LLVM and Clang under `lib.llvm`; the recipe
does not rebuild them. It checks `protoc --version` before configuring the
application with `cmake/toolchains/gcc85-sdk.cmake`, `RelWithDebInfo`, and
`BUILD_TESTING=ON`. The application is installed under
`<scratch>/install`.

To run the configured tests after the build:

```sh
ctest --test-dir /var/tmp/clang-tracer-gcc85/build/clang-tracer --output-on-failure
```

The build script does not run tests automatically. No build or test result is
implied by this procedure.

## Compatibility details

The SDK compiler can parse C++20 while the target standard library predates
parts of that library. `include/tracer/compat.hpp` selects standard `std::span`
when available and otherwise provides a dynamic-extent view, plus compatibility
helpers for byte views, `bit_cast`, `countr_zero`, `construct_at`, string
prefix/suffix/membership checks, bit predicates, sequence erasure, and
`remove_cvref_t`. Define
`TRACER_FORCE_COMPAT_LIBRARY=1` when compiling compatibility checks to exercise
the project fallback even with a newer host library.

Runtime paths include both `lib` and `lib64` in the scratch dependency prefix,
the SDK's `x86_64-1a-linux-gnu/lib64`, and its sysroot `lib64`. Executables use
the SDK loader explicitly. This keeps the binaries and shared libraries tied
to the intended older runtime instead of depending on whichever loader happens
to be first on the host.

The loader validates actual ELF SONAMEs and mapped paths, including older
`libc-2.23.so` and `libpthread-2.23.so` filenames. For an application without
libdl loaded, bootstrap uses the matching libc's private loader entry points.
Runtime calls use the canonical mapped module path and load bias, with live
entry-instruction checks for the loader and pthread entry points. GNU build IDs
and binary rebuild polling are not required. A second shared probe can normalize
only this session's verified existing pthread detour.

The SDK's libc process-vm wrappers are stubs; native memory reads/writes use
the supported x86-64 kernel syscalls directly. The running kernel must still
support the tracer's existing ptrace and process-vm operations; the SDK name
does not establish compatibility with a Linux 2.6.32 kernel.

The private standard-library image uses the SDK's exact static archives,
retains only available helper roots, and also supplies GCC 8's out-of-line
character allocator constructors. Sealed gthread markers are bound privately;
an attempted actual key creation traps. The runtime checks allow either the
verified libc forwarding mutex stubs or the separate pthread implementation.
They continue rejecting application interposition. Native state/exception
RTTI remains enabled; only the LLVM memory-manager/importer translation units
match the SDK LLVM build's disabled RTTI.

Lifecycle fingerprints use version 2: module-local LLVM attribute-group
numbers are normalized while actual function, dependency and call attributes
remain part of the fingerprint. Unrelated template instantiations can no
longer split an otherwise identical scope generation. Quoted `#number` data
and semantic attribute changes still distinguish generations.

Embedded compilation disables automatic host C++ include discovery and uses
the native compiler's recorded include roots. This applies to private owner
activation, SDK agent callbacks, and analysis-test compilation; a newer host
GCC installation cannot supply optional headers missing from the SDK library.
For example, GCC 11's `<span>` must not be combined with GCC 8's `type_traits`.

ELF validation checks complete file-backed `PT_LOAD` ranges by canonical path,
permissions and file offsets, including segments that begin inside a merged
mapping or span several mappings. Distinct complete load biases remain
ambiguous and are rejected. Missing mappings and incomplete segment coverage
remain errors. Native stack caches are reset for a new session after prior
callbacks drain; adding probes within the same session reuses those caches.

The optional Linux regression runner mounts an overlay only inside its private
test namespace and leaves the host build tree unchanged:

```sh
tests/overlay_integration.sh /absolute/build/directory -j2
```

It requires user/mount namespaces and OverlayFS support. Normal tracer use does
not create filesystems or require these test facilities.

The controller must map the same runtime ELF files as its target. Use the
SDK-built controller with the original SDK-runtime target. The installed
modern agent cannot run in that older runtime, and loading an SDK agent's old
libdl/pthread into a modern libc process is not a valid control. Benchmarks
therefore compare the same GCC 8.5 workload and shared library under each
build's matching runtime. Their differences include compiler/library and
dependency effects; they do not isolate just the compatibility helpers.

Legacy libc lacks the newer dedicated main-call wrapper. Busy-main admission
therefore requires the exact ELF `main` function and a direct startup chain.
If that leader is blocked in another syscall, an eligible existing worker can
host injection. A sleeping worker retains the original interrupted-sleep policy;
a directly started pthread worker in userspace must have bounded application CFI
ending at exact `start_thread` in the mapped SDK `libpthread.so.0`, then exact
`__clone` in the mapped libc. No arbitrary blocked syscall becomes an injection
host. Worker TLS and the separation between the process PID and host TID are
validated. Unsupported all-blocked and external job-stop cases report
`FAILED_PRECONDITION` after safe release, without false shutdown cleanup errors.
A fully stripped busy executable without that proof is rejected; idle attach
remains available. The legacy stripped-admission regression checks that
rejection and subsequent progress, while the modern-runtime regression still
checks successful stripped busy-main admission.

Entry instruction relocation and the supported C++ object/signature subset
are unchanged. In particular, this work does not add arbitrary RIP-relative
entry relocation or GCC 8 by-value class ABI evidence. Positive test fixtures
use stack locals before global accesses so compiler PIE defaults do not turn
them into tests of that separate entry-relocation limitation.

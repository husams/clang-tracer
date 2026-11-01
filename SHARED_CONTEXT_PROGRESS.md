# Shared-context implementation

Specification: [shared-context design, revision 52](https://chatgpt.com/space/page_6fcc0f77babc819197120f600e001777).
Worktree: `4916`, based on `2418d9be2f8facbe2eb4ccfed9ab0c679700f70e`.
The feature was merged into `main` at `841503d`. Installed-runtime validation
and its packaging follow-up are recorded below.

## Current status

All implementation milestones and acceptance checks are complete. The final
coherent Linux build passes **18/18** tests together (**190.88 seconds**, two jobs), including
the complete native shared-context aggregate and live class integration. The
latest full local macOS build passes **4/4** tests. Focused native two-recording
durable-delivery and typed-query executables also pass.

## Implementation milestones

- [x] M1: Engine-owned complete scope records, immutable generations, exact
  invocation binding restoration, rollback and nonthrowing reverse cleanup.
- [x] M2: Constructor dependencies, typed injection, shorthand, get/with,
  generic payloads, early scope ending, sorted shared guards and confinement.
- [x] M3: Live private owner/callback binding, complete C++ ABI/layout contracts,
  sealed standard-library and allocator support, effect and unwind admission.
- [x] M4: Recursive bounded Protobuf capture, immutable schema leases, package
  lineage, dynamic decoding and typed cross-revision field queries.
- [x] M5: Native pthread propagation, accepted-task draining, bounded correlated
  diagnostics, cancellation cleanup and code/schema lifetime pins.
- [x] M6: Background gRPC export, epochs/backpressure, durable producer and
  collector journals, independent recording stores and restart-safe Watch.

## Important implementation details

- Private modules resolve only explicit retained owner/support/native
  capabilities. Application symbol lookup cannot supply missing helpers.
- Owner fingerprints retain complete reachable user, engine and standard-library
  bodies. Generic types are completed before owner codec emission; this avoids
  incidental Clang pointer-attribute differences without weakening body checks.
- State and generic payload type identities preserve canonical template arguments;
  generated C++ separately removes diagnostic elaborated-type keywords.
- Producer, collector, receiver and CLI share one length-delimited schema revision
  algorithm, including descriptor bytes, family, root and container roles.
- Snapshots retain event bytes and native schema metadata independently from JIT
  code. Descriptors do not consume the 256 KiB event allowance.
- Capture storage is partitioned into 48 MiB arenas, 16 MiB native workspace and
  64 MiB transport storage. Guarded support has a separate 64 MiB pool; schema
  retention has a separate global 64 MiB ceiling.
- Cleanup can release validated private storage even when all ordinary exception
  lanes are occupied. Native Linux exhaustion checks exercise this path.
- Every context activation, including detour reuse, verifies that the kernel
  reports CET disabled. The pthread adapter preserves a verified ENDBR64 landing
  pad and relocates the following prologue; the controller independently verifies
  the entry offset and original bytes.
- Durable recordings use separate bounded, SHA-256-named collector stores and the
  shared journal budget. Resumed native sources carry PID/start-time metadata;
  Watch routing checks process identity before accepting a recovered association.
- `events PID` preserves an attached-session filter when available and otherwise
  uses a PID filter for retained/resumed data, without inventing control sessions.
- A vanished task during ptrace enumeration is discarded only after confirmed
  ENOENT; a live or unverifiable task's permission failure still rejects freezing.

## Validation

Linux: Rocky/RHEL x86-64, Clang/LLVM 21.1.8, GCC toolset 15 standard-library profile.
The coherent build is `linux-dev:/tmp/clang-tracer-shared-context-4916/build-live-final`.
Earlier incremental artifacts had incompatible old/new packet layouts; their
failures are not evidence for the final coherent build. Source copying is checked
by SHA-256 rather than relying on preserved timestamps.

Verified native cases include private generic int/string roundtrip, shared-tag
owner reuse across independent callbacks, changed constructor-helper generations,
the exact nested acceptance fixture, typed Watch fields, shared child ordering,
accepted pthread drain, bounded entry stacks, fail-open PRE failure, original C++
exceptions skipping POST, durable collector restart, and post-restart text viewing.
Two-recording durable loopback and focused typed-query executables also pass.

The exact acceptance fixture is unchanged:
`tests/fixtures/shared_context_nested.cpp`
SHA-256: `bd37dc43fb05e89cde09643b369937fe78dad38b3c6fe6ed40fad1037cbf9f5d`.

Historical checkpoints are archived in the ignored validation artifact directory
`build-linux-results/2026-11-01-shared-context/progress-history.md`.
The final build, complete CTest output, live integration reports and focused
delivery/query results are retained beside that history.

## Installed-runtime validation

The Linux runtime was rebuilt from merged `main` and installed under
`/usr/local`. Testing the installed client, server and agent exposed a missing
private compiler header: `context_codec.hpp` includes `context_schema.hpp`,
which was absent from the runtime installation. The install now includes that
header. The existing live class integration stages and uses an installed
runtime, so the same omission is covered by the normal 18-test suite.
The complete Linux suite passes all 18 tests in 190.88 seconds with two jobs;
the installed class integration passes in 126.00 seconds.

With the corrected package, the installed Linux runtime passed the full live
class integration, including shared-state mutation, typed event decoding,
pthread propagation and drain, entry stacks, contained PRE failures, native
exceptions, durable restart and resumed text viewing. The installed server also
started and answered the CLI without an explicit agent path. Installed binary
build IDs and the private schema header match the validated build.

The macOS portable build passed all four tests with local network delivery
enabled (90.10 seconds). A full native configuration rejects macOS with
`The native tracing backend requires Linux x86_64`; there is no macOS native
runtime install or live-activation backend in this implementation.

Deployment logs are retained under
`build-live-final/install-validation/` on `linux-dev`, with local copies under
`build-linux-results/2026-11-01-install-main/`.

# Update running application code and state on `c++17`

Use a matching Linux x86-64 REPL, server, and agent with `%help live update`
available. For changes to a probe callback, use [observer reload](observer-reload.md).

## Replace application code

Build the application with its original C++17 compile profile, debug info,
`-O0 -fno-inline`, `-fpatchable-function-entry=16,0` and a GNU ELF build ID.
Before editing, preserve the original project files in a separate snapshot
root. Record their paths and SHA-256 hashes alongside the ELF and exact
compilation database hashes in `clang-tracer-live.textproto`. Use the
[manifest example](../../../../docs/native-repl.md#live-code-replacement-on-linux-x86-64)
for the field names and admission limits.

Attach to the application and save a body edit under the recorded source root.
From the REPL in that project, preview the edit and check the resolved entries
and blockers. Apply when the entries match the intended change.

```text
%live preview source/price.cpp --compdb compile_commands.json --build clang-tracer-live.textproto
%live update source/price.cpp --compdb compile_commands.json --build clang-tracer-live.textproto
%live status
```

Trigger the same workload before and after to verify the applied revision.
`%live watch` applies admitted edits as files are saved. `%live revert` restores
the parent revision. Use `%live revert 0` to restore the build's original
entries and exercise the workload again. Application ABI or call-coverage
blockers can require a rebuild.

Run `ctest --test-dir build -R '^native_repl_live_update$' --output-on-failure`
in a full build with tests enabled for the branch's C++17 live update fixture.

## Update existing objects and migrate layouts

`%live scan-heap --type demo::Cache` supplies candidate evidence.
`%live update-data` writes through verified owners or guarded borrows
supplied by the application's discovery function. Use
`--discover-file discover.cpp --discover-function discover` for an explicit
`clang_tracer::ObjectSink` callback. `--from-scan ID` needs an owner the
updater can reacquire. Atomic shared-owner discovery requires libstdc++
support for `std::atomic<std::shared_ptr<T>>`. On the GCC 8.5 SDK use
explicit discovery for those owners.

Managed layout changes use `<tracer/live_migration.hpp>`, the exported
application registry, and a migration recipe. Hold the registry gate in a
stable caller before entering a replaceable function and pass its
`Registry::Access` into the function. Use `%live recover` in the same
server session on `RESTORATION_REQUIRED`. Follow the
[managed migration walkthrough](../../../../docs/native-repl.md#live-code-replacement-on-linux-x86-64)
for publication, hook and recovery details.

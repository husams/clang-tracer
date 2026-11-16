# Track C++ objects on `c++17`

Use the target's existing ELF and matching DWARF. Keep writer, lifetime, and
ownership evidence tied to the observed lifetime ID and generation. The same
address can belong to successive objects. A watch covers the admitted writers
and lifecycle boundaries. Report uncertain boundaries, coverage gaps, dropped
events, and failed samples when describing the result.

## Capture one lifetime and its stores

From the repository root on Linux x86-64, use the installed matching
`c++17` tools. Run the [object-write lesson](../../../../examples/lessons/17-object-writes/README.md)
in a fresh work directory or use new output paths.

```sh
cd examples/lessons/17-object-writes
make
./target
```

In a second terminal in the lesson directory, start `clang-tracer-repl`.
Paste these lines using the fixture's printed PID. Preparation can run before
attach. Preview is read-only and should report `binding=same-build` and the
admitted lifetime and store sites. Start prints the watch ID.

```text
%object prepare bank::Account --module ./target --field balance --code "bank::deposit(bank::Account &, int)" --backend code-cache --output ./account-watch
%attach PID
%object preview --plan ./account-watch
%object start --plan ./account-watch --stack-depth 4
%record account.db --sqlite --name object.trace --probe WATCH_ID --count 5 --timeout 30
```

Use the printed ID in place of `WATCH_ID`. Press Enter once in the target's
terminal while the recorder waits. Expect five events for one lifetime,
including stores `100 -> 125 -> 165`, and `complete=true`.

```text
%object status WATCH_ID
%object stop WATCH_ID
%detach
%events open account.db --as saved
%events list saved
%events show saved --index 1
%events stack saved --index 1
```

Check `executed_stores=2` and `dropped=0`. Verify `active=no` and
`restoration_pending=no` after stop before detaching. Retry stop with the
same ID when restoration is pending. Replay can run in a fresh REPL. Repeat
with `--count 10` and two Enter presses to see two lifetimes at the same
address. Add `--no-values` at start to record completed stores including
same-value stores as `FIELD_WRITTEN` without samples.

## Select writers

- **A store and its source location.** Use a typed leaf path with `--field`
  and exact writer selectors with `--code`. Code cache translates admitted
  instructions. Choose `--backend ptrace` for diagnostic single-stepping.
  The plan preserves the backend. Use the [AArch64 procedure](../../../../docs/native-repl.md#trace-scalar-writes-on-linux-aarch64)
  for an AArch64 target.
- **A non-leaf shared-library writer.** In preparation supply
  `--executable ./app --module ./libwriter.so --backend code-cache --follow-calls`.
  In a live standalone watch, use the mapped writer's `--module` and
  `--follow-calls`. The runtime resolves admitted PLT calls using live
  mappings. Review remaining coverage gaps in status and replay. Combined
  owner, collection, caller, and writer-entry scopes restrict call following.

For borrowed objects and backend limits, use the
[native REPL guide](../../../../docs/native-repl.md#trace-object-lifetimes-and-state-in-a-prebuilt-binary).
For collections, structured writes, or method snapshots, use
[object state](object-state.md).

## Register typed observers for a C++ type

Use `%object-probe` to install typed method observers and admitted
lifecycle or writer instrumentation under one registration. Prepare with
the original header, compilation database, translation unit, exact
`--method` selectors, and C++17 `--callbacks` source. Then attach and
preview the package against the mapped target before starting it.

```text
%object-probe preview --package ./counter.object
%object-probe start --package ./counter.object
%object-probe status REGISTRATION_ID
%object-probe stop REGISTRATION_ID
```

Use the registration ID to record grouped lifetime and field events and the
member probe IDs for PRE and POST. Correlate verified observations by
registration, object ID, and generation. A receiver whose construction
predates attach can have provisional identity. Check `identity_verified()`
and use the reported coverage when describing its lifetime.

For preparation commands, a C++17 callback and writer-only packages, see
[registering a C++ type](../../../../docs/native-repl.md#register-a-c-type-for-object-probing).

# C++17 branch reference

The native REPL on this branch includes the operations below. Use `%help COMMAND` for the syntax provided by your installed build. Target control and mutation commands require a compatible local Linux server. Use a disposable target for the examples that modify its behavior.

## Launch, control, and profile a target

| Command and example | Options and behavior |
| --- | --- |
| `%launch ./app --arg sample --env MODE=test --cwd ./work` | Launch and attach at the startup checkpoint. Repeat `--arg VALUE` and `--env NAME=VALUE`. `--cwd DIR` selects the working directory. `--stdin FILE` supplies input. `--stdout NEW_FILE` and `--stderr NEW_FILE` select new output files. `--timeout SEC` bounds startup. `--preload PATH` selects a launch gate library. `--supervise` enables owned process-tree supervision and `--gate-children` pauses supported exec descendants. |
| `%children` | List paused exec descendants of owned gated launches. |
| `%resume 12345`, `%terminate 12345` | Release the startup gate or terminate and reap an owned target. Omit the PID for the selected target. |
| `%call app::scale --arg 6 --arg 1.5 --timeout 5` | Invoke a registered scalar function on the paused startup thread. Repeat `--arg` in signature order. |
| `%call-result --timeout 5` | Wait for a previous call that timed out or was interrupted. |
| `%modules --elf /path/to/app` | Show imports, exports, and sections from the selected mapped ELF. Use `%modules --watch --count 5 --timeout 10` to report mapping changes. |
| `%library load /path/to/libdemo.so` | Load a trusted server-side shared object into the selected target. It stays loaded until the target exits. |
| `%syscalls --count 20 --timeout 5` | Trace syscall entry and exit on existing threads. |
| `%faults --count 10 --timeout 5` | Observe delivered fault signals via Linux tracefs. |
| `%sample cpu --count 20 --timeout 5` | Sample user-space PCs with Linux perf. |
| `%sample allocations --count 20 --timeout 5` | Capture allocation requests to glibc malloc, calloc, and realloc via perf uprobes. |

For profiling and module watching, `--count N` bounds the records and `--timeout SEC` bounds the capture. See [launch and call walkthroughs](../native-repl.md#launch-a-local-application) and [local sampling](../native-repl.md#capture-local-syscalls-or-cpu-samples).

## Inspect and change target memory

Get a fresh mapping token with `%memory maps` and a thread ID with `%threads`. Use live addresses for data and watchpoints. The executable patch form uses the ELF-relative address from `%probe entry`. Substitute actual addresses and IDs in the examples.

| Command and example | Options and behavior |
| --- | --- |
| `%memory allocate 4096`, `%memory free ALLOCATION_ID` | Allocate up to 1 MiB of zeroed memory owned by this session. Restore patches on an allocation before freeing it. |
| `%memory write ADDRESS --mapping TOKEN --expect "00" --replace "2a"` | Write up to 256 bytes in a verified private writable mapping. Expected bytes must match and the replacement must have the same length. |
| `%memory patch --module ELF --elf-address ADDRESS --mapping TOKEN --expect "55" --replace "90"` | Apply a verified same-width instruction patch at the selected entry. Provide bytes and complete instructions appropriate to that entry. |
| `%memory import-hook --module ELF --symbol NAME --original PC --address REPLACEMENT --mapping TOKEN --abi linux-x86-64-sysv` | Redirect one resolved import slot. Verify the original address and use an ABI-compatible replacement in the main executable. |
| `%memory restore PATCH_ID` | Restore the original bytes for a write, executable patch, or import hook. |
| `%memory monitor-start --tid TID --address ADDRESS --size 4096 --mapping TOKEN --timeout 5` | Monitor the nominated thread's first touch on dedicated pages. `--size` is the range in bytes. `--timeout` bounds protection changes. |
| `%memory monitor-stop` | Restore original protections and print first touches. |
| `%watchpoint --tid TID --address ADDRESS --size 4 --mapping TOKEN --access write --count 4 --timeout 5` | Set a hardware watchpoint on a naturally aligned 1, 2, 4, or 8-byte range. `--access` accepts `write` or `read-write`. |

An allocation lab starts with `%memory allocate 4096`. Use the returned address and a fresh token with the one-byte write above. Read the byte with `%memory read ADDRESS --mapping TOKEN --type u8` and expect 42. Restore the returned patch ID, read again and expect zero, then free the allocation.

## Capture instructions and replay them

The optional Gum backend records instructions on one nominated thread within a verified executable range. Obtain `TID`, `ADDRESS`, and `TOKEN` using `%threads`, `%probe entry`, and `%memory maps`. The target needs an eligible separate control thread.

```bash
%instructions start --tid TID --address ADDRESS --size 32 --mapping TOKEN --count 64 --timeout 5
%instructions stop --save instructions.pb
%instructions replay instructions.pb
```

Trigger the selected function while capture runs, then stop and save. Replay prints the saved capture without a live target. `--size BYTES` defines the executable window, `--count N` caps records, and `--timeout SEC` caps elapsed time. `--registers` includes general-purpose register snapshots. `--callback-library PATH` selects a trusted native instruction callback. `--memory-address ADDRESS --memory-size BYTES` filters scalar memory accesses. `--discover` records executed call destinations.

## Probe options and callback methods

The following opt-ins apply to validation and deployment. Use the same flags when validating and installing.

| Option | API and behavior |
| --- | --- |
| `--allow-arg-overrides` | `Call::override_argument(index, value)` stages a scalar argument write in PRE. |
| `--allow-result-override` | `Call::override_result(value)` stages a scalar return write in POST. |
| `--allow-replacement` | `Call::replace_with(value)` or `replace_with()` supplies a scalar or void result in PRE and skips the automatic original call. |
| `--allow-call-original` | `Call::call_original<T>()` invokes the original from PRE with its entry arguments. Repeat calls return the first result. |
| `--trace-exceptions` | Publish EXCEPTION events from the original call. Capture them with `%events --phase EXCEPTION`. The exception still propagates to the caller. |
| `--reclaimable` | Declare that the observer leaves no escaping JIT addresses or thread-local storage so it can be reclaimed after retirement. Shared-context work is excluded from this contract. |
| `%probe reload PROBE_ID --callback hook.cpp --watch --count 2 --timeout 20` | Watch the callback file for changes and reload valid generations. Use `--count` and `--timeout` to bound the watch. |

See the [scalar override tutorial](../native-repl.md#opt-in-to-scalar-overrides) for the target marker, supported scalar types, and local control requirements.

## Additional client and callback APIs

| Python method | RPC | Behavior |
| --- | --- | --- |
| `allocate_memory(session_id, size)` | `AllocateMemory` | Return the allocation ID, address, and size. |
| `free_memory(session_id, allocation_id)` | `FreeMemory` | Free the session allocation and report its state. |
| `load_target_library(session_id, path)` | `LoadTargetLibrary` | Load a trusted library on the target host. |
| `apply_memory_patch(request)` | `ApplyMemoryPatch` | Submit an executable patch, import redirect, or writable patch with a mapping token and expected bytes. |
| `restore_memory_patch(session_id, patch_id)` | `RestoreMemoryPatch` | Restore and verify original bytes. |
| `start_page_monitor(request)`, `stop_page_monitor(session_id)` | `StartPageMonitor`, `StopPageMonitor` | Arm dedicated pages, then restore them and return first touches. |
| `start_instruction_trace(session_id, address, size, *, tid, mapping_token, **options)` | `StartInstructionTrace` | Start bounded instruction capture. Options are `max_records=256`, `timeout_seconds=5`, `registers=False`, `callback_library=""`, `memory_address=0`, `memory_size=0`, and `discover_calls=False`. |
| `stop_instruction_trace(session_id)` | `StopInstructionTrace` | Stop and return original PCs in execution order. |

Python `validate_probe` and `inject` also accept `allow_call_original` and `trace_exceptions`. Both default to false. Other opt-ins can be set in the corresponding Protobuf requests on a compatible server.

```python
allocation = client.allocate_memory(session.id, 4096)
try:
    mapping = client.list_memory_mappings(session.id)
    initial = client.read_memory(
        session.id, allocation.address, 1, mapping_token=mapping.mapping_token,
    )
    assert initial == b"\x00"
finally:
    client.free_memory(session.id, allocation.id)
```

A captured native REPL `Event::stack()` returns an owned vector of frames, including each PC, stack pointer, kind, module, module offset, function, file, line, and application-frame classification.

```cpp
for (const auto& frame : tracer.batch("fullstack").at(0).stack())
  std::cout << frame.function << ' ' << frame.file << ':' << frame.line << '\n';
```

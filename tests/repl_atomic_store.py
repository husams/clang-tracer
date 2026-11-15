"""Exercise native atomic updates and active detach on a shared field."""

import argparse
from collections import Counter
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import (Output, VALUE_FIELDS, assert_untraced_tasks, maps, read_code, read_redirects,
                 send, stop, verify_restored_redirects, wait_counts)

OPERATIONS = {
    "store": ('"demo::store_age(demo::Person &, int)"', "movl", "89 37", ".store(", "__atomic_store"),
    "add": ('"demo::add_age(demo::Person &)"', "lock xaddl", "f0 0f c1 07", ".fetch_add(", "__atomic_fetch_add"),
    "cas": ('"demo::change_age(demo::Person &, int)"', "lock cmpxchgl", "f0 0f b1 37",
            ".compare_exchange_strong(", "__atomic_compare_exchange"),
}


def verify(events, operation, tids, initial_address, destination, pc, no_values, results,
           final, partial, backend):
    assert all(int(event["probeSequence"]) == i and not int(event.get("serverDroppedEvents", 0))
               and not int(event.get("agentDroppedEvents", 0))
               for i, event in enumerate(events, 1)), events
    created, *writes = events
    first = created["objectObservation"]
    assert first["kind"] == "CREATED"
    assert int(first["lifetimeId"]) == int(first["generation"]) == 1
    assert int(created["objectAddress"]) == initial_address
    if no_values:
        assert not VALUE_FIELDS.intersection(first), first
    else:
        assert first["newInteger"] == 42
    if partial:
        closure = writes.pop()["objectObservation"]
        assert closure["kind"] == "OBSERVATION_ENDED" and closure["closureReason"] == "WATCH_DETACHED", closure
        assert closure["lifetimeId"] == first["lifetimeId"] and closure["generation"] == first["generation"]
        assert not VALUE_FIELDS.intersection(closure)
    lines = set()
    observed = Counter()
    increments = set()
    last_by_tid = {}
    for index, event in enumerate(writes):
        item = event["objectObservation"]
        assert item["kind"] == ("FIELD_WRITTEN" if no_values else "FIELD_CHANGED"), item
        assert item["field"] == "demo::Person::age" and item["concreteType"] == "demo::Person", item
        assert int(item["lifetimeId"]) == int(item["generation"]) == 1, item
        assert int(event["objectAddress"]) == initial_address
        assert int(item["destination"]) == destination
        assert int(item["pc"]) == int(item["elfPc"]) == pc
        assert "stack" not in event
        assert event["tid"] in tids, event
        writer = tids.index(event["tid"])
        observed[event["tid"]] += 1
        path, line = Path(item["file"]), item["line"]
        lines.add((path, line))
        if no_values:
            assert not VALUE_FIELDS.intersection(item), item
        elif operation == "store":
            assert "oldInteger" not in item and item["newInteger"] == writer + 1, item
        elif operation == "add":
            old = item["oldInteger"] if partial else results[writer]
            if partial:
                assert 42 <= old < final and old not in increments, item
                assert old > last_by_tid.get(event["tid"], 41), item
                if backend == "ptrace":
                    assert old == 42 + index, item
                increments.add(old)
                last_by_tid[event["tid"]] = old
            assert (item["oldInteger"], item["newInteger"]) == (old, old + 1), item
        else:
            assert results[writer] == 1
            assert (item["oldInteger"], item["newInteger"]) == (42, (7, 9)[writer]), item
    [(path, line)] = lines
    _, _, _, call, builtin = OPERATIONS[operation]
    source = path.read_text().splitlines()
    assert 1 <= line <= len(source)
    assert call in source[line - 1] or builtin in "\n".join(source[max(0, line - 3):line + 2]), (
        path, line, source[line - 1]
    )
    by_tid = tuple(observed[tid] for tid in tids)
    if not partial:
        expected = results if operation == "cas" else (1, 1)
        assert by_tid == expected, by_tid
        if operation == "store" and not no_values and backend == "ptrace":
            assert writes[-1]["objectObservation"]["newInteger"] == final
    return by_tid, f"{path.name}:{line}"


def run_case(args, backend, operation, no_values, detach, trial):
    selector, instruction, encoding, *_ = OPERATIONS[operation]
    iterations = args.iterations if detach else 1
    events = 1 + (1 if operation == "cas" else 2 * iterations)
    timeout = args.timeout
    with tempfile.TemporaryDirectory(prefix="ct-atomic-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, timeout)

        target, app = start([args.fixture, operation, str(iterations)] + (["paced"] if detach else []))
        tids = tuple(map(int, app.expect(r"^READY tids=(\d+),(\d+)$").groups()))
        assert len(set(tids)) == 2 and target.pid not in tids
        endpoint = f"unix://{root / 'control.sock'}"
        limit = events + 128
        _, server_log = start([args.server, "--listen", endpoint, "--agent", args.agent,
                               "--event-history-limit", str(limit)])
        server_log.expect(r"gRPC server listening")
        common = [args.repl, "--endpoint", endpoint, "--buffer-events", str(limit)]
        control, output = start([*common, "--attach", str(target.pid)])
        session = output.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        token = maps(control, output)
        send(control, f"%probe entry {selector} --mapping {token}")
        output.expect(r"^entry=0x[0-9a-f]+ .*boundary=DWARF ")
        found = output.expect(
            rf"^(0x[0-9a-f]+)  ((?:[0-9a-f]{{2}} )+) +{instruction}\b.*\(%rdi\).* next=(0x[0-9a-f]+)$"
            r"|^error:"
        )
        assert found[1], "".join(output.seen[-12:])
        pc, original, successor = int(found[1], 16), bytes.fromhex(found[2]), int(found[3], 16)
        assert original == bytes.fromhex(encoding) and successor == pc + len(original)
        output.expect(r"^patch_size=")
        assert read_code(control, output, pc, token, len(original)) == original

        option = " --no-values" if no_values else ""
        send(control, f"%object start --type demo::Person --field age --code {selector} "
                      f"--backend {backend} --stack-depth 0{option}")
        watch = output.expect(r"^watch=(\d+) active=yes stores=1 .*atomic MOV/XADD/CMPXCHG|^error:")
        assert watch[1], "".join(output.seen[-12:])
        assert f"values={'off' if no_values else 'on'}" in watch.string
        redirects = read_redirects(control, output, watch, backend, target.pid, tids)
        armed = read_code(control, output, pc, maps(control, output), len(original))
        assert armed == (original if redirects else b"\xcc" + original[1:])

        send(target, "s")
        setup = app.expect(r"^SETUP address=(0x[0-9a-f]+) age=(0x[0-9a-f]+)$")
        address, destination = int(setup[1], 16), int(setup[2], 16)
        db, export = root / "trace.db", root / "trace.json"
        recorder, recording = start(common)
        record_timeout = 5 if detach else timeout
        send(recorder, f"%record {db} --sqlite --session {session} --name object.trace "
                       f"--probe {watch[1]} --count {events} --timeout {record_timeout}")
        wait_counts(db, lambda n: n.get(target.pid) == 1, timeout)
        send(target, "g")
        progress = None
        if detach:
            wait_counts(db, lambda n: all(n.get(tid, 0) > 0 for tid in tids), timeout)
            send(target, "p")
            progress = tuple(map(int, app.expect(r"^PROGRESS completed=(\d+),(\d+)$").groups()))
            assert all(0 < n < iterations for n in progress), progress
            if redirects:
                assert_untraced_tasks(target.pid, tids)
            send(control, "%detach")
            output.expect(rf"^detached session={re.escape(session)}$")
            send(target, "p")
            resumed = tuple(map(int, app.expect(r"^PROGRESS completed=(\d+),(\d+)$").groups()))
            assert all(start < end <= iterations for start, end in zip(progress, resumed)), (
                progress, resumed
            )

        done = app.expect(r"^DONE value=(\d+) completed=(\d+),(\d+) results=(\d+),(\d+)$")
        final, completed1, completed2, result1, result2 = map(int, done.groups())
        assert completed1 == completed2 == iterations
        results = (result1, result2)
        if operation == "store":
            assert final in (1, 2)
        elif operation == "add":
            assert final == 42 + 2 * iterations
            assert sum(results) == (2 * iterations) * (84 + 2 * iterations - 1) // 2
            if not detach:
                assert sorted(results) == [42, 43], results
        else:
            assert sorted(results) == [0, 1] and final == (7 if result1 else 9)

        if detach:
            recording.expect(r"^received=\d+ .*timed_out=true")
        else:
            recording.expect(rf"^received={events} .*complete=true")
            send(control, f"%object status {watch[1]}")
            status = output.expect(rf"^watch={watch[1]} active=yes .*observations={events} .*")
            assert "dropped=0 " in status.string
            if redirects:
                # CAS executes in both workers but publishes only on success.
                assert "executed_stores=2 " in status.string
            send(control, "%detach")
            output.expect(rf"^detached session={re.escape(session)}$")
        send(recorder, "%quit")
        assert recorder.wait(timeout=timeout) == 0, "".join(recording.seen[-12:])

        send(control, f"%attach {target.pid}")
        next_session = output.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        assert next_session != session
        token = maps(control, output)
        restored = read_code(control, output, pc, token, len(original))
        assert restored == original
        verify_restored_redirects(control, output, token, target.pid, redirects)
        send(control, f"%memory read {destination:#x} --mapping {token} --type i32")
        output.expect(rf"^{destination:#x} i32={final}$")

        send(control, f"%events open {db} --as saved")
        opened = output.expect(r"^opened events=(\d+) retained=(\d+) evicted=0$")
        assert opened[1] == opened[2]
        send(control, f"%events list saved --limit {opened[1]}")
        kind = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        display = output.expect(rf" {kind} .*demo::Person::age.* pc={pc:#x} ")
        if no_values:
            assert "demo::Person::age destination=" in display.string and "->" not in display.string
        elif operation == "store":
            assert "old=unavailable ->" in display.string
        send(control, f"%events export saved --format json --output {export}")
        send(control, "%detach")
        output.expect(rf"^detached session={re.escape(next_session)}$")

        captured = json.loads(export.read_text())
        by_tid, source = verify(captured, operation, tids, address, destination,
                                pc, no_values, results, final, detach, backend)
        if detach:
            assert all(0 < n < iterations for n in by_tid), by_tid
            assert all(recorded >= completed for recorded, completed in zip(by_tid, progress)), (
                by_tid, progress
            )
        send(control, "%quit")
        assert control.wait(timeout=timeout) == 0, "".join(output.seen[-12:])

        send(target, "q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=timeout) == 0
        message = (f"trial={trial} operation={operation} mode={'no-values' if no_values else 'values'}"
                   f" backend={backend} pc={pc:#x} source={source} tids={tids}"
                   f" bytes={original.hex(' ')} -> {armed.hex(' ')} -> {restored.hex(' ')}"
                   f" restored_redirects={len(redirects)} recorded_by_worker={by_tid}")
        if detach:
            message += f" progress_before_detach={progress} progress_after_detach={resumed}"
        print(message + f" completed_by_worker={(iterations, iterations)} results={results}"
              f" final={final} target_exit=0", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--detach", action="store_true")
    parser.add_argument("--iterations", type=int, default=1000)
    parser.add_argument("--trials", type=int, default=1)
    parser.add_argument("--timeout", type=int, default=45)
    parser.add_argument("--backend", choices=("ptrace", "code-cache", "both"), default="ptrace")
    args = parser.parse_args()
    if args.iterations < 1 or args.trials < 1 or args.timeout < 1:
        parser.error("iterations, trials and timeout must be positive")
    original = hashlib.sha256(Path(args.fixture).read_bytes()).digest()
    operations = ("add",) if args.detach else OPERATIONS
    backends = ("ptrace", "code-cache") if args.backend == "both" else (args.backend,)
    for trial in range(1, args.trials + 1):
        for backend in backends:
            for operation in operations:
                for no_values in (False, True):
                    run_case(args, backend, operation, no_values, args.detach, trial)
    assert hashlib.sha256(Path(args.fixture).read_bytes()).digest() == original


if __name__ == "__main__":
    main()

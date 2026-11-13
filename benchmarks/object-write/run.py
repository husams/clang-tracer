#!/usr/bin/env python3
"""Time the native object watch and check active detach on the same two-worker ELF."""

import argparse
from collections import Counter
from contextlib import ExitStack, closing
import hashlib
import json
from pathlib import Path
import platform
import queue
import re
import signal
import sqlite3
import statistics
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests"))
from repl_browser_integration import Transcript, stop

VALUE_FIELDS = {"oldInteger", "newInteger", "oldString", "newString", "oldBytes", "newBytes"}
MODES = ("untraced", "values", "no-values")
METHOD = "demo::WriteBatch::run()"


def send(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()


class Output(Transcript):
    def __init__(self, stream, timeout):
        super().__init__(stream)
        self.timeout = timeout

    def expect(self, pattern):
        deadline = time.monotonic() + self.timeout
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                line = None
            if line is None:
                raise AssertionError((pattern, "".join(self.seen[-25:])))
            self.seen.append(line)
            match = re.search(pattern, line)
            if match:
                return match


def read_counts(db):
    try:
        with closing(sqlite3.connect(f"file:{db}?mode=ro", uri=True, timeout=0.2)) as conn:
            return dict(conn.execute("SELECT tid, count(*) FROM trace_events GROUP BY tid"))
    except sqlite3.OperationalError:
        return {}


def wait_counts(db, predicate, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        counts = read_counts(db)
        if predicate(counts):
            return counts
        time.sleep(0.005)
    raise AssertionError(("recorder did not reach the expected counts", read_counts(db)))


def maps(control, output):
    send(control, "%memory maps")
    token = output.expect(r"^memory mapping=(\S+)$")[1]
    output.expect(r"^memory mappings=[1-9][0-9]*$")
    return token


def read_code(control, output, pc, token, count):
    send(control, f"%memory read {pc:#x} --mapping {token} --count {count}")
    return b"".join(bytes.fromhex(output.expect(rf"^{pc + offset:#x}  ((?:[0-9a-f]{{2}} )+) +\|")[1])
                    for offset in range(0, count, 16))


def cpu_usage(pid):
    usage = {}
    for stat in Path(f"/proc/{pid}/task").glob("*/schedstat"):
        try:
            usage[stat.parent.name] = int(stat.read_text().split()[0])
        except (FileNotFoundError, ProcessLookupError):
            continue
    return usage


def cpu_delta(pid, before):
    return sum(total - before.get(tid, 0) for tid, total in cpu_usage(pid).items())


def assert_anonymous_rx(pid, address):
    mappings = Path(f"/proc/{pid}/maps").read_text().splitlines()
    for entry in mappings:
        span, permissions, *_ = entry.split()
        start, end = (int(part, 16) for part in span.split("-"))
        if start <= address < end:
            assert permissions.startswith("r-x") and len(entry.split()) == 5, entry
            return
    raise AssertionError(f"no executable code-cache mapping at {address:#x}")


def read_redirects(control, output, watch, backend, pid, tids, *, live_read=None, runtime_calls=False):
    assert f"backend={backend}" in watch.string and "dropped=0 " in watch.string
    count = int(re.search(r"redirects=(\d+)", watch.string)[1])
    assert bool(count) == (backend == "code-cache")
    redirects = {}
    for _ in range(count):
        found = output.expect(r"^redirect=(0x[0-9a-f]+) translated=(0x[0-9a-f]+) "
                              r"original=([0-9a-f]+) installed=([0-9a-f]+)$")
        pc, translated = (int(value, 16) for value in found.groups()[:2])
        original, armed = (bytes.fromhex(value) for value in found.groups()[2:])
        assert len(original) == len(armed) and original != armed
        if platform.machine() == "aarch64":
            assert len(armed) == 4
            branch = int.from_bytes(armed, "little")
            assert branch >> 26 == 5 or (runtime_calls and branch >> 26 == 37)
            displacement = (branch & 0x1ffffff) - (branch & 0x2000000)
            assert pc + 4 * displacement == translated
        else:
            assert len(armed) >= 5
            if runtime_calls and len(armed) >= 10 and armed[0] == 0xe9 and armed[-5] == 0xe8:
                assert armed[5:-5] == b"\x90" * (len(armed) - 10)
                gateway = pc + len(armed) + int.from_bytes(armed[-4:], "little", signed=True)
                assert_anonymous_rx(pid, gateway)
            else:
                assert ((len(armed) == 5 and original[0] == armed[0] == 0xe8) or
                        (armed[0] == 0xe9 and armed[5:] == b"\x90" * (len(armed) - 5)))
            assert pc + 5 + int.from_bytes(armed[1:5], "little", signed=True) == translated
        assert pc not in redirects
        redirects[pc] = original, armed, translated
        assert_anonymous_rx(pid, translated)
    if redirects:
        if live_read is None:
            token = maps(control, output)
            def live_read(pc, size):
                return read_code(control, output, pc, token, size)
        for pc, (_, armed, _) in redirects.items():
            assert live_read(pc, len(armed)) == armed
        assert_untraced_tasks(pid, tids)
    return redirects


def assert_untraced_tasks(pid, tids):
    for tid in (pid, *tids):
        status = Path(f"/proc/{pid}/task/{tid}/status").read_text()
        assert int(re.search(r"^TracerPid:\s+(\d+)$", status, re.MULTILINE)[1]) == 0


def verify_restored_redirects(control, output, token, pid, redirects, *, live_read=None):
    if live_read is None:
        def live_read(pc, size):
            return read_code(control, output, pc, token, size)
    for pc, (original, _, translated) in redirects.items():
        assert live_read(pc, len(original)) == original
        assert_anonymous_rx(pid, translated)


def verify_record(events, mode, count, tids, base, stride, offset, pc, line, partial):
    no_values = mode == "no-values"
    per_worker = count // 2
    expected = {
        base + i * stride: tids[i // per_worker]
        for i in range(count)
    }
    lifetimes = {}
    observed = Counter()
    for index, event in enumerate(events, 1):
        assert int(event["probeSequence"]) == index, event
        assert not int(event.get("serverDroppedEvents", 0)), event
        assert not int(event.get("agentDroppedEvents", 0)), event
        assert "stack" not in event, event
        item = event["objectObservation"]
        assert item["concreteType"] == "demo::Person", item
        address = int(event["objectAddress"])
        identity = (int(item["lifetimeId"]), int(item["generation"]))
        if no_values:
            assert not VALUE_FIELDS.intersection(item), item
        if item["kind"] == "CREATED":
            assert address in expected and address not in lifetimes, item
            assert identity[0] > 0 and identity[1] == 1, item
            if not no_values:
                assert item["newInteger"] == 42, item
            lifetimes[address] = identity
            continue
        if item["kind"] == "OBSERVATION_ENDED":
            assert partial and lifetimes[address] == identity, item
            assert item["closureReason"] == "WATCH_DETACHED", item
            assert not VALUE_FIELDS.intersection(item), item
            continue
        assert item["kind"] == ("FIELD_WRITTEN" if no_values else "FIELD_CHANGED"), item
        if not no_values:
            assert (item["oldInteger"], item["newInteger"]) == (42, 1), item
        assert item["field"] == "demo::Person::age", item
        assert int(item["destination"]) == address + offset, item
        assert int(item["pc"]) == int(item["elfPc"]) == pc, item
        assert Path(item["file"]).name == "update.cpp" and item["line"] == line, item
        assert lifetimes[address] == identity, item
        tid = event["tid"]
        assert expected.get(address) == tid, (hex(address), tid, expected.get(address))
        observed[(address, tid)] += 1

    assert set(lifetimes) == set(expected), len(lifetimes)
    assert len(set(lifetimes.values())) == count
    intended = Counter((address, tid) for address, tid in expected.items())
    extra = observed - intended
    assert not extra, ("duplicate or unexpected", extra)
    by_tid = tuple(sum(times for (_, writer), times in observed.items() if writer == tid)
                   for tid in tids)
    if partial:
        assert all(0 < n < per_worker for n in by_tid), by_tid
    else:
        missing = intended - observed
        assert not missing, ("missing writes", missing)
        assert by_tid == (per_worker, per_worker), by_tid
    return by_tid


def run_case(args, backend, mode, detach, trial):
    count, timeout = args.writes, args.timeout
    per_worker = count // 2
    with tempfile.TemporaryDirectory(prefix="ct-writes-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(
                argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True,
            )
            cleanup.callback(stop, process)
            return process, Output(process.stdout, timeout)

        target, application = start([args.fixture, str(count)])
        tids = tuple(map(int, application.expect(r"^READY tids=(\d+),(\d+)$").groups()))
        assert len(set(tids)) == 2 and target.pid not in tids
        progress = counts = None
        method = mode == "method"
        server_process = None
        if mode != "untraced":
            endpoint = f"unix://{root / 'control.sock'}"
            limit = 2 * count + 128
            server_process, server_log = start([args.server, "--listen", endpoint, "--agent", args.agent,
                                   "--event-history-limit", str(limit)])
            server_log.expect(r"gRPC server listening")
            common = [args.repl, "--endpoint", endpoint, "--buffer-events", str(limit)]
            control, output = start([*common, "--attach", str(target.pid)])
            session = output.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]

            token = maps(control, output)
            send(control, f"%probe entry person_write_many --mapping {token}")
            output.expect(r"^entry=0x[0-9a-f]+ .*boundary=DWARF ")
            store = output.expect(
                r"^(0x[0-9a-f]+)  ((?:[0-9a-f]{2} )+) +movl\s+\$(?:0x)?1,.* next=(0x[0-9a-f]+)$"
                r"|^error:"
            )
            assert store[1], "".join(output.seen[-12:])
            pc, before, successor = int(store[1], 16), bytes.fromhex(store[2]), int(store[3], 16)
            assert successor == pc + len(before) and before[0] == 0xc7, before
            output.expect(r"^patch_size=")
            assert read_code(control, output, pc, token, len(before)) == before
            if method:
                send(control, f'%probe inject "{METHOD}" --backend entry')
                watch = output.expect(r"^installed probe=(\d+) .*|^error:")
                assert watch[1], "".join(output.seen[-12:])
                redirects = {}
            else:
                option = " --no-values" if mode == "no-values" else ""
                send(control, "%object start --type demo::Person --field age "
                              f"--code person_write_many --backend {backend} --stack-depth 0{option}")
                watch = output.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
                assert watch[1], "".join(output.seen[-12:])
                assert f"values={'off' if option else 'on'}" in watch.string
                redirects = read_redirects(control, output, watch, backend, target.pid, tids)
            armed = read_code(control, output, pc, maps(control, output), len(before))
            if not method:
                assert armed == (before if redirects else b"\xcc" + before[1:])

        send(target, "s")
        setup = application.expect(r"^SETUP base=(0x[0-9a-f]+) stride=(\d+) offset=(\d+)$")
        base, stride, offset = int(setup[1], 16), int(setup[2]), int(setup[3])
        if mode != "untraced":
            db, export = root / "writes.db", root / "writes.json"
            recorder, recording = start(common)
            expected_events = 4 if method else count * 2
            name = METHOD if method else "object.trace"
            send(recorder, f'%record {db} --sqlite --session {session} --name "{name}" '
                           f"--probe {watch[1]} --count {expected_events} --timeout {timeout}")
            # All creation rows are committed before releasing the workers.
            if not method:
                wait_counts(db, lambda n: n.get(target.pid) == count, timeout)

        begin_cpu = cpu_usage(target.pid)
        begin_server_cpu = cpu_usage(server_process.pid) if server_process else {}
        send(target, "g")
        if detach:
            wait_counts(db, lambda n: all(n.get(tid, 0) > 0 for tid in tids), timeout)
            send(target, "p")
            progress = tuple(map(int, application.expect(r"^PROGRESS completed=(\d+),(\d+)$").groups()))
            assert all(0 < n < per_worker for n in progress), progress
            send(control, "%detach")
            output.expect(rf"^detached session={re.escape(session)}$")

        done = application.expect(r"^DONE ns=(\d+) completed=(\d+),(\d+) correct=(\d+),(\d+)$")
        elapsed, *completed = map(int, done.groups())
        app_cpu = cpu_delta(target.pid, begin_cpu)
        worker_cpu = next((re.match(r"^CPU ns=(\d+),(\d+)$", row) for row in reversed(application.seen)
                           if row.startswith("CPU ns=")), None)
        server_cpu = cpu_delta(server_process.pid, begin_server_cpu) if server_process else 0
        assert completed == [per_worker] * 4, done.string
        if mode != "untraced":
            if detach:
                # Allow the already published prefix to reach SQLite before
                # cancelling the live recorder, after the timed interval.
                previous = read_counts(db)
                for _ in range(3):
                    time.sleep(0.02)
                    current = read_counts(db)
                    if current != previous:
                        previous = current
                recorder.send_signal(signal.SIGINT)
                recording.expect(r"^received=\d+ .*cancelled=true")
            else:
                recording.expect(rf"^received={expected_events} .*complete=true")
                if not method:
                    send(control, f"%object status {watch[1]}")
                    status = output.expect(rf"^watch={watch[1]} active=yes .*observations={count * 2} .*")
                    assert "dropped=0 " in status.string
                    if redirects:
                        assert f"executed_stores={count} " in status.string
                send(control, "%detach")
                output.expect(rf"^detached session={re.escape(session)}$")
            send(recorder, "%quit")
            assert recorder.wait(timeout=timeout) == 0, "".join(recording.seen[-12:])

            send(control, f"%attach {target.pid}")
            new_session = output.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
            assert new_session != session
            token = maps(control, output)
            restored = read_code(control, output, pc, token, len(before))
            assert restored == before
            verify_restored_redirects(control, output, token, target.pid, redirects)

            send(control, f"%events open {db} --as saved")
            opened = output.expect(r"^opened events=(\d+) retained=(\d+) evicted=0$")
            assert opened[1] == opened[2]
            send(control, f"%events export saved --format json --output {export}")
            send(control, "%detach")
            output.expect(rf"^detached session={re.escape(new_session)}$")
            source = Path(__file__).with_name("update.cpp")
            line = next(i for i, text in enumerate(source.read_text().splitlines(), 1)
                        if "STORE_LINE" in text)
            captured = json.loads(export.read_text())
            if method:
                assert len(captured) == 4
                assert {(event["tid"], event["phase"]) for event in captured} == {
                    (tid, phase) for tid in tids for phase in ("PRE", "POST")}
                assert all(not int(event.get("serverDroppedEvents", 0)) and
                           not int(event.get("agentDroppedEvents", 0)) for event in captured)
                counts = (2, 2)
            else:
                counts = verify_record(captured, mode, count, tids, base, stride, offset, pc, line, detach)
            send(control, "%quit")
            assert control.wait(timeout=timeout) == 0, "".join(output.seen[-12:])

        send(target, "q")
        application.expect(r"^EXIT$")
        assert target.wait(timeout=timeout) == 0
        report = f"trial={trial} mode={mode} elapsed_ns={elapsed} tids={tids}"
        if mode != "untraced":
            report += (f" backend={backend} store={pc:#x} source=update.cpp:{line}"
                       f" bytes={before.hex(' ')} -> {armed.hex(' ')} -> {restored.hex(' ')}"
                       f" restored_redirects={len(redirects)} recorded_by_worker={counts}")
        if detach:
            report += f" progress_before_detach={progress}"
        report += f" app_cpu_ns={app_cpu} server_cpu_ns={server_cpu} writes_per_sec={count * 1e9 / elapsed:.0f}"
        if worker_cpu:
            report += f" worker_cpu_ns={sum(map(int, worker_cpu.groups()))}"
        if mode != "untraced" and not detach:
            # Object creation occurs before the timed interval.
            report += f" write_phase_events_per_sec={(4 if method else count) * 1e9 / elapsed:.0f} dropped=0"
        print(report + f" completed_by_worker={(per_worker, per_worker)} target_exit=0", flush=True)
        return elapsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda path: str(Path(path).resolve()))
    parser.add_argument("--writes", type=int, default=2000)
    parser.add_argument("--trials", type=int, default=5)
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--detach", action="store_true", help="run active detach, without benchmarking")
    parser.add_argument("--method", action="store_true", help="also measure an ordinary native entry probe")
    parser.add_argument("--backend", choices=("ptrace", "code-cache", "both"), default="ptrace")
    args = parser.parse_args()
    if args.writes < 2 or args.writes % 2 or args.trials < 1 or args.timeout < 1:
        parser.error("writes must be positive and even, and trials and timeout must be positive")
    original = hashlib.sha256(Path(args.fixture).read_bytes()).digest()

    backends = ("ptrace", "code-cache") if args.backend == "both" else (args.backend,)
    if args.detach:
        for trial in range(1, args.trials + 1):
            for backend in backends:
                for mode in MODES[1:]:
                    run_case(args, backend, mode, True, trial)
    else:
        conditions = [("", "untraced")]
        if args.method:
            conditions.append(("entry", "method"))
        conditions.extend((backend, mode) for backend in backends for mode in MODES[1:])
        samples = {condition: [] for condition in conditions}
        for trial in range(args.trials):
            pivot = trial % len(conditions)
            for backend, mode in conditions[pivot:] + conditions[:pivot]:
                samples[backend, mode].append(run_case(args, backend, mode, False, trial + 1))
        medians = {}
        for (backend, mode), timings in samples.items():
            medians[backend, mode] = statistics.median(timings)
            print(f"{backend or 'none'} {mode} median_ns={medians[backend, mode]:.0f} min_ns={min(timings)} "
                  f"max_ns={max(timings)} trials_ns={timings}")
        for backend in backends:
            baseline = medians["", "untraced"]
            values, no_values = (medians[backend, mode] for mode in MODES[1:])
            saved = values - no_values
            print(f"{backend} values_overhead_ns={values-baseline:.0f} "
                  f"no_values_overhead_ns={no_values-baseline:.0f} saved_ns={saved:.0f} "
                  f"saved_percent_of_default_elapsed={100 * saved / values:.3f}")
    assert hashlib.sha256(Path(args.fixture).read_bytes()).digest() == original


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Exercise class based pre/post callbacks through the public CLI."""

from __future__ import annotations

import argparse
import re
import signal
import subprocess
import tempfile
import time
from pathlib import Path

from client_integration import decode_file, decode_frame, read_frames, require, write_text


def run(client: Path, endpoint: str, *args: str, ok: bool = True,
        timeout: float = 30) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run([str(client), "--endpoint", endpoint, *args],
                            capture_output=True, timeout=timeout, check=False)
    if ok:
        require(result.returncode == 0,
                f"CLI {' '.join(args)} failed: {result.stderr.decode(errors='replace')}")
    else:
        require(result.returncode != 0, f"CLI {' '.join(args)} unexpectedly succeeded")
    return result


def output(result: subprocess.CompletedProcess[bytes]) -> str:
    return (result.stdout + result.stderr).decode("utf-8", errors="replace")


def probe_id(text: str) -> int:
    match = re.search(r"\bprobe\s*=\s*(\d+)", text)
    require(match is not None, f"trace output lacks probe ID:\n{text}")
    return int(match.group(1))


def trace(client: Path, endpoint: str, pid: int, declaration: str, callback: Path,
          compdb: Path, source: Path) -> tuple[int, str]:
    text = output(run(client, endpoint, "trace", str(pid), declaration,
                      "--callback", str(callback), timeout=45))
    return probe_id(text), text


def decoded_events(protoc: Path, proto: Path, path: Path) -> list[str]:
    return decode_file(protoc, proto, "TraceEvent", path)


def wait_binary(client: Path, endpoint: str, pid: int, probe: int,
                output_dir: Path, protoc: Path, proto: Path, want: int,
                after: int) -> tuple[list[str], int]:
    deadline = time.monotonic() + 20
    cursor = after
    rows: list[str] = []
    while time.monotonic() < deadline and len(rows) < want:
        path = output_dir / f"probe-{probe}-after-{cursor}.pb"
        result = subprocess.run(
            [str(client), "--endpoint", endpoint, "--format", "protobuf",
             "--output", str(path), "events", str(pid), "--count", "8",
             "--after", str(cursor), "--timeout", "2"],
            capture_output=True, timeout=5, check=False)
        detail = output(result).lower()
        require(result.returncode == 0 or "timed out" in detail or "deadline exceeded" in detail,
                f"binary event stream failed: {output(result)}")
        if not path.exists() or not path.stat().st_size:
            continue
        for row in decoded_events(protoc, proto, path):
            sequence = re.search(r"^sequence:\s*(\d+)", row, re.M)
            if sequence:
                cursor = max(cursor, int(sequence.group(1)))
            if re.search(rf"^probe_id:\s*{probe}\s*$", row, re.M):
                rows.append(row)
    require(len(rows) >= want, f"did not receive {want} binary hook events: {rows}")
    return rows, cursor


def callback_value(row: str) -> int:
    match = re.search(r"^callback_value:\s*(-?\d+)", row, re.M)
    return int(match.group(1)) if match else 0


def wait_readable(client: Path, endpoint: str, pid: int, probe: int,
                  output_dir: Path, want: int = 2, after: int = 0,
                  batch_size: int = 8) -> tuple[list[str], int]:
    deadline = time.monotonic() + 20
    cursor = after
    rows: list[str] = []
    while time.monotonic() < deadline and len(rows) < want:
        count = batch_size
        result = subprocess.run([str(client), "--endpoint", endpoint, "events", str(pid),
                                 "--count", str(count), "--after", str(cursor),
                                 "--timeout", "2"], capture_output=True, timeout=5,
                                check=False)
        text = output(result)
        with (output_dir / "readable-events.log").open("a", encoding="utf-8") as event_log:
            event_log.write(f"--- probe={probe} after={cursor} rc={result.returncode} ---\n")
            event_log.write(text)
            if text and not text.endswith("\n"):
                event_log.write("\n")
        if result.returncode != 0:
            require("timed out" in text.lower() or "deadline exceeded" in text.lower(),
                    f"event stream failed for a reason other than its bounded timeout: {text}")
        for line in text.splitlines():
            sequence = re.search(r"\bcursor=(\d+)", line)
            if sequence:
                cursor = max(cursor, int(sequence.group(1)))
            if re.search(rf"\bprobe={probe}\b", line):
                rows.append(line)
        if len(rows) < want:
            time.sleep(.05)
    require(len(rows) >= want, f"did not receive {want} callback events: {rows}")
    return rows, cursor


def wait_event_text(client: Path, endpoint: str, pid: int, probe: int,
                    after: int = 0) -> str:
    """Scan one event at a time so a quiet stream need not fill a large count."""
    deadline = time.monotonic() + 20
    cursor = after
    last_text = ""
    while time.monotonic() < deadline:
        result = subprocess.run(
            [str(client), "--endpoint", endpoint, "events", str(pid),
             "--count", "1", "--after", str(cursor), "--timeout", "2"],
            capture_output=True, timeout=5, check=False)
        last_text = output(result)
        sequence = re.search(r"\bcursor=(\d+)", last_text)
        if sequence:
            cursor = max(cursor, int(sequence.group(1)))
        if result.returncode == 0 and re.search(rf"\bprobe={probe}\b", last_text):
            return last_text
        if result.returncode != 0:
            require("timed out" in last_text.lower() or
                    "deadline exceeded" in last_text.lower(),
                    f"event stream failed while scanning probe {probe}: {last_text}")
        time.sleep(.05)
    raise RuntimeError(
        f"did not receive readable event for probe {probe}; last output: {last_text}")


def watch_session_after(client: Path, endpoint: str, session_id: str, after: int,
                        output_dir: Path, protoc: Path,
                        proto: Path) -> tuple[list[str], int]:
    """Read bounded binary Watch batches for a specific active session."""
    request = output_dir / "detached-watch.textproto"
    write_text(request, f'session_ids: "{session_id}"\nafter_sequence: {after}\n')
    output_path = output_dir / "detached-watch.pb"
    result = subprocess.run(
        [str(client), "--endpoint", endpoint, "--request", str(request), "--output",
         str(output_path), "watch", "--count", "128", "--timeout", "1"],
        capture_output=True, timeout=4, check=False)
    detail = output(result).lower()
    require(result.returncode == 0 or "timed out" in detail or "deadline exceeded" in detail,
            f"WatchEvents failed for a reason other than its bounded timeout: {output(result)}")
    if not output_path.exists() or output_path.stat().st_size == 0:
        return [], after
    frames = read_frames(output_path)
    decoded = [decode_frame(protoc, proto, "TraceEvent", frame) for frame in frames]
    sequences = [int(match.group(1)) for row in decoded
                 if (match := re.search(r"^sequence:\s*(\d+)", row, re.M))]
    require(len(sequences) == len(decoded), f"WatchEvents frame lacks sequence: {decoded}")
    return decoded, max([after] + sequences)


def remove(client: Path, endpoint: str, pid: int, probe: int) -> None:
    run(client, endpoint, "remove", str(pid), str(probe))


def remove_after_callback_failure(client: Path, endpoint: str, pid: int, probe: int,
                                 protoc: Path, proto: Path, output_dir: Path,
                                 label: str) -> None:
    path = output_dir / f"remove-{label}.pb"
    result = run(client, endpoint, "--format", "protobuf", "--output", str(path),
                 "remove", str(pid), str(probe))
    if not path.exists():
        path.write_bytes(result.stdout)
    decoded = decode_file(protoc, proto, "Probe", path)
    errors = re.search(r"^callback_errors:\s*(\d+)", decoded[0], re.M)
    require(errors is not None and int(errors.group(1)) >= 1,
            f"RemoveProbe did not report a callback failure for {label}: {decoded}")


def latest_worker_stats(path: Path) -> dict[int, tuple[int, int]]:
    stats: dict[int, tuple[int, int]] = {}
    if not path.exists():
        return stats
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.search(r"^WORKER id=(\d+) tid=\d+ object=\S+ calls=(\d+) failures=(\d+)$",
                          line)
        if match:
            worker, calls, failures = (int(value) for value in match.groups())
            stats[worker] = (calls, failures)
    return stats


def wait_shared_context_log(path: Path, minimum_events: int,
                            timeout: float = 20.0) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
        events = re.findall(r"^shared-context event request\.completed states=\d+$",
                            text, re.M)
        if len(events) >= minimum_events:
            return text
        time.sleep(.05)
    raise RuntimeError(
        f"server did not decode {minimum_events} shared-context events:\n"
        f"{path.read_text(encoding='utf-8', errors='replace') if path.exists() else ''}")


def shared_context_event_count(path: Path) -> int:
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    return len(re.findall(r"^shared-context event request\.completed states=\d+$",
                          text, re.M))


def wait_context_watch_cursor(client: Path, endpoint: str, pid: int,
                              timeout: float = 20.0) -> int:
    deadline = time.monotonic() + timeout
    cursor = 0
    while time.monotonic() < deadline:
        result = subprocess.run(
            [str(client), "--endpoint", endpoint, "events", str(pid),
             "--count", "32", "--after", str(cursor), "--timeout", "2"],
            capture_output=True, timeout=5, check=False)
        text = output(result)
        pending = cursor
        for line in text.splitlines():
            match = re.search(r"\bcursor=(\d+)", line)
            if match:
                pending = int(match.group(1))
            if "shared-context event=request.completed" in line:
                return pending
        if result.returncode != 0:
            require("timed out" in text.lower() or "deadline exceeded" in text.lower(),
                    f"WatchEvents failed while locating shared context: {text}")
        cursor = max(cursor, pending)
        time.sleep(.05)
    raise RuntimeError("WatchEvents did not expose a shared-context event")


def latest_shared_child_count(path: Path) -> int:
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    values = re.findall(r"shared_children=(\d+)\b", text)
    return int(values[-1]) if values else 0


def wait_for_shared_pre_diagnostic(path: Path, after: int,
                                   timeout: float = 8.0) -> str:
    deadline = time.monotonic() + timeout
    pattern = re.compile(
        r"^clang-tracer context diagnostic "
        r"(?=[^\n]*\breason=hook_failure\b)"
        r"(?=[^\n]*\bphase=pre\b)"
        r"(?=[^\n]*\baggregate=0\b)"
        r"[^\n]*\bcall_id=([1-9]\d*)\b[^\n]*$", re.M)
    while time.monotonic() < deadline:
        text = path.read_bytes()[after:].decode("utf-8", errors="replace") \
            if path.exists() else ""
        match = pattern.search(text)
        if match:
            require(int(match.group(1)) > 0,
                    f"shared PRE diagnostic lacks a native call ID: {match.group(0)}")
            return match.group(0)
        time.sleep(.05)
    tail = path.read_bytes()[after:].decode("utf-8", errors="replace") \
        if path.exists() else ""
    raise RuntimeError(f"target did not emit a correlated shared PRE diagnostic:\n{tail}")


def wait_server_ready(client: Path, endpoint: str,
                      server: subprocess.Popen[bytes], timeout: float = 12.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        require(server.poll() is None, "server exited during startup")
        result = subprocess.run([str(client), "--endpoint", endpoint, "list"],
                                capture_output=True, timeout=2, check=False)
        if result.returncode == 0:
            return
        time.sleep(.1)
    raise RuntimeError("server did not become ready")


def stop(process: subprocess.Popen[bytes] | None) -> None:
    if process is None:
        return
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser()
    for key in ("server", "client", "agent", "target", "threaded-target", "protoc",
                "proto", "compdb", "source", "threaded-source", "output-dir"):
        parser.add_argument("--" + key, required=True, type=Path)
    args = parser.parse_args()
    for key in ("server", "client", "agent", "target", "threaded_target", "protoc",
                "proto", "compdb", "source", "threaded_source"):
        value = getattr(args, key)
        require(value.exists(), f"missing {key}: {value}")
        setattr(args, key, value.resolve())
    args.output_dir = args.output_dir.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="clang-tracer-interceptor-") as temp:
        root = Path(temp)
        endpoint = f"unix://{root}/server.sock"
        # Durable collector state must be isolated per test run. The restart
        # scenario below reuses this directory within the run; later CTest
        # invocations must begin with a fresh CREATE checkpoint.
        durable_directory = root / "context-journal"
        durable_directory.mkdir()
        target_log = (args.output_dir / "target.log").open("wb")
        server_log = (args.output_dir / "server.log").open("wb")
        target = server = threaded = None
        report: list[str] = []
        try:
            target = subprocess.Popen([str(args.target)], stdout=target_log,
                                      stderr=target_log)
            server = subprocess.Popen([str(args.server), "--agent", str(args.agent),
                                       "--listen", endpoint, "--timeout", "10",
                                       "--context-durable-dir", str(durable_directory)],
                                      stdout=server_log, stderr=server_log)
            ready_by = time.monotonic() + 12
            while time.monotonic() < ready_by:
                require(server.poll() is None, "server exited during startup")
                probe = subprocess.run([str(args.client), "--endpoint", endpoint, "list"],
                                       capture_output=True, timeout=2, check=False)
                if probe.returncode == 0:
                    break
                time.sleep(.1)
            else:
                raise RuntimeError("server did not become ready")

            attached = output(run(args.client, endpoint, "attach", str(target.pid)))
            require(re.search(r"state=ATTACHED", attached, re.I),
                    f"could not attach to class target: {attached}")
            session_match = re.search(r"\bsession=([^\s]+)", attached)
            require(session_match is not None, f"attach output lacks session ID: {attached}")
            session_id = session_match.group(1)

            callback = args.output_dir / "canonical-interceptor.cpp"
            callback.write_text((Path(__file__).resolve().parents[1] /
                                 "examples/interceptor.cpp").read_text(encoding="utf-8"),
                                encoding="utf-8")
            probe, text = trace(args.client, endpoint, target.pid,
                                "Fixture::overloaded(int)", callback,
                                args.compdb, args.source)
            require("Fixture::overloaded(int)" in text, "CLI did not resolve the int overload")
            rows, cursor = wait_readable(args.client, endpoint, target.pid, probe,
                                         args.output_dir, 4)
            event_cursor = cursor
            phases: dict[int, dict[str, str]] = {}
            for line in rows:
                phase = re.search(r"\bphase=(pre|post)", line)
                call = re.search(r"\bcall=(\d+)", line)
                argument = re.search(r"args=\[(?:\w+=)?int32:(-?\d+)", line)
                event_pid = re.search(r"\bpid=(\d+)", line)
                event_tid = re.search(r"\btid=(\d+)", line)
                result_value = re.search(r"result=int32:(-?\d+)", line)
                require(phase and call and argument and event_pid and event_tid,
                        f"readable event omitted typed call data: {line}")
                item = phases.setdefault(int(call.group(1)), {})
                item[phase.group(1)] = line
                item[phase.group(1) + "-pid"] = event_pid.group(1)
                item[phase.group(1) + "-tid"] = event_tid.group(1)
                if phase.group(1) == "post":
                    require(result_value is not None,
                            f"post event lacks original method result: {line}")
                if phase.group(1) == "post":
                    require(result_value is not None and
                            int(result_value.group(1)) == int(argument.group(1)) + 11,
                            f"post event does not contain the original method result: {line}")
            complete_pairs = [pair for pair in phases.values()
                              if "pre" in pair and "post" in pair]
            require(complete_pairs,
                    f"pre/post events did not share a call ID: {rows}")
            require(any(pair["pre-pid"] == pair["post-pid"] == str(target.pid) and
                        pair["pre-tid"] == pair["post-tid"] for pair in complete_pairs),
                    f"paired events changed PID or TID: {complete_pairs}")
            require(any(re.search(r"\bthis=0x(?!0+$)[0-9a-f]+", line, re.I)
                        for line in rows), "method events lack object addresses")

            # Decode wire events and check phase, call correlation, and typed result fields.
            binary_path = args.output_dir / "class-events.pb"
            result = run(args.client, endpoint, "--format", "protobuf", "--output",
                         str(binary_path), "events", str(target.pid), "--count", "16",
                         "--after", "0", "--timeout", "8", timeout=12)
            if not binary_path.exists():
                binary_path.write_bytes(result.stdout)
            binary = decoded_events(args.protoc, args.proto, binary_path)
            require(len(binary) >= 2, f"expected paired protobuf events, got {len(binary)}")
            event_cursor = max([event_cursor] + [int(match.group(1)) for row in binary
                                 if (match := re.search(r"^sequence:\s*(\d+)", row, re.M))])
            by_call: dict[str, dict[str, str]] = {}
            for row in binary:
                call_match = re.search(r"^call_id:\s*(\d+)", row, re.M)
                phase_match = re.search(r"^phase:\s*(PRE|POST)", row, re.M)
                if call_match and phase_match:
                    by_call.setdefault(call_match.group(1), {})[phase_match.group(1)] = row
            paired = next((pair for pair in by_call.values()
                           if "PRE" in pair and "POST" in pair), None)
            require(paired is not None, f"protobuf frames contain no complete call pair: {binary}")
            pre_row = paired["PRE"]
            post_row = paired["POST"]
            pre_pid = re.search(r"^pid:\s*(\d+)", pre_row, re.M)
            post_pid = re.search(r"^pid:\s*(\d+)", post_row, re.M)
            pre_tid = re.search(r"^tid:\s*(\d+)", pre_row, re.M)
            post_tid = re.search(r"^tid:\s*(\d+)", post_row, re.M)
            argument = re.search(r"arguments\s*\{\s*type: INT32\s+signed_value:\s*(-?\d+)",
                                 post_row, re.S)
            actual_result = re.search(r"result\s*\{\s*type: INT32\s+signed_value:\s*(-?\d+)",
                                      post_row, re.S)
            require(pre_pid and post_pid and pre_tid and post_tid and
                    pre_pid.group(1) == post_pid.group(1) == str(target.pid) and
                    pre_tid.group(1) == post_tid.group(1),
                    f"protobuf pair changed PID or TID: {paired}")
            require(argument and actual_result and
                    int(actual_result.group(1)) == int(argument.group(1)) + 11,
                    f"protobuf post result is not the original method result: {paired}")
            require(callback_value(pre_row) == int(argument.group(1)) * 100 + 17 and
                    callback_value(post_row) == int(actual_result.group(1)),
                    f"callback return values differ from the typed call: {paired}")
            require("result {" in post_row and "signed_value:" in post_row,
                    f"post event lacks the original typed result: {post_row}")
            remove(args.client, endpoint, target.pid, probe)
            report.append("class pre/post events exposed typed arguments, results, object address, and matching call IDs")

            # Optional pre-only and post-only methods, including a void-returning hook.
            cases = (
                ("pre-only", "class Interceptor { public: void pre(const Call &call) { (void)call.arg<int>(0); } };", "int"),
                ("post-only", "class Interceptor { public: long long post(const Call &call) { return call.result<int>(); } };", "int"),
                ("void-hook", "class Interceptor { public: void post(const Call &call) { call.result<void>(); } };", "void"),
            )
            for label, source, decl_type in cases:
                hook = args.output_dir / f"{label}.cpp"
                write_text(hook, source)
                declaration = "Fixture::overloaded(int)" if decl_type == "int" else "traced_void(int)"
                active, _ = trace(args.client, endpoint, target.pid, declaration,
                                  hook, args.compdb, args.source)
                expect = 1 if label == "pre-only" else 2
                events, cursor = wait_readable(args.client, endpoint, target.pid, active,
                                               args.output_dir, expect, event_cursor)
                event_cursor = cursor
                observed_phases = {re.search(r"\bphase=(pre|post)", row).group(1)
                                   for row in events}
                require(observed_phases == ({"pre"} if label == "pre-only" else {"post"}),
                        f"{label} emitted unexpected phases: {events}")
                if label == "void-hook":
                    void_path = args.output_dir / "void-result.pb"
                    result = run(args.client, endpoint, "--format", "protobuf", "--output",
                                 str(void_path), "events", str(target.pid), "--count", "1",
                                 "--after", str(cursor), "--timeout", "5", timeout=8)
                    if not void_path.exists():
                        void_path.write_bytes(result.stdout)
                    void_rows = decoded_events(args.protoc, args.proto, void_path)
                    require(len(void_rows) == 1 and "phase: POST" in void_rows[0] and
                            re.search(r"result\s*\{\s*type: VOID\s*\}", void_rows[0]),
                            f"void hook did not publish an explicit VOID result: {void_rows}")
                remove(args.client, endpoint, target.pid, active)
            report.append("pre-only, post-only, and void post hooks ran through CLI tracing")

            # A fresh object spans each PRE/POST pair; member data must never
            # accumulate across invocations or be shared by concurrent calls.
            member_hook = args.output_dir / "invocation-members.cpp"
            write_text(member_hook, """struct Interceptor {
              int visits = 0;
              long long pre(Call&) { return ++visits; }
              long long post(const Call&) { return ++visits; }
            };""")
            active, _ = trace(args.client, endpoint, target.pid,
                              "Fixture::overloaded(int)", member_hook,
                              args.compdb, args.source)
            rows, event_cursor = wait_binary(args.client, endpoint, target.pid, active,
                                            args.output_dir, args.protoc, args.proto,
                                            6, event_cursor)
            for row in rows:
                expected = 1 if re.search(r"^phase: PRE$", row, re.M) else 2
                require(callback_value(row) == expected,
                        f"interceptor member lifetime crossed an invocation: {row}")
            remove(args.client, endpoint, target.pid, active)
            report.append("each invocation used fresh member state shared by its PRE/POST pair")

            # A failed PRE disables that invocation's POST, while later
            # invocations still execute their hooks and the original target.
            recovery_hook = args.output_dir / "invocation-failure.cpp"
            write_text(recovery_hook, """struct Interceptor {
              long long pre(const Call& call) {
                if (call.id() % 2) throw 7;
                return 1;
              }
              long long post(const Call&) { return 2; }
            };""")
            active, _ = trace(args.client, endpoint, target.pid,
                              "Fixture::overloaded(int)", recovery_hook,
                              args.compdb, args.source)
            rows, event_cursor = wait_binary(args.client, endpoint, target.pid, active,
                                            args.output_dir, args.protoc, args.proto,
                                            4, event_cursor)
            for row in rows:
                call = re.search(r"^call_id:\s*(\d+)", row, re.M)
                require(call and int(call.group(1)) % 2 == 0,
                        f"failed invocation emitted a successful hook event: {row}")
            remove_after_callback_failure(args.client, endpoint, target.pid, active,
                                          args.protoc, args.proto, args.output_dir,
                                          "invocation-recovery")
            report.append("PRE failure skipped only the affected invocation and later invocations recovered")

            # Exercise the full schema path through a native target call. The
            # nested vector/map/optional types are compiled from this exact
            # source by both probes and decoded by the server's dynamic pool.
            shared_package = root / "shared-context-package"
            (shared_package / ".git").mkdir(parents=True)
            state_hook = shared_package / "shared_context_nested.cpp"
            write_text(state_hook,
                       (Path(__file__).resolve().parent / "fixtures" /
                        "shared_context_nested.cpp").read_text(encoding="utf-8"))

            def trace_shared(declaration: str) -> tuple[int, str]:
                text = output(run(args.client, endpoint, "trace", str(target.pid),
                                  declaration, "--callback", str(state_hook),
                                  timeout=60))
                return probe_id(text), text

            join_probe, join_text = trace_shared("shared_context_spawn_join(int)")
            child_probe, child_text = trace_shared("shared_child_target(int)")
            require(join_probe != child_probe and "shared_context_spawn_join" in join_text and
                    "shared_child_target" in child_text,
                    f"shared-context producer probes did not activate: {join_text}; {child_text}")
            context_log = wait_shared_context_log(args.output_dir / "server.log", 4)
            for expected in ('request_id: "request-42"', 'sku: "book"',
                             "quantity: 2", 'key: "billing"',
                             "calls: 1", "durations_ms: 12.5"):
                require(expected in context_log,
                        f"dynamic shared-state receiver omitted {expected!r}: {context_log}")
            call_one = context_log.find("calls: 1")
            call_two = context_log.find("calls: 2", call_one + 1)
            require(call_one >= 0 and call_two > call_one,
                    "cross-probe child event did not precede the joined parent event")
            context_cursor = wait_context_watch_cursor(
                args.client, endpoint, target.pid)
            event_after = max(0, context_cursor - 1)
            watch = run(args.client, endpoint, "events", str(target.pid),
                        "--count", "1", "--after", str(event_after), timeout=10)
            watch_text = output(watch)
            require("shared-context event=request.completed" in watch_text and
                    'request_id [revision=' in watch_text and
                    'string:"request-42"' in watch_text and
                    "items.sku [revision=" in watch_text and 'string:"book"' in watch_text,
                    f"WatchEvents did not expose decoded nested shared state: {watch_text}")
            query = run(args.client, endpoint, "events", str(target.pid),
                        "--count", "1", "--after", str(event_after),
                        "--context-field", "request_id,items.sku", timeout=10)
            query_text = output(query)
            require("request_id [revision=" in query_text and
                    '=string:"request-42"' in query_text and
                    re.search(r'items\.sku \[revision=[^\]]+\] =\[[^\]]*'
                              r'string:"book"', query_text) is not None,
                    f"typed multi-field query missed selected values: {query_text}")
            missing_query = run(args.client, endpoint, "events", str(target.pid),
                                "--count", "1", "--after", str(event_after),
                                "--context-field", "future_field", timeout=10)
            missing_text = output(missing_query)
            require("future_field [revision=" in missing_text and
                    "=<missing>" in missing_text,
                    f"cross-revision query did not report a schema-missing field: {missing_text}")
            target_output = (args.output_dir / "target.log").read_text(
                encoding="utf-8", errors="replace")
            require(re.search(r"shared_join=32\b", target_output) and
                    re.search(r"shared_early=48\b", target_output),
                    "original join/early-return target functions did not preserve their results")
            remove(args.client, endpoint, target.pid, child_probe)
            drain_started = time.monotonic()
            remove(args.client, endpoint, target.pid, join_probe)
            task_drain_seconds = time.monotonic() - drain_started
            require(task_drain_seconds >= .25,
                    f"removal did not wait for accepted child tasks ({task_drain_seconds:.3f}s)")
            report.append("shared aliases, nested map/vector state, typed event decode, cross-probe order, and accepted pthread drain passed")

            # Stack metadata must survive the shared-context wire envelope and
            # appear through the same WatchEvents viewer used by ordinary hooks.
            stack_events_before = shared_context_event_count(
                args.output_dir / "server.log")
            stack_probe_text = output(run(
                args.client, endpoint, "trace", str(target.pid),
                "shared_child_target(int)", "--callback", str(state_hook),
                "--stack", "4", timeout=60))
            stack_probe = probe_id(stack_probe_text)
            wait_shared_context_log(args.output_dir / "server.log",
                                    stack_events_before + 1)
            stack_watch = wait_event_text(
                args.client, endpoint, target.pid, stack_probe)
            require(re.search(rf"probe={stack_probe}.*?entry-stack "
                              r"(?:COMPLETE|TRUNCATED|PARTIAL) depth=\d+/4",
                              stack_watch, re.S) is not None,
                    f"shared-context WatchEvents omitted the captured entry stack: {stack_watch}")
            remove(args.client, endpoint, target.pid, stack_probe)
            report.append("shared-context WatchEvents retained and displayed bounded entry stacks")

            # A PRE callback failure on the child must not cancel the original
            # pthread target. Keep the outer shared-state probe active so the
            # child crosses the real scheduler propagation path.
            failing_hook = shared_package / "shared-context-failing-pre.cpp"
            write_text(failing_hook, state_hook.read_text(encoding="utf-8").replace(
                'state.request_id = "request-42";', 'throw 7;'))
            target_log_path = args.output_dir / "target.log"
            diagnostic_start = target_log_path.stat().st_size
            failed_child_text = output(run(
                args.client, endpoint, "trace", str(target.pid),
                "shared_child_target(int)", "--callback", str(failing_hook),
                timeout=60))
            failed_child_probe = probe_id(failed_child_text)
            children_before_failure = latest_shared_child_count(
                args.output_dir / "target.log")
            child_deadline = time.monotonic() + 12
            while (time.monotonic() < child_deadline and
                   latest_shared_child_count(args.output_dir / "target.log") <=
                       children_before_failure):
                time.sleep(.05)
            require(latest_shared_child_count(args.output_dir / "target.log") >
                        children_before_failure,
                    "child target did not execute after shared PRE callback failure")
            remove_after_callback_failure(
                args.client, endpoint, target.pid, failed_child_probe,
                args.protoc, args.proto, args.output_dir, "shared-child-pre")
            wait_for_shared_pre_diagnostic(target_log_path, diagnostic_start)
            require(target.poll() is None,
                    "target did not survive shared child PRE callback failure")
            remove(args.client, endpoint, target.pid, join_probe)
            report.append("shared child PRE failure was contained and original pthread target still ran")
            time.sleep(1.6)  # Let the previously accepted detached child finish.

            # The native throw target is caught by the fixture main loop. Its
            # shared POST event must be skipped when the original unwinds.
            throw_probe, _ = trace_shared("shared_context_native_throw(int)")
            target_log_path = args.output_dir / "target.log"
            target_log_size = target_log_path.stat().st_size
            context_events_before_throw = shared_context_event_count(
                args.output_dir / "server.log")
            throw_deadline = time.monotonic() + 12
            observed_caught_throw = False
            while time.monotonic() < throw_deadline:
                with target_log_path.open("rb") as log:
                    log.seek(target_log_size)
                    new_text = log.read().decode("utf-8", errors="replace")
                if re.search(r"shared_exceptions=1\b", new_text):
                    observed_caught_throw = True
                    break
                time.sleep(.05)
            require(observed_caught_throw,
                    "native target exception was not caught by the fixture")
            remove(args.client, endpoint, target.pid, throw_probe)
            require(shared_context_event_count(args.output_dir / "server.log") ==
                        context_events_before_throw,
                    "shared POST event was emitted after the native target threw")
            report.append("native target exception skipped shared POST and remained contained")

            # The producer journal and collector commit ledger must survive a
            # hard collector restart. The active target keeps its exporter
            # connected to the same endpoint and resumes from the persisted
            # durable acknowledgement.
            reconnect_probe, _ = trace_shared("shared_child_target(int)")
            before_restart = shared_context_event_count(args.output_dir / "server.log")
            wait_shared_context_log(args.output_dir / "server.log",
                                    before_restart + 1)
            server.kill()
            server.wait(timeout=5)
            server = None
            Path(root, "server.sock").unlink(missing_ok=True)
            server_log.flush()
            server = subprocess.Popen(
                [str(args.server), "--agent", str(args.agent), "--listen", endpoint,
                 "--timeout", "10", "--context-durable-dir",
                 str(durable_directory)], stdout=server_log, stderr=server_log)
            wait_server_ready(args.client, endpoint, server)
            wait_shared_context_log(args.output_dir / "server.log",
                                    before_restart + 2, timeout=25)
            require(target.poll() is None,
                    "target did not survive durable collector restart")
            restarted_watch = wait_event_text(
                args.client, endpoint, target.pid, reconnect_probe)
            require("shared-context event=request.completed" in restarted_watch and
                    "request_id [revision=" in restarted_watch and
                    'string:"request-42"' in restarted_watch and
                    "items.sku [revision=" in restarted_watch and
                    'string:"book"' in restarted_watch,
                    f"post-restart events PID viewer omitted shared payload: {restarted_watch}")
            report.append("durable reconnect resumed with its PID, probe, and shared payload in Watch")

            # The collector restart intentionally forgets control sessions;
            # reattach before issuing later trace/remove operations.
            reattached = output(run(args.client, endpoint, "attach", str(target.pid)))
            require(re.search(r"state=ATTACHED", reattached, re.I),
                    f"could not reattach class target after collector restart: {reattached}")
            session_match = re.search(r"\bsession=([^\s]+)", reattached)
            require(session_match is not None,
                    f"reattach output lacks session ID: {reattached}")
            session_id = session_match.group(1)
            event_cursor = 0  # The restarted server's event sequence is fresh.

            # Floating point and pointer Call accessors use the same paired hooks.
            typed_cases = (
                ("double", "Fixture::inspect(double) const",
                 "class Interceptor { public: void pre(const Call &call) { (void)call.arg<double>(0); } void post(const Call &call) { (void)call.result<double>(); } };"),
                ("pointer", "traced_pointer(void*)",
                 "class Interceptor { public: void pre(const Call &call) { (void)call.arg<void*>(0); } void post(const Call &call) { (void)call.result<void*>(); } };"),
            )
            for label, declaration, source in typed_cases:
                hook = args.output_dir / f"{label}-hook.cpp"
                write_text(hook, source)
                active, _ = trace(args.client, endpoint, target.pid, declaration,
                                  hook, args.compdb, args.source)
                _, event_cursor = wait_readable(args.client, endpoint, target.pid, active,
                                                args.output_dir, 2, event_cursor)
                remove(args.client, endpoint, target.pid, active)
            report.append("Call accessors accepted double and pointer arguments and results")

            # A class that fails to compile is rejected before a probe is installed.
            bad = args.output_dir / "malformed-interceptor.cpp"
            write_text(bad, "class Interceptor { long long pre(const Call &call) { return call.arg<int>(0); }")
            for label, source_text in (("malformed", bad),
                                       ("no-hook", args.output_dir / "no-hook.cpp")):
                if label == "no-hook":
                    write_text(source_text, "class Interceptor {};")
                failed = run(args.client, endpoint, "trace", str(target.pid),
                             "Fixture::overloaded(int)", "--callback", str(source_text),
                             ok=False, timeout=45)
                require(output(failed).strip(), f"{label} callback produced no diagnostic")
            report.append("malformed and no-hook class callbacks were rejected with diagnostics")

            # Runtime callback exceptions from bad Call<T>/index usage fail closed.
            for label, body in (("wrong-type", "call.arg<double>(0)"),
                                ("wrong-index", "call.arg<int>(99)"),
                                ("wrong-result-pre", "call.result<int>()"),
                                ("throwing", "throw 7")):
                hook = args.output_dir / f"{label}.cpp"
                write_text(hook, f"class Interceptor {{ public: long long pre(const Call &call) {{ {body}; return 9; }} }};")
                active, _ = trace(args.client, endpoint, target.pid,
                                  "Fixture::overloaded(int)", hook,
                                  args.compdb, args.source)
                time.sleep(.4)
                # RemoveProbe reports the agent's contained callback failure count.
                remove_after_callback_failure(args.client, endpoint, target.pid, active,
                                              args.protoc, args.proto, args.output_dir, label)
                require(target.poll() is None, f"target died after {label} callback")
            target_rows = (args.output_dir / "target.log").read_text(
                encoding="utf-8", errors="replace")
            method_rows = re.findall(
                r"^TARGET pid=(\d+).*?arg=(-?\d+).*?method=(-?\d+)",
                target_rows, re.M)
            require(method_rows, "target produced no original method result after failing hooks")
            require(all(int(row_pid) == target.pid and int(value) == int(arg) + 11
                        for row_pid, arg, value in method_rows),
                    "class callback failure changed original method results")
            report.append("throwing and invalid Call access incremented callback_errors while original calls remained correct")

            # Exercise the same class against worker threads, then detach under load.
            threaded_log = (args.output_dir / "threaded-target.log").open("wb")
            threaded = subprocess.Popen([str(args.threaded_target), "--busy-main"], stdout=threaded_log,
                                        stderr=threaded_log)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and not (args.output_dir / "threaded-target.log").stat().st_size:
                time.sleep(.05)
            thread_attached = output(run(args.client, endpoint, "attach", str(threaded.pid)))
            require(re.search(r"state=ATTACHED", thread_attached, re.I),
                    f"could not attach to threaded target: {thread_attached}")
            thread_session_match = re.search(r"\bsession=([^\s]+)", thread_attached)
            require(thread_session_match is not None,
                    f"threaded attach output lacks session ID: {thread_attached}")
            thread_session_id = thread_session_match.group(1)
            thread_hook = args.output_dir / "threaded-slow.cpp"
            write_text(thread_hook, "#include <thread>\n#include <chrono>\nclass Interceptor { public: void pre(const Call &) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); } void post(const Call &) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); } };")
            method_probe, _ = trace(args.client, endpoint, threaded.pid,
                                    "threaded::Worker::work(int)", thread_hook,
                                    args.compdb, args.threaded_source)
            free_probe, _ = trace(args.client, endpoint, threaded.pid,
                                  "threaded::namespaced_work(int)", thread_hook,
                                  args.compdb, args.threaded_source)
            method_events, thread_cursor = wait_readable(
                args.client, endpoint, threaded.pid, method_probe, args.output_dir, 6,
                batch_size=128)
            free_events, free_cursor = wait_readable(
                args.client, endpoint, threaded.pid, free_probe, args.output_dir, 6,
                thread_cursor, batch_size=128)
            thread_cursor = max(thread_cursor, free_cursor)
            tids = {int(re.search(r"\btid=(\d+)", line).group(1))
                    for line in method_events + free_events}
            require(len(tids) >= 2, f"callbacks did not run concurrently on worker threads: {tids}")
            stats_before = latest_worker_stats(args.output_dir / "threaded-target.log")
            require(len(stats_before) == 4,
                    f"threaded target did not report all workers before detach: {stats_before}")
            require(all(failures == 0 for _, failures in stats_before.values()),
                    f"workers reported incorrect target results before detach: {stats_before}")
            detached = output(run(args.client, endpoint, "detach", str(threaded.pid), timeout=20))
            require(re.search(r"state=DETACHED", detached, re.I),
                    f"detach did not complete: {detached}")
            for _ in range(4):
                pending, thread_cursor = watch_session_after(
                    args.client, endpoint, thread_session_id, thread_cursor,
                    args.output_dir, args.protoc, args.proto)
                if not pending:
                    break
            else:
                raise RuntimeError("WatchEvents kept receiving data after detach")
            time.sleep(.25)
            no_more, updated_cursor = watch_session_after(
                args.client, endpoint, thread_session_id, thread_cursor,
                args.output_dir, args.protoc, args.proto)
            require(not no_more and updated_cursor == thread_cursor,
                    f"events continued after detach: {no_more}")
            require(threaded.poll() is None, "threaded target exited during detach")
            progress_deadline = time.monotonic() + 3
            stats_after = latest_worker_stats(args.output_dir / "threaded-target.log")
            while time.monotonic() < progress_deadline and not all(
                    worker in stats_after and stats_after[worker][0] > previous[0]
                    for worker, previous in stats_before.items()):
                time.sleep(.05)
                stats_after = latest_worker_stats(args.output_dir / "threaded-target.log")
            require(all(worker in stats_after and stats_after[worker][0] > previous[0]
                        for worker, previous in stats_before.items()),
                    f"worker calls stopped advancing after detach: before={stats_before}, after={stats_after}")
            require(all(failures == 0 for _, failures in stats_after.values()),
                    f"workers reported incorrect results after detach: {stats_after}")
            task_statuses = list(Path(f"/proc/{threaded.pid}/task").glob("*/status"))
            require(bool(task_statuses), "threaded target has no live tasks after detach")
            for status in task_statuses:
                try:
                    status_text = status.read_text(encoding="utf-8", errors="replace")
                except FileNotFoundError:
                    continue
                tracer_pid = re.search(r"^TracerPid:\s*(\d+)", status_text, re.M)
                require(tracer_pid is not None and tracer_pid.group(1) == "0",
                        f"task remains traced after detach: {status}")
            report.append("class hooks ran on multiple target threads and detach stopped further events while target remained live")

            (args.output_dir / "report.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
        finally:
            stop(threaded)
            stop(target)
            stop(server)
            target_log.close()
            server_log.close()
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

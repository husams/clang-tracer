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
                      "--callback", str(callback), "--compdb", str(compdb),
                      "--source", str(source), timeout=45))
    return probe_id(text), text


def decoded_events(protoc: Path, proto: Path, path: Path) -> list[str]:
    return decode_file(protoc, proto, "TraceEvent", path)


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


def watch_session_after(client: Path, endpoint: str, session_id: str, after: int,
                        output_dir: Path, protoc: Path, proto: Path) -> tuple[list[str], int]:
    """Use WatchEvents directly so a detached PID need not resolve to an active session."""
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
        target_log = (args.output_dir / "target.log").open("wb")
        server_log = (args.output_dir / "server.log").open("wb")
        target = server = threaded = None
        report: list[str] = []
        try:
            target = subprocess.Popen([str(args.target)], stdout=target_log,
                                      stderr=target_log)
            server = subprocess.Popen([str(args.server), "--agent", str(args.agent),
                                       "--listen", endpoint, "--timeout", "10"],
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
                value = re.search(r"\bcallback=(-?\d+)", line)
                argument = re.search(r"args=\[int32:(-?\d+)", line)
                event_pid = re.search(r"\bpid=(\d+)", line)
                event_tid = re.search(r"\btid=(\d+)", line)
                result_value = re.search(r"result=int32:(-?\d+)", line)
                require(phase and call and value and argument and event_pid and event_tid,
                        f"readable event omitted typed call data: {line}")
                item = phases.setdefault(int(call.group(1)), {})
                item[phase.group(1)] = line
                item[phase.group(1) + "-pid"] = event_pid.group(1)
                item[phase.group(1) + "-tid"] = event_tid.group(1)
                if phase.group(1) == "pre":
                    expected = int(argument.group(1)) * 100 + 17
                else:
                    require(result_value is not None,
                            f"post event lacks original method result: {line}")
                    expected = int(result_value.group(1))
                require(int(value.group(1)) == expected,
                        f"wrong {phase.group(1)} callback value: {line}")
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
                             "--compdb", str(args.compdb), "--source", str(args.source),
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

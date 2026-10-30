#!/usr/bin/env python3
"""Exercise the human-readable clang-tracer client against owned fixtures."""

from __future__ import annotations

import argparse
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# Reuse fixture/process and protobuf framing checks from the legacy CLI test.
from client_integration import decode_file, require, target_rows, wait_for_rows, write_text


class FriendlyClient:
    def __init__(self, client: Path, endpoint: str | None):
        self.client = client
        self.endpoint = endpoint

    def run(self, *arguments: str, timeout: float = 12.0,
            expected_ok: bool | None = True, env: dict[str, str] | None = None
            ) -> subprocess.CompletedProcess[bytes]:
        command = [str(self.client)]
        if self.endpoint is not None:
            command.extend(("--endpoint", self.endpoint))
        command.extend(arguments)
        completed = subprocess.run(
            command,
            capture_output=True,
            timeout=timeout,
            check=False,
            env=env,
        )
        if expected_ok is True:
            require(completed.returncode == 0,
                    f"CLI {' '.join(arguments)} failed ({completed.returncode}): "
                    f"{completed.stderr.decode(errors='replace')}")
        elif expected_ok is False:
            require(completed.returncode != 0,
                    f"CLI {' '.join(arguments)} unexpectedly succeeded")
        return completed


def cli_text(result: subprocess.CompletedProcess[bytes]) -> str:
    return (result.stdout + result.stderr).decode("utf-8", errors="replace")


def has_field(text: str, field: str, value: str | int) -> bool:
    return re.search(rf"\b{re.escape(field)}\b\s*[:=]?\s*{re.escape(str(value))}\b",
                     text, re.IGNORECASE) is not None


def save_output(path: Path, result: subprocess.CompletedProcess[bytes]) -> str:
    text = cli_text(result)
    write_text(path, text)
    return text


def check_events(text: str, pid: int, probe_ids: set[int], minimum: int = 1) -> int:
    event_lines = [line for line in text.splitlines() if re.search(r"(?i)\bcursor=", line)]
    require(len(event_lines) >= minimum, f"expected {minimum} readable events:\n{text}")
    cursors: list[int] = []
    seen_probes: set[int] = set()
    for line in event_lines:
        cursor = re.search(r"(?i)\bcursor=(\d+)", line)
        event_pid = re.search(r"(?i)\bpid=(\d+)", line)
        probe = re.search(r"(?i)\bprobe=(\d+)", line)
        require(cursor is not None and event_pid is not None and probe is not None,
                f"event lacks cursor, PID, or probe ID:\n{line}")
        require(int(event_pid.group(1)) == pid, f"event has wrong PID:\n{line}")
        cursors.append(int(cursor.group(1)))
        seen_probes.add(int(probe.group(1)))
        require(re.search(r"(?i)lost\(agent=\d+,server=\d+\)", line) is not None,
                f"event lacks loss counters:\n{line}")
        typed_value = re.search(r"(?i)\bargs=\[\s*int32:(-?\d+)", line)
        callback = re.search(r"(?i)\bcallback=(-?\d+)", line)
        require(typed_value and callback,
                f"event lacks typed int32 argument or callback value:\n{line}")
        argument = int(typed_value.group(1))
        require(int(callback.group(1)) == argument * 100 + 17,
                f"callback disagrees with typed int32 argument:\n{line}")
    require(cursors == sorted(cursors) and len(cursors) == len(set(cursors)),
            f"event cursors are not strictly increasing:\n{text}")
    require(seen_probes & probe_ids, f"events lack expected probes {sorted(probe_ids)}:\n{text}")
    return cursors[-1]


def collect_until_probes(client: FriendlyClient, pid: int, probe_ids: set[int], after: int,
                         timeout: float, output_dir: Path, prefix: str) -> tuple[str, int]:
    deadline = time.monotonic() + timeout
    cursor = after
    batches: list[str] = []
    found: set[int] = set()
    index = 0
    while not probe_ids <= found:
        remaining = deadline - time.monotonic()
        require(remaining > 0, f"timed out waiting for probes {sorted(probe_ids)}")
        wait_seconds = max(1, min(5, int(remaining)))
        result = client.run("events", str(pid), "--count", "1", "--after", str(cursor),
                            "--timeout", str(wait_seconds), timeout=wait_seconds + 3)
        batch = save_output(output_dir / f"{prefix}-{index:02d}.txt", result)
        index += 1
        cursors = [int(value) for value in re.findall(r"(?i)\bcursor=(\d+)", batch)]
        require(cursors, f"event batch contained no readable event:\n{batch}")
        require(min(cursors) > cursor, f"event cursor did not advance after {cursor}:\n{batch}")
        cursor = max(cursors)
        found.update(int(value) for value in re.findall(r"(?i)\bprobe=(\d+)", batch))
        batches.append(batch)
    combined = "\n".join(batches)
    check_events(combined, pid, probe_ids, minimum=len(batches))
    return combined, cursor


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    parser.add_argument("--agent", required=True, type=Path)
    parser.add_argument("--protoc", required=True, type=Path)
    parser.add_argument("--proto", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    for name in ("server", "client", "target", "agent", "protoc", "proto"):
        path = getattr(args, name).resolve()
        require(path.is_file(), f"{name} file does not exist: {path}")
        setattr(args, name, path)

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / "report.txt"
    target_log_path = output_dir / "target.log"
    server_log_path = output_dir / "server.log"
    report: list[str] = []
    target = None
    server = None
    stream = None
    target_log = None
    server_log = None
    try:
        # The private socket directory is removed only after the owned server exits.
        with tempfile.TemporaryDirectory(prefix="clang-tracer-friendly-") as socket_dir:
            endpoint = f"unix://{socket_dir}/server.sock"
            target_log = target_log_path.open("wb")
            target = subprocess.Popen([str(args.target)], stdout=target_log, stderr=target_log)
            wait_for_rows(target_log_path, target.pid, 1)
            server_log = server_log_path.open("wb")
            server = subprocess.Popen(
                [str(args.server), "--agent", str(args.agent), "--listen", endpoint,
                 "--timeout", "10"], stdout=server_log, stderr=server_log)
            client = FriendlyClient(args.client, endpoint)
            env_client = FriendlyClient(args.client, None)
            endpoint_env = os.environ.copy()
            endpoint_env["CLANG_TRACER_ENDPOINT"] = endpoint

            ready = False
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(server.poll() is None, "server exited before becoming ready")
                result = env_client.run("list", timeout=2, expected_ok=None, env=endpoint_env)
                if result.returncode == 0:
                    ready = True
                    break
                time.sleep(0.1)
            require(ready, "friendly client could not reach server")

            attach_text = save_output(output_dir / "attach.txt",
                                      client.run("attach", str(target.pid)))
            require(has_field(attach_text, "pid", target.pid),
                    f"attach output lacks target PID:\n{attach_text}")
            session_match = re.search(r"(?i)\bsession(?:_id)?\s*[:=]\s*['\"]?([\w-]+)",
                                      attach_text)
            require(session_match is not None, f"attach output lacks session ID:\n{attach_text}")

            free_callback = output_dir / "free-callback.cpp"
            write_text(free_callback,
                       'extern "C" long long trace_callback(const TraceContext& context) {\n'
                       '  return context.arguments[0].signed_value * 100 + 17;\n'
                       '}\n')
            free_text = save_output(
                output_dir / "add-free.txt",
                client.run("trace", str(target.pid), "--function", "traced_work",
                           "--return", "int32", "--arg", "int32", "--callback",
                           str(free_callback)))
            free_probe_match = re.search(r"(?i)\bprobe(?:_id)?\s*[:=]\s*(\d+)", free_text)
            require(free_probe_match is not None, f"trace output lacks probe ID:\n{free_text}")
            free_probe = int(free_probe_match.group(1))
            require(has_field(free_text, "pid", target.pid),
                    f"trace output lacks target PID:\n{free_text}")

            method_callback = output_dir / "method-callback.cpp"
            write_text(method_callback,
                       'extern "C" long long trace_callback(const TraceContext& context) {\n'
                       '  return context.arguments[0].signed_value * 100 + 17;\n'
                       '}\n')
            method_text = save_output(
                output_dir / "add-method.txt",
                client.run("trace", str(target.pid), "--method", "_ZN7Fixture10overloadedEi",
                           "--return", "int32", "--arg", "int32", "--callback",
                           str(method_callback)))
            method_probe_match = re.search(r"(?i)\bprobe(?:_id)?\s*[:=]\s*(\d+)", method_text)
            require(method_probe_match is not None, f"method trace lacks probe ID:\n{method_text}")
            method_probe = int(method_probe_match.group(1))
            list_text = save_output(output_dir / "list.txt", client.run("list"))
            require(has_field(list_text, "pid", target.pid) and
                    re.search(r"(?i)session=", list_text),
                    f"list output lacks readable session status:\n{list_text}")

            probe_ids = {free_probe, method_probe}
            events_text, latest_cursor = collect_until_probes(
                client, target.pid, probe_ids, 0, 20, output_dir, "events-readable")
            save_output(output_dir / "events-readable.txt",
                        subprocess.CompletedProcess([], 0, events_text.encode(), b""))
            method_event_lines = [line for line in events_text.splitlines()
                                  if has_field(line, "probe", method_probe)]
            require(any(re.search(r"(?i)this=0x(?!0+$)[0-9a-f]+", line)
                        for line in method_event_lines),
                    f"readable method events lack a non-null object address:\n{events_text}")


            # Also retain a protobuf-mode capture and decode its wire frames.
            binary_path = output_dir / "events.pb"
            binary_run = subprocess.run(
                [str(args.client), "--endpoint", endpoint, "--format", "protobuf",
                 "--output", str(binary_path), "events", str(target.pid), "--count", "2",
                 "--timeout", "10"],
                capture_output=True, timeout=15, check=False)
            require(binary_run.returncode == 0,
                    f"protobuf events failed: {binary_run.stderr.decode(errors='replace')}")
            if not binary_path.exists():
                binary_path.write_bytes(binary_run.stdout)
            decoded = decode_file(args.protoc, args.proto, "TraceEvent", binary_path)
            require(len(decoded) == 2, f"protobuf mode produced {len(decoded)} events, expected 2")
            report.append("protobuf mode preserved decodable length-framed TraceEvent output")

            # Bounded then live streams; SIGINT must stop the client without disarming probes.
            bounded = save_output(output_dir / "events-bounded.txt",
                                  client.run("events", str(target.pid), "--count", "2",
                                             "--after", "0", "--timeout", "10", timeout=14))
            latest_cursor = check_events(bounded, target.pid, probe_ids, minimum=2)
            stream = subprocess.Popen(
                [str(args.client), "--endpoint", endpoint, "events", str(target.pid)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                try:
                    stream.communicate(timeout=1.2)
                except subprocess.TimeoutExpired:
                    require(stream.poll() is None, "unbounded event stream exited before SIGINT")
                    stream.send_signal(signal.SIGINT)
                    stream_stdout, stream_stderr = stream.communicate(timeout=5)
                else:
                    raise RuntimeError("unbounded event stream exited before SIGINT")
                require(stream.returncode == 0,
                        f"SIGINT stream returned {stream.returncode}: "
                        f"{stream_stderr.decode(errors='replace')}")
            finally:
                if stream.poll() is None:
                    stream.terminate()
                    try:
                        stream.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        stream.kill()
                        stream.wait(timeout=3)
            write_text(output_dir / "events-live.txt",
                       (stream_stdout + stream_stderr).decode(errors="replace"))
            latest_cursor = max(latest_cursor, check_events(
                stream_stdout.decode(errors="replace"), target.pid, probe_ids))
            subsequent, latest_cursor = collect_until_probes(
                client, target.pid, probe_ids, latest_cursor, 12, output_dir,
                "events-after-interrupt")
            save_output(output_dir / "events-after-interrupt.txt",
                        subprocess.CompletedProcess([], 0, subsequent.encode(), b""))
            report.append("bounded and unbounded event streams completed; SIGINT left probes active")

            # Friendly validation errors: bad type, malformed option, and missing callback file.
            for label, command in (
                ("invalid-argument", ("trace", str(target.pid), "--function", "traced_work",
                                      "--return")),
                ("missing-callback", ("trace", str(target.pid), "--function", "traced_work",
                                       "--return", "int32", "--arg", "int32", "--callback",
                                       str(output_dir / "not-present.cpp"))),
                ("invalid-type", ("trace", str(target.pid), "--function", "traced_work",
                                   "--return", "int32", "--arg", "nonsense", "--callback",
                                   str(free_callback))),
            ):
                invalid_endpoint = FriendlyClient(args.client, "unix:///no/such/clang-tracer.sock")
                result = invalid_endpoint.run(*command, expected_ok=False)
                text = save_output(output_dir / f"{label}.txt", result)
                require(text.strip() and not re.search(r"(?i)segmentation fault|terminate called|traceback|connection refused|unavailable",
                                                       text),
                        f"{label} did not produce a friendly error:\n{text}")
            report.append("invalid arguments, types, and missing callback files returned user-facing errors")

            remove_text = save_output(output_dir / "remove.txt",
                                      client.run("remove", str(target.pid), str(free_probe)))
            require(re.search(r"(?i)removed pid=", remove_text) and
                    has_field(remove_text, "probe", free_probe) and
                    has_field(remove_text, "restored", "true"),
                    f"remove output lacks readable probe status:\n{remove_text}")
            detach_text = save_output(output_dir / "detach.txt",
                                      client.run("detach", str(target.pid)))
            require(has_field(detach_text, "pid", target.pid),
                    f"detach output lacks target PID:\n{detach_text}")
            require(re.search(rf"(?:\[|,){method_probe}:restored=true(?:,|\])",
                              detach_text) is not None,
                    f"detach did not restore the active method probe:\n{detach_text}")
            previous = len(target_rows(target_log_path, target.pid))
            wait_for_rows(target_log_path, target.pid, previous + 2)
            require(target.poll() is None, "target stopped after detach")
            report.append("remove and detach restored target code; target kept returning original results")

            server.send_signal(signal.SIGTERM)
            try:
                status = server.wait(timeout=15)
            except subprocess.TimeoutExpired as error:
                server.kill()
                server.wait(timeout=3)
                raise RuntimeError("server did not shut down gracefully") from error
            require(status == 0, f"server shutdown returned {status}")
            report.append("friendly CLI integration passed")
            return 0
    finally:
        if stream is not None and stream.poll() is None:
            stream.terminate()
            try:
                stream.wait(timeout=3)
            except subprocess.TimeoutExpired:
                stream.kill()
                stream.wait(timeout=3)
        if server is not None and server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=3)
        if target is not None and target.poll() is None:
            target.send_signal(signal.SIGTERM)
            try:
                target.wait(timeout=3)
            except subprocess.TimeoutExpired:
                target.kill()
                target.wait(timeout=3)
        if target_log is not None:
            target_log.close()
        if server_log is not None:
            server_log.close()
        write_text(report_path, "\n".join(report) + ("\n" if report else ""))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"Friendly CLI integration failed: {error}", file=sys.stderr)
        sys.exit(1)

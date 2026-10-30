#!/usr/bin/env python3
"""Exercise the public clang-tracer CLI against one owned fixture target."""

from __future__ import annotations

import argparse
import re
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def textproto_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{escaped}"'


def read_frames(path: Path) -> list[bytes]:
    data = path.read_bytes()
    frames: list[bytes] = []
    offset = 0
    while offset < len(data):
        require(len(data) - offset >= 4, f"truncated frame header in {path}")
        length = int.from_bytes(data[offset : offset + 4], byteorder="little", signed=False)
        offset += 4
        require(len(data) - offset >= length, f"truncated protobuf payload in {path}")
        frames.append(data[offset : offset + length])
        offset += length
    require(bool(frames), f"no protobuf frames written to {path}")
    return frames


def decode_frame(protoc: Path, proto: Path, message: str, payload: bytes) -> str:
    completed = subprocess.run(
        [str(protoc), f"--proto_path={proto.parent}", f"--decode=clangtracer.v1.{message}", str(proto)],
        input=payload,
        capture_output=True,
        timeout=5,
        check=False,
    )
    require(completed.returncode == 0,
            f"protoc could not decode {message}: {completed.stderr.decode(errors='replace')}")
    return completed.stdout.decode("utf-8")


def decode_file(protoc: Path, proto: Path, message: str, path: Path) -> list[str]:
    decoded = [decode_frame(protoc, proto, message, frame) for frame in read_frames(path)]
    write_text(path.with_suffix(path.suffix + ".txt"), "\n--- next frame ---\n".join(decoded))
    return decoded


def run_client(client: Path, endpoint: str, request: Path, output: Path, command: str,
               *extra: str, timeout: float = 20.0) -> subprocess.CompletedProcess[bytes]:
    completed = subprocess.run(
        [str(client), "--endpoint", endpoint, "--request", str(request), "--output", str(output),
         command, *extra],
        capture_output=True,
        timeout=timeout,
        check=False,
    )
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"CLI {command} exited {completed.returncode}: {detail}")
    return completed


def target_rows(path: Path, pid: int) -> list[tuple[int, int]]:
    rows: list[tuple[int, int]] = []
    if not path.exists():
        return rows
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"^TARGET pid=(\d+) entry=\S+ arg=(-?\d+) free=(-?\d+)", line)
        if not match:
            continue
        row_pid, argument, result = (int(value) for value in match.groups())
        require(row_pid == pid, f"target log reports unexpected PID {row_pid}")
        require(result == argument * 3 + 7, f"target returned wrong original result: {line}")
        rows.append((argument, result))
    return rows


def wait_for_rows(path: Path, pid: int, count: int, timeout: float = 5.0) -> list[tuple[int, int]]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rows = target_rows(path, pid)
        if len(rows) >= count:
            return rows
        time.sleep(0.1)
    raise RuntimeError(f"target produced fewer than {count} result rows")


def check_event(decoded: str, pid: int, probe_id: int) -> None:
    event_pid = re.search(r"^pid:\s*(\d+)\s*$", decoded, re.MULTILINE)
    event_probe = re.search(r"^probe_id:\s*(\d+)\s*$", decoded, re.MULTILINE)
    argument = re.search(
        r"arguments\s*\{\s*type:\s*INT32\s+signed_value:\s*(-?\d+)", decoded, re.DOTALL
    )
    callback = re.search(r"^callback_value:\s*(-?\d+)\s*$", decoded, re.MULTILINE)
    require(event_pid and int(event_pid.group(1)) == pid, f"event has unexpected PID:\n{decoded}")
    require(event_probe and int(event_probe.group(1)) == probe_id,
            f"event has unexpected probe ID:\n{decoded}")
    require(argument is not None, f"event lacks the int32 argument:\n{decoded}")
    require(callback is not None, f"event lacks callback result:\n{decoded}")
    value = int(argument.group(1))
    require(int(callback.group(1)) == value * 100 + 17,
            f"native callback computed the wrong result:\n{decoded}")


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
    requests = output_dir / "requests"
    requests.mkdir(exist_ok=True)
    report_path = output_dir / "report.txt"
    target_log_path = output_dir / "target.log"
    server_log_path = output_dir / "server.log"
    report_lines: list[str] = []
    target = None
    server = None

    try:
        with tempfile.TemporaryDirectory(prefix="clang-tracer-client-") as socket_dir:
            endpoint = f"unix://{socket_dir}/server.sock"
            target_log = target_log_path.open("wb")
            target = subprocess.Popen([str(args.target)], stdout=target_log, stderr=target_log)
            wait_for_rows(target_log_path, target.pid, 1)

            server_log = server_log_path.open("wb")
            server = subprocess.Popen(
                [str(args.server), "--agent", str(args.agent), "--listen", endpoint,
                 "--timeout", "10"],
                stdout=server_log,
                stderr=server_log,
            )

            list_request = requests / "list.textproto"
            write_text(list_request, "")
            ready = False
            ready_deadline = time.monotonic() + 10.0
            while time.monotonic() < ready_deadline:
                require(server.poll() is None, "server exited before becoming ready")
                result = subprocess.run(
                    [str(args.client), "--endpoint", endpoint, "--request", str(list_request),
                     "--output", str(output_dir / "ready.pb"), "list"],
                    capture_output=True,
                    timeout=3,
                    check=False,
                )
                if result.returncode == 0:
                    ready = True
                    break
                time.sleep(0.1)
            require(ready, "client could not reach server during bounded readiness checks")

            attach_request = requests / "attach.textproto"
            write_text(attach_request, f"pid: {target.pid}\n")
            attach_path = output_dir / "attach.pb"
            run_client(args.client, endpoint, attach_request, attach_path, "attach")
            attach_text = decode_file(args.protoc, args.proto, "Session", attach_path)[0]
            session_match = re.search(r'^id:\s*"([^"]+)"\s*$', attach_text, re.MULTILINE)
            require(session_match is not None, f"Attach response lacks a session ID:\n{attach_text}")
            session_id = session_match.group(1)
            require(f"pid: {target.pid}\n" in attach_text, "Attach response returned the wrong PID")
            require("state: ATTACHED" in attach_text, "Attach response is not ATTACHED")

            callback = ('extern "C" long long trace_callback(const TraceContext& context) '
                        '{ return context.arguments[0].signed_value * 100 + 17; }\n')
            add_request = requests / "add.textproto"
            write_text(
                add_request,
                f"session_id: {textproto_string(session_id)}\n"
                "spec {\n"
                '  symbol: "traced_work"\n'
                "  signature { kind: FUNCTION result: INT32 arguments: INT32 }\n"
                f"  callback_source: {textproto_string(callback)}\n"
                "}\n",
            )
            add_path = output_dir / "add.pb"
            run_client(args.client, endpoint, add_request, add_path, "add")
            add_text = decode_file(args.protoc, args.proto, "Probe", add_path)[0]
            probe_match = re.search(r"^id:\s*(\d+)\s*$", add_text, re.MULTILINE)
            require(probe_match is not None, f"Add response lacks probe ID:\n{add_text}")
            probe_id = int(probe_match.group(1))
            require("active: true" in add_text, "Add response says probe is inactive")

            watch_request = requests / "watch.textproto"
            write_text(watch_request, f"session_ids: {textproto_string(session_id)}\n")
            watch_path = output_dir / "watch.pb"
            run_client(args.client, endpoint, watch_request, watch_path, "watch", "--count", "3",
                       "--timeout", "15", timeout=20)
            event_texts = decode_file(args.protoc, args.proto, "TraceEvent", watch_path)
            require(len(event_texts) == 3, f"Watch wrote {len(event_texts)} frames, expected 3")
            for event_text in event_texts:
                check_event(event_text, target.pid, probe_id)
            report_lines.append("CLI Watch wrote and decoded three valid event frames")

            remove_request = requests / "remove.textproto"
            write_text(remove_request,
                       f"session_id: {textproto_string(session_id)}\nprobe_id: {probe_id}\n")
            remove_path = output_dir / "remove.pb"
            run_client(args.client, endpoint, remove_request, remove_path, "remove")
            remove_text = decode_file(args.protoc, args.proto, "Probe", remove_path)[0]
            # Protobuf text output omits default-valued bools, so false may be absent.
            require("active: true" not in remove_text, "Remove response says probe is still active")
            require("original_bytes_restored: true" in remove_text,
                    "Remove response did not confirm original bytes were restored")

            detach_request = requests / "detach.textproto"
            write_text(detach_request, f"session_id: {textproto_string(session_id)}\n")
            detach_path = output_dir / "detach.pb"
            run_client(args.client, endpoint, detach_request, detach_path, "detach")
            detach_text = decode_file(args.protoc, args.proto, "Session", detach_path)[0]
            require("state: DETACHED" in detach_text, "Detach response is not DETACHED")
            previous_rows = len(target_rows(target_log_path, target.pid))
            wait_for_rows(target_log_path, target.pid, previous_rows + 2)
            require(target.poll() is None, "target exited after detach")
            report_lines.append("CLI Remove restored code and target returned original results after detach")

            list_path = output_dir / "list.pb"
            run_client(args.client, endpoint, list_request, list_path, "list")
            list_text = decode_file(args.protoc, args.proto, "ListSessionsResponse", list_path)[0]
            require(session_id in list_text and "DETACHED" in list_text,
                    "ListSessions did not report the detached session")

            server.send_signal(signal.SIGTERM)
            try:
                server_status = server.wait(timeout=15)
            except subprocess.TimeoutExpired as error:
                server.kill()
                server.wait(timeout=3)
                raise RuntimeError("server did not shut down gracefully") from error
            require(server_status == 0, f"server shutdown returned {server_status}")
            rows_after_shutdown = len(target_rows(target_log_path, target.pid))
            wait_for_rows(target_log_path, target.pid, rows_after_shutdown + 2)
            require(target.poll() is None, "target exited after server shutdown")
            report_lines.append("server exited cleanly and target continued after shutdown")
            report_lines.append("CLI integration passed")
            return 0
    finally:
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
        write_text(report_path, "\n".join(report_lines) + ("\n" if report_lines else ""))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"CLI integration failed: {error}", file=sys.stderr)
        sys.exit(1)

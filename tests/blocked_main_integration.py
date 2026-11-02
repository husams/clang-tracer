#!/usr/bin/env python3
"""Attach to targets whose main thread is blocked in a non-sleep syscall.

A: the attach succeeds through a sleeping worker for every main-thread
   blocking primitive, the target is left untouched after detach.
B: with no sleeping thread, attach fails with FAILED_PRECONDITION and a
   message listing thread IDs and syscall numbers.
C: a server whose only session failed to attach shuts down cleanly.
"""

from __future__ import annotations

import argparse
import re
import signal
import subprocess
import tempfile
import time
from pathlib import Path

MAIN_WAITS = ("futex", "epoll", "poll", "read", "accept", "pause", "sigwait")
FUTEX_SYSCALL = 202

CALLBACK = ('extern "C" long long trace_callback(const TraceContext &context) '
            '{ return context.arguments[0].signed_value * 100 + 17; }\n')


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def run(client: Path, endpoint: str, *args: str, ok: bool = True,
        timeout: float = 30) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run([str(client), "--endpoint", endpoint, *args],
                            capture_output=True, timeout=timeout, check=False)
    output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    require((result.returncode == 0) == ok,
            f"CLI {' '.join(args)} returned {result.returncode}: {output}")
    return result


def text(result: subprocess.CompletedProcess[bytes]) -> str:
    return (result.stdout + result.stderr).decode("utf-8", errors="replace")


def stop(process: subprocess.Popen[bytes] | None) -> None:
    if process is not None and process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)


def start_server(args: argparse.Namespace, endpoint: str, timeout: int,
                 log: Path) -> subprocess.Popen[bytes]:
    handle = log.open("wb")
    server = subprocess.Popen([str(args.server), "--agent", str(args.agent),
                               "--listen", endpoint, "--timeout", str(timeout)],
                              stdout=handle, stderr=handle)
    handle.close()  # The child keeps its own descriptor.
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        require(server.poll() is None, f"server exited during startup: {log}")
        probe = subprocess.run([str(args.client), "--endpoint", endpoint, "list"],
                               capture_output=True, timeout=5, check=False)
        if probe.returncode == 0:
            return server
        time.sleep(.1)
    stop(server)
    raise RuntimeError(f"server did not become ready: {log}")


def start_target(args: argparse.Namespace, log: Path,
                 *options: str) -> tuple[subprocess.Popen[bytes], int]:
    handle = log.open("wb")
    target = subprocess.Popen([str(args.target), *options], stdout=handle,
                              stderr=handle)
    handle.close()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        match = re.search(r"TARGET pid=(\d+)", log.read_text(errors="replace"))
        if match:
            require(int(match.group(1)) == target.pid, "target reported another PID")
            return target, target.pid
        require(target.poll() is None, f"target exited during startup: {log}")
        time.sleep(.05)
    stop(target)
    raise RuntimeError(f"target did not start: {log}")


def read_syscall(pid: int, tid: int | None = None) -> int | None:
    path = Path(f"/proc/{pid}/syscall" if tid is None else f"/proc/{pid}/task/{tid}/syscall")
    try:
        fields = path.read_text().split()
    except OSError:
        return None
    if not fields or not fields[0].lstrip("-").isdigit():
        return None
    return int(fields[0])


def settled_syscall(pid: int, timeout: float = 5) -> int:
    """Return the syscall number once the main thread is stably blocked."""
    deadline = time.monotonic() + timeout
    previous = None
    while time.monotonic() < deadline:
        current = read_syscall(pid)
        if current is not None and current == previous:
            return current
        previous = current
        time.sleep(.2)
    raise RuntimeError(f"main thread of {pid} never settled in a syscall")


def tasks(pid: int) -> list[int]:
    return sorted(int(entry.name) for entry in Path(f"/proc/{pid}/task").iterdir()
                  if entry.name.isdigit())


def task_status(pid: int, tid: int) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in Path(f"/proc/{pid}/task/{tid}/status").read_text().splitlines():
        key, _, value = line.partition(":\t")
        fields[key] = value
    return fields


def require_untraced(pid: int) -> None:
    status = Path(f"/proc/{pid}/status").read_text()
    require("TracerPid:\t0" in status, f"leader still traced:\n{status}")
    for tid in tasks(pid):
        require(task_status(pid, tid).get("TracerPid") == "0",
                f"task {tid} of {pid} is still traced")


def require_workers_running(pid: int, count: int = 4) -> None:
    workers = [tid for tid in tasks(pid) if tid != pid]
    require(len(workers) >= count, f"expected {count} workers, found {workers}")
    for tid in workers:
        state = task_status(pid, tid).get("State", "")
        require(state[:1] in ("R", "S", "D"), f"worker {tid} is not running: {state}")


def scenario_attach(args: argparse.Namespace, output_dir: Path, callback: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="clang-tracer-blocked-main-") as sockets:
        endpoint = f"unix://{sockets}/server.sock"
        server = start_server(args, endpoint, 10, output_dir / "server-attach.log")
        try:
            for wait in MAIN_WAITS:
                target, pid = start_target(args, output_dir / f"target-{wait}.log", wait)
                try:
                    blocked = settled_syscall(pid)
                    attach = text(run(args.client, endpoint, "attach", str(pid)))
                    require(re.search(r"state=ATTACHED", attach, re.I) is not None,
                            f"{wait}: attach did not report ATTACHED: {attach}")
                    if wait == "futex":
                        require(blocked == FUTEX_SYSCALL,
                                f"futex main blocked in syscall {blocked}")
                        traced = text(run(args.client, endpoint, "trace", str(pid),
                                          "add(int, int)", "--callback", str(callback),
                                          timeout=45))
                        match = re.search(r"\bprobe\s*=\s*(\d+)", traced)
                        require(match is not None, f"trace output lacks probe ID: {traced}")
                        probe_id = int(match.group(1))
                        events = text(run(args.client, endpoint, "events", str(pid),
                                          "--count", "1", "--timeout", "10", timeout=15))
                        require(re.search(rf"\bprobe={probe_id}\b", events) is not None,
                                f"no events arrived for probe {probe_id}: {events}")
                        run(args.client, endpoint, "remove", str(pid), str(probe_id))
                    run(args.client, endpoint, "detach", str(pid), timeout=20)

                    require(target.poll() is None, f"{wait}: target died")
                    require_untraced(pid)
                    deadline = time.monotonic() + 5
                    after = None
                    while time.monotonic() < deadline:
                        after = read_syscall(pid)
                        if after == blocked:
                            break
                        time.sleep(.1)
                    require(after == blocked,
                            f"{wait}: main left syscall {blocked}, now {after}")
                    require_workers_running(pid)
                finally:
                    stop(target)
        finally:
            stop(server)


def scenario_no_host(args: argparse.Namespace, output_dir: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="clang-tracer-blocked-main-") as sockets:
        endpoint = f"unix://{sockets}/server.sock"
        log = output_dir / "server-no-host.log"
        server = start_server(args, endpoint, 1, log)
        target = None
        try:
            target, pid = start_target(args, output_dir / "target-no-host.log",
                                       "futex", "--blocked-workers")
            settled_syscall(pid)
            failure = text(run(args.client, endpoint, "attach", str(pid), ok=False,
                               timeout=30))
            require("gRPC code 9" in failure, f"attach was not FAILED_PRECONDITION: {failure}")
            require(f"syscall {FUTEX_SYSCALL}" in failure,
                    f"attach error lacks the blocking syscall: {failure}")
            require("busy-main" not in failure, f"stale busy-main message: {failure}")
            require(target.poll() is None, "target died after failed attach")
            require_untraced(pid)

            # C: the failed attach left no agent state, so shutdown is clean.
            server.send_signal(signal.SIGTERM)
            try:
                status = server.wait(timeout=20)
            except subprocess.TimeoutExpired:
                raise RuntimeError(f"server did not stop after SIGTERM: {log}")
            require(status == 0, f"server exited with status {status}: {log}")
            server_log = log.read_text(encoding="utf-8", errors="replace")
            require("cleanup incomplete" not in server_log,
                    f"server reported incomplete cleanup:\n{server_log}")
            require(target.poll() is None, "target died after server shutdown")
            require_untraced(pid)
        finally:
            stop(target)
            stop(server)


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("server", "client", "agent", "target", "output-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    for name in ("server", "client", "agent", "target"):
        path = getattr(args, name).resolve()
        require(path.exists(), f"missing {name}: {path}")
        setattr(args, name, path)
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    callback = output_dir / "callback.cpp"
    callback.write_text(CALLBACK, encoding="utf-8")

    scenario_attach(args, output_dir, callback)
    scenario_no_host(args, output_dir)
    print(f"blocked-main attach through sleeping workers ({', '.join(MAIN_WAITS)}), "
          "FAILED_PRECONDITION without a host, and clean shutdown passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

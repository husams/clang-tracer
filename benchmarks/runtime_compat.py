#!/usr/bin/env python3
"""Paired public-client runtime benchmark for compatible tracer builds."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import platform
import selectors
import socket
import subprocess
import sys
import time
from pathlib import Path
from statistics import median
from typing import Any

from clang_tracer import Client, messages


WARMUP_CALLS = 100
OFF_CALLS = 5_000_000
STACK_DEPTH = 24
CALLBACK_SOURCE = """
class Interceptor {
public:
  void pre(const Call &) const {}
  void post(const Call &) const {}
};
"""
SHARED_CALLBACK_SOURCE = """
struct Scope {};
struct Counters : shared_state<Scope> { int calls = 0; };
struct Interceptor {
  Interceptor(State<Counters>);
  void pre(const Call &, Counters &state) { ++state.calls; }
  void post(const Call &, Counters &state) { ++state.calls; }
};
"""
SELECTOR = "stack_add(int, int)"
EVENT_TIMEOUT = 10


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-server", required=True, type=Path)
    parser.add_argument("--baseline-agent", required=True, type=Path)
    parser.add_argument("--candidate-server", type=Path)
    parser.add_argument("--candidate-agent", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    parser.add_argument("--target-library", type=Path)
    parser.add_argument("--candidate-target", type=Path, help="Same workload under candidate runtime; defaults to --target")
    parser.add_argument("--candidate-target-library", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--modes", nargs="+", choices=("off", "trace", "stack", "shared"), default=["off", "trace", "stack"])
    parser.add_argument("--rounds", type=positive_int, default=9)
    parser.add_argument("--target-cpu", type=nonnegative_int, default=2)
    parser.add_argument("--server-cpu", type=nonnegative_int, default=3)
    parser.add_argument("--traced-calls", type=positive_int, default=1500)
    args = parser.parse_args()
    if 2 * (WARMUP_CALLS + args.traced_calls) >= 4096:
        parser.error("--traced-calls must keep the event count below the 4096-event history limit")
    args.candidate_server = args.candidate_server or args.baseline_server
    return args


def positive_int(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be at least 1")
    return number


def nonnegative_int(value: str) -> int:
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be nonnegative")
    return number


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_inputs(args: argparse.Namespace) -> dict[str, Path]:
    paths = {
        "target": args.target,
        "candidate_target": args.candidate_target or args.target,
        "baseline_server": args.baseline_server,
        "baseline_agent": args.baseline_agent,
        "candidate_server": args.candidate_server,
        "candidate_agent": args.candidate_agent,
    }
    if args.target_library:
        paths["target_library"] = args.target_library
    if args.candidate_target_library:
        paths["candidate_target_library"] = args.candidate_target_library
    resolved = {name: path.resolve(strict=True) for name, path in paths.items()}
    for name in ("target", "candidate_target", "baseline_server", "candidate_server"):
        if not os.access(resolved[name], os.X_OK):
            raise ValueError(f"{name} is not executable: {resolved[name]}")
    for name in ("baseline_agent", "candidate_agent", "target_library"):
        if name in resolved and not resolved[name].is_file():
            raise ValueError(f"{name} is not a file: {resolved[name]}")
    return resolved


def capture_hashes(paths: dict[str, Path]) -> dict[str, str]:
    return {name: sha256(path) for name, path in sorted(paths.items())}


def make_endpoint() -> str:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return f"127.0.0.1:{listener.getsockname()[1]}"


class TargetProcess:
    def __init__(self, executable: Path, cpu: int, stderr_path: Path) -> None:
        self.stderr_stream = stderr_path.open("wb")
        self.process = subprocess.Popen(
            ["taskset", "-c", str(cpu), str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self.stderr_stream,
            bufsize=0,
        )
        assert self.process.stdout is not None and self.process.stdin is not None
        os.set_blocking(self.process.stdout.fileno(), False)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)
        self.buffer = bytearray()

    @property
    def pid(self) -> int:
        return self.process.pid

    def read_line(self, timeout: float) -> str:
        deadline = time.monotonic() + timeout
        while True:
            newline = self.buffer.find(b"\n")
            if newline >= 0:
                line = bytes(self.buffer[:newline]).decode("utf-8", errors="replace")
                del self.buffer[: newline + 1]
                return line
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise TimeoutError(f"timed out waiting for target output; pid={self.pid}")
            chunk = os.read(self.process.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError(
                    f"target exited before writing a complete line; pid={self.pid}, "
                    f"returncode={self.process.poll()}"
                )
            self.buffer.extend(chunk)

    def run_count(self, count: int) -> dict[str, int]:
        assert self.process.stdin is not None
        self.process.stdin.write(f"{count}\n".encode())
        self.process.stdin.flush()
        fields = self.read_line(60).split(",")
        if len(fields) != 6 or fields[0] != "RESULT":
            raise RuntimeError(f"malformed target result: {fields!r}")
        names = ("calls", "wall_ns", "cpu_ns", "checksum", "valid")
        result = dict(zip(names, map(int, fields[1:])))
        if result["calls"] != count or result["valid"] != 1:
            raise RuntimeError(f"target reported an invalid result: {result}")
        remainder = count % 32
        input_sum = (count // 32) * 528 + remainder * (remainder + 1) // 2
        expected = 3 * input_sum + 7 * count + count * (count + 1) // 2
        if result["checksum"] != expected:
            raise RuntimeError(f"target checksum mismatch: {result}")
        return result

    def stop(self) -> None:
        if self.process.poll() is None:
            assert self.process.stdin is not None
            self.process.stdin.write(b"0\n")
            self.process.stdin.flush()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired as error:
                    raise RuntimeError(
                        f"owned target did not stop; pid={self.pid}"
                    ) from error
                raise RuntimeError(
                    f"owned target ignored the 0 command and required SIGTERM; pid={self.pid}"
                )
        if self.process.returncode != 0:
            raise RuntimeError(
                f"target exited with status {self.process.returncode}; pid={self.pid}"
            )
        self.selector.close()
        self.stderr_stream.close()


def read_maps(pid: int, path: Path) -> None:
    maps_path = Path(f"/proc/{pid}/maps")
    path.write_text(maps_path.read_text())


def numeric_value(value: Any) -> int:
    if value.type != messages.INT32:
        raise RuntimeError(f"expected INT32 value, found {value.type}")
    return int(value.signed_value)


def event_record(event: Any) -> dict[str, Any]:
    return {
        "sequence": int(event.sequence),
        "probe_sequence": int(event.probe_sequence),
        "probe_id": int(event.probe_id),
        "call_id": int(event.call_id),
        "phase": messages.TracePhase.Name(event.phase),
        "arguments": [numeric_value(value) for value in event.arguments],
        "result": numeric_value(event.result) if event.HasField("result") else None,
        "agent_dropped_events": int(event.agent_dropped_events),
        "server_dropped_events": int(event.server_dropped_events),
        "stack": {
            "status": messages.StackTrace.Status.Name(event.stack.status),
            "application_only": bool(event.stack.application_only),
            "frames": [
                {
                    "pc": int(frame.pc),
                    "kind": messages.StackFrame.Kind.Name(frame.kind),
                    "module": frame.module,
                    "function": frame.function,
                    "application_frame": bool(frame.application_frame),
                }
                for frame in event.stack.frames
            ],
        },
    }


def validate_events(
    events: list[Any], expected: int, probe_id: int, mode: str
) -> tuple[int, int]:
    if len(events) != expected:
        raise RuntimeError(f"expected {expected} trace events, received {len(events)}")
    seen_calls: dict[int, dict[str, Any]] = {}
    agent_drops = 0
    server_drops = 0
    for index, event in enumerate(events, start=1):
        if event.probe_id != probe_id:
            raise RuntimeError(f"unexpected probe id in event {event.sequence}")
        if event.probe_sequence != index:
            raise RuntimeError(
                f"probe sequence gap: expected {index}, got {event.probe_sequence}"
            )
        agent_drops = max(agent_drops, int(event.agent_dropped_events))
        server_drops = max(server_drops, int(event.server_dropped_events))
        if agent_drops or server_drops:
            raise RuntimeError(
                f"trace drops reported: agent={agent_drops}, server={server_drops}"
            )
        if mode == "stack":
            if event.stack.status != messages.StackTrace.COMPLETE:
                raise RuntimeError(f"incomplete stack trace for call {event.call_id}")
            if not event.stack.application_only or not event.stack.frames:
                raise RuntimeError(
                    f"stack trace lacks application-only frames for call {event.call_id}"
                )
        elif event.stack.status != messages.StackTrace.DISABLED:
            raise RuntimeError(f"unexpected stack capture in {mode} mode")
        if len(event.arguments) != 2:
            raise RuntimeError(f"expected two call arguments for {event.call_id}")
        arguments = [numeric_value(value) for value in event.arguments]
        call = seen_calls.setdefault(event.call_id, {})
        phase = messages.TracePhase.Name(event.phase)
        if phase in call:
            raise RuntimeError(f"duplicate {phase} event for call {event.call_id}")
        call[phase] = (arguments, event)

    if len(seen_calls) != expected // 2:
        raise RuntimeError(f"expected {expected // 2} call pairs, found {len(seen_calls)}")
    for call_id, phases in seen_calls.items():
        if set(phases) != {"PRE", "POST"}:
            raise RuntimeError(f"unpaired PRE/POST event for call {call_id}")
        pre_args, _ = phases["PRE"]
        post_args, post_event = phases["POST"]
        if pre_args != post_args:
            raise RuntimeError(f"PRE/POST arguments differ for call {call_id}")
        if post_event.result is None or numeric_value(post_event.result) != sum(pre_args):
            raise RuntimeError(f"POST result mismatch for call {call_id}")
    return agent_drops, server_drops


def affinity_start_server(client: Client, executable: Path, agent: Path, cpu: int) -> float:
    original = os.sched_getaffinity(0)
    if cpu not in original:
        raise ValueError(f"server CPU {cpu} is outside the current affinity {sorted(original)}")
    started = time.perf_counter()
    try:
        os.sched_setaffinity(0, {cpu})
        client.start_server(
            str(executable), agent=str(agent), timeout=30, operation_timeout=30
        )
    finally:
        os.sched_setaffinity(0, original)
    return time.perf_counter() - started


def run_trial(
    *,
    output: Path,
    round_number: int,
    variant: str,
    mode: str,
    paths: dict[str, Path],
    args: argparse.Namespace,
) -> dict[str, Any]:
    trial_dir = output / f"round-{round_number:02d}-{variant}-{mode}"
    trial_dir.mkdir()
    record: dict[str, Any] = {
        "round": round_number,
        "variant": variant,
        "mode": mode,
        "status": "running",
        "target_pid": None,
        "endpoint": None,
        "startup_seconds": 0.0,
        "attach_seconds": 0.0,
        "inject_seconds": 0.0,
    }
    target: TargetProcess | None = None
    client: Client | None = None
    session_id: str | None = None
    cleanup_errors: list[str] = []
    try:
        target = TargetProcess(
            (paths["candidate_target"] if variant == "candidate" else paths["target"]), args.target_cpu, trial_dir / "target-stderr.log"
        )
        record["target_pid"] = target.pid
        ready = target.read_line(10)
        if ready != f"READY {target.pid}":
            raise RuntimeError(f"unexpected target readiness line: {ready!r}")

        probe = None
        removed = None
        events: list[Any] = []
        if mode != "off":
            endpoint = make_endpoint()
            record["endpoint"] = endpoint
            server_name = f"{variant}_server"
            agent_name = f"{variant}_agent"
            client = Client(endpoint, timeout=30)
            record["startup_seconds"] = affinity_start_server(
                client, paths[server_name], paths[agent_name], args.server_cpu
            )
            read_maps(target.pid, trial_dir / "maps.before-attach.txt")
            started = time.perf_counter()
            session = client.attach(target.pid)
            record["attach_seconds"] = time.perf_counter() - started
            session_id = session.id
            read_maps(target.pid, trial_dir / "maps.after-attach.txt")
            started = time.perf_counter()
            probe = client.inject(
                session.id,
                SELECTOR,
                SHARED_CALLBACK_SOURCE if mode == "shared" else CALLBACK_SOURCE,
                stack_depth=STACK_DEPTH if mode == "stack" else 0,
            )
            record["inject_seconds"] = time.perf_counter() - started
            record["probe_id"] = int(probe.id)

        warmup = target.run_count(WARMUP_CALLS)
        count = OFF_CALLS if mode == "off" else args.traced_calls
        measured = target.run_count(count)
        record["measurement"] = measured
        record["ns_per_call"] = measured["wall_ns"] / count

        if mode != "off":
            # Shared-state hooks publish only explicit context snapshots. This
            # mode measures guarded mutation without requesting snapshots.
            expected_events = 0 if mode == "shared" else 2 * (WARMUP_CALLS + count)
            if expected_events:
                events = client.collect_traces(
                    expected_events,
                    session_ids=[session_id],
                    timeout=EVENT_TIMEOUT,
                )
            with (trial_dir / "events.json").open("w") as stream:
                json.dump([event_record(event) for event in events], stream)
                stream.write("\n")
            agent_drops, server_drops = validate_events(
                events, expected_events, int(probe.id), mode
            )
            removed = client.remove_probe(session_id, probe.id)
            record["events"] = len(events)
            record["agent_drops"] = max(agent_drops, int(removed.agent_dropped_events))
            record["server_drops"] = server_drops
            record["restored"] = bool(removed.original_bytes_restored)
            record["callback_errors"] = int(removed.callback_errors)
            if not removed.original_bytes_restored:
                raise RuntimeError("remove_probe did not report original bytes restored")
            if record["agent_drops"] or server_drops:
                raise RuntimeError(
                    f"probe reported drops: agent={record['agent_drops']}, server={server_drops}"
                )
            if removed.callback_errors:
                raise RuntimeError(f"probe callback errors: {removed.callback_errors}")
        else:
            record.update(events=0, agent_drops=0, server_drops=0, restored=None)
        record["warmup"] = warmup
        record["status"] = "ok"
    except BaseException as error:
        record["status"] = "error"
        record["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if client is not None:
            if session_id:
                try:
                    detached = client.detach(session_id)
                    record["detached"] = True
                    record["detach_state"] = messages.Session.State.Name(detached.state)
                    if detached.state != messages.Session.DETACHED:
                        raise RuntimeError("detach did not return DETACHED state")
                    for final_probe in detached.probes:
                        if final_probe.active or not final_probe.original_bytes_restored:
                            raise RuntimeError("detach left an active or unrestored probe")
                except BaseException as error:
                    cleanup_errors.append(f"detach: {type(error).__name__}: {error}")
            if client.server is not None:
                try:
                    (trial_dir / "server-stderr.log").write_text(
                        client.server.diagnostics()
                    )
                except BaseException as error:
                    cleanup_errors.append(f"server diagnostics: {error}")
            try:
                client.close()
                record["server_stopped"] = True
            except BaseException as error:
                cleanup_errors.append(f"server shutdown: {type(error).__name__}: {error}")
                record["server_stopped"] = False
                if client.server is not None:
                    record["server_pid"] = client.server.pid
                    (trial_dir / "server-stderr.log").write_text(
                        client.server.diagnostics()
                    )
        if target is not None:
            try:
                target.stop()
                record["target_stopped"] = True
            except BaseException as error:
                cleanup_errors.append(f"target shutdown: {type(error).__name__}: {error}")
                record["target_stopped"] = False
        if cleanup_errors:
            record["cleanup_errors"] = cleanup_errors
            record["status"] = "error"
        (trial_dir / "trial.json").write_text(
            json.dumps(record, indent=2, sort_keys=True) + "\n"
        )
    if cleanup_errors:
        raise RuntimeError("trial cleanup failed: " + "; ".join(cleanup_errors))
    return record


def summarize(trials: list[dict[str, Any]]) -> dict[str, Any]:
    groups: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for trial in trials:
        groups.setdefault((trial["variant"], trial["mode"]), []).append(trial)
    summary: dict[str, Any] = {"groups": {}, "paired_relative_percent": {}}
    for (variant, mode), samples in sorted(groups.items()):
        values = [float(sample["ns_per_call"]) for sample in samples]
        summary["groups"][f"{variant}/{mode}"] = {
            "samples": len(values),
            "median_ns_per_call": median(values),
            "min_ns_per_call": min(values),
            "max_ns_per_call": max(values),
        }
    by_pair = {(trial["round"], trial["variant"], trial["mode"]): trial for trial in trials}
    for mode in sorted({trial["mode"] for trial in trials}):
        differences = []
        for round_number in sorted({trial["round"] for trial in trials}):
            baseline = by_pair.get((round_number, "baseline", mode))
            candidate = by_pair.get((round_number, "candidate", mode))
            if baseline is None or candidate is None:
                continue
            base_value = float(baseline["ns_per_call"])
            candidate_value = float(candidate["ns_per_call"])
            differences.append(
                {
                    "round": round_number,
                    "candidate_vs_baseline_percent": (candidate_value / base_value - 1) * 100,
                }
            )
        summary["paired_relative_percent"][mode] = differences
    return summary


def run(args: argparse.Namespace) -> int:
    if not args.output.is_absolute():
        raise ValueError("--output must be an absolute path")
    if args.output.exists():
        raise FileExistsError(f"--output must be a fresh directory: {args.output}")
    paths = resolve_inputs(args)
    if not Path("/proc/self/maps").exists():
        raise RuntimeError("this benchmark requires Linux /proc/<pid>/maps")
    if not shutil_which("taskset"):
        raise RuntimeError("taskset is required to pin target processes")
    available_cpus = os.sched_getaffinity(0)
    for label, cpu in (("target", args.target_cpu), ("server", args.server_cpu)):
        if cpu not in available_cpus:
            raise ValueError(f"{label} CPU {cpu} unavailable; allowed: {sorted(available_cpus)}")

    args.output.mkdir(parents=True)
    initial_hashes = capture_hashes(paths)
    (args.output / "hashes.initial.json").write_text(
        json.dumps(initial_hashes, indent=2, sort_keys=True) + "\n"
    )
    environment = {
        "python": sys.version,
        "platform": platform.platform(),
        "machine": platform.machine(),
        "target_cpu": args.target_cpu,
        "server_cpu": args.server_cpu,
        "rounds": args.rounds,
        "traced_calls": args.traced_calls,
        "warmup_calls": WARMUP_CALLS,
        "off_calls": OFF_CALLS,
        "stack_depth": STACK_DEPTH,
        "modes": list(args.modes),
        "inputs": {name: str(path) for name, path in paths.items()},
        "sha256": initial_hashes,
    }
    (args.output / "environment.json").write_text(
        json.dumps(environment, indent=2, sort_keys=True) + "\n"
    )
    trials: list[dict[str, Any]] = []
    modes = tuple(args.modes)
    variants = ("baseline", "candidate")
    try:
        for round_number in range(1, args.rounds + 1):
            ordered_variants = variants if round_number % 2 else tuple(reversed(variants))
            rotate = (round_number - 1) % len(modes)
            ordered_modes = modes[rotate:] + modes[:rotate]
            for variant in ordered_variants:
                for mode in ordered_modes:
                    trial = run_trial(
                        output=args.output,
                        round_number=round_number,
                        variant=variant,
                        mode=mode,
                        paths=paths,
                        args=args,
                    )
                    trials.append(trial)
                    append_csv(args.output / "trials.csv", trial)
        summary = summarize(trials)
        (args.output / "summary.json").write_text(
            json.dumps(summary, indent=2, sort_keys=True) + "\n"
        )
    except BaseException as error:
        (args.output / "driver-error.json").write_text(
            json.dumps(
                {"error": f"{type(error).__name__}: {error}", "completed_trials": len(trials)},
                indent=2,
            )
            + "\n"
        )
        raise
    finally:
        final_hashes = capture_hashes(paths)
        (args.output / "hashes.final.json").write_text(
            json.dumps(final_hashes, indent=2, sort_keys=True) + "\n"
        )
        if final_hashes != initial_hashes:
            (args.output / "immutable-input-error.json").write_text(
                json.dumps(
                    {"initial": initial_hashes, "final": final_hashes},
                    indent=2,
                    sort_keys=True,
                )
                + "\n"
            )
            raise RuntimeError("one or more benchmark input binaries changed during the run")
    print(f"Benchmark evidence saved to {args.output}")
    return 0


def append_csv(path: Path, trial: dict[str, Any]) -> None:
    existed = path.exists()
    with path.open("a", newline="") as stream:
        writer = csv.DictWriter(
            stream,
            fieldnames=(
                "round",
                "variant",
                "mode",
                "calls",
                "wall_ns",
                "cpu_ns",
                "ns_per_call",
                "startup_seconds",
                "attach_seconds",
                "inject_seconds",
                "events",
                "agent_drops",
                "server_drops",
                "restored",
            ),
        )
        if not existed:
            writer.writeheader()
        measurement = trial.get("measurement", {})
        writer.writerow(
            {
                "round": trial["round"],
                "variant": trial["variant"],
                "mode": trial["mode"],
                "calls": measurement.get("calls", ""),
                "wall_ns": measurement.get("wall_ns", ""),
                "cpu_ns": measurement.get("cpu_ns", ""),
                "ns_per_call": trial.get("ns_per_call", ""),
                "startup_seconds": trial.get("startup_seconds", ""),
                "attach_seconds": trial.get("attach_seconds", ""),
                "inject_seconds": trial.get("inject_seconds", ""),
                "events": trial.get("events", ""),
                "agent_drops": trial.get("agent_drops", ""),
                "server_drops": trial.get("server_drops", ""),
                "restored": trial.get("restored", ""),
            }
        )


def shutil_which(command: str) -> str | None:
    """Resolve a small host utility without importing shell helpers."""
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(directory) / command
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return None


if __name__ == "__main__":
    try:
        raise SystemExit(run(parse_args()))
    except Exception as error:
        print(f"runtime compatibility benchmark failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error

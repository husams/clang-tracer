"""Compare the owned Linux fixture against a symbol-based instruction oracle."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess


def symbols(pilot):
    listing = subprocess.check_output(["nm", "-n", "--defined-only", pilot], text=True)
    result = {}
    for line in listing.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2].startswith("pilot_"):
            result[parts[2]] = int(parts[0], 16)
    return result


def expected_pcs(rounds, signal):
    path = ["save", "clear", "rounds", "test_signal", "skip_signal"]
    if signal:
        path += ["number", "pid", "tid", "sigusr1", "send_signal"]
    path.append("target")
    for remaining in range(rounds, 0, -1):
        path += ["parity", "branch"]
        if remaining % 2:
            path += ["indirect", "add", "return", "skip_direct"]
        else:
            path += ["direct", "add", "return"]
        path += ["decrement", "repeat"]
    return path + ["value", "restore", "finish"]


def run(command, gum, names, rounds, limit, signal):
    result = subprocess.run([*command, gum, str(rounds), str(limit), str(signal)],
                            text=True, capture_output=True, timeout=24)
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    if result.returncode:
        completed = [row["mode"] for row in rows]
        raise RuntimeError(f"fixture failed ({result.returncode}) after {completed}\n{result.stderr}")
    modes = ["baseline", "native", "post"] if gum == "-" else ["baseline", "native", "gum", "post"]
    if [row["mode"] for row in rows] != modes:
        raise RuntimeError(f"unexpected fixture phases: {rows}")

    base = names["pilot_work"]
    path = expected_pcs(rounds, signal)
    expected = [names[f"pilot_pc_{label}"] - base for label in path]
    identities = {(row["pid"], row["tid"], row["begin"], row["end"]) for row in rows}
    if len(identities) != 1:
        raise RuntimeError(f"backend identity changed: {identities}")
    pid, tid, begin, end = identities.pop()
    if pid != tid or end - begin != names["pilot_work_end"] - base:
        raise RuntimeError("fixture thread or address range differs from the oracle")
    for row in rows:
        mode = row["mode"]
        tracing = mode in {"native", "gum"}
        if row["value"] != 3 * rounds or row["signals"] != signal:
            raise RuntimeError(f"{mode} changed the workload result or signal delivery: {row}")
        offsets = row.pop("offsets")
        wanted = expected[:limit] if tracing else []
        if offsets != wanted:
            index = next((i for i, (a, b) in enumerate(zip(offsets, wanted)) if a != b),
                         min(len(offsets), len(wanted)))
            raise RuntimeError(f"{mode} differs at instruction {index}: "
                               f"got {offsets[index:index + 4]}, wanted {wanted[index:index + 4]}")
        if row["truncated"] != int(tracing and len(expected) > limit):
            raise RuntimeError(f"{mode} reported the wrong limit outcome")
        row["count"] = len(offsets)
        row["sequence_sha256"] = hashlib.sha256(json.dumps(offsets).encode()).hexdigest()
        if rounds <= 3:
            row["offsets"] = offsets
    return {"rounds": rounds, "limit": limit, "signal": signal,
            "expected_count": len(expected), "phases": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--native-only", action="store_true")
    parser.add_argument("--trials", type=int, default=9)
    parser.add_argument("--cpu", type=int)
    parser.add_argument("--loader", type=Path)
    parser.add_argument("--library-path")
    args = parser.parse_args()
    if not 1 <= args.trials <= 30:
        parser.error("--trials must be between 1 and 30")
    if bool(args.loader) != bool(args.library_path):
        parser.error("--loader and --library-path must be supplied together")
    if args.cpu is not None:
        os.sched_setaffinity(0, {args.cpu})
    pilot = args.build.resolve() / "pilot"
    gum = "-" if args.native_only else args.build.resolve() / "gum-backend.so"
    command = [pilot]
    if args.loader:
        command = [args.loader, "--library-path", args.library_path, pilot]
    names = symbols(pilot)
    smoke = [run(command, gum, names, 3, cap, signal)
             for cap, signal in [(256, 0), (256, 1), (7, 1), (32, 0)]]
    trials = [run(command, gum, names, 256, 8192, 0) for _ in range(args.trials)]
    medians = {
        row["mode"]: {
            metric: statistics.median(trial["phases"][index][metric] for trial in trials)
            for metric in ("work_ns", "wall_ns")
        }
        for index, row in enumerate(trials[0]["phases"])
    }
    print(json.dumps({"cpu_affinity": sorted(os.sched_getaffinity(0)), "smoke": smoke,
                      "trials": trials, "median_ns": medians}, indent=2))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Separate first-in-process and subsequent injection latency samples."""

import argparse
import json
import statistics
from pathlib import Path


def summarize(paths):
    groups = {"first": [], "subsequent": [], "all": []}
    runs = []
    for path in paths:
        recorded = json.loads(path.read_text())
        rows = recorded["rows"]
        if not rows or [row["repeat"] for row in rows] != list(range(len(rows))):
            raise ValueError(f"{path}: expected consecutive repeats starting at zero")
        if any(not row["restored"] or row["verified_posts"] < 1 for row in rows):
            raise ValueError(f"{path}: injection correctness checks did not pass")
        groups["first"].append(rows[0])
        groups["subsequent"].extend(rows[1:])
        groups["all"].extend(rows)
        runs.append(
            {
                "path": str(path.resolve()),
                "samples": len(rows),
                "injection_median_ms": statistics.median(
                    row["injection_ms"] for row in rows
                ),
            }
        )
    result = {"runs": runs}
    for name, rows in groups.items():
        result[name] = {"samples": len(rows)}
        if rows:
            result[name].update(
                {
                    key: statistics.median(row[key] for row in rows)
                    for key in ("resolve_ms", "add_ms", "injection_ms")
                }
            )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, nargs="+", required=True)
    parser.add_argument("--candidate", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {
        "baseline": summarize(args.baseline),
        "candidate": summarize(args.candidate),
    }
    result["injection_reduction_percent"] = {
        name: 100
        * (
            1
            - result["candidate"][name]["injection_ms"]
            / result["baseline"][name]["injection_ms"]
        )
        for name in ("first", "subsequent", "all")
        if result["baseline"][name]["samples"] and result["candidate"][name]["samples"]
    }
    with args.output.open("x") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

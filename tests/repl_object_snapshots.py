"""Exercise logical containers, tagged values and a secondary base using an unchanged ELF."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, send, stop


def run(args):
    sources = ("fixture", "state.hpp", "policy.hpp", "state.cpp")
    original = {name: hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() for name in sources}
    with tempfile.TemporaryDirectory(prefix="ct-complex-object-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        for name in sources:
            shutil.copy2(args.fixture_dir / name, root / name)
        compdb = (args.fixture_dir / "compile_commands.json").read_text()
        (root / "compile_commands.json").write_text(compdb.replace(str(args.fixture_dir), str(root)))

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([root / "fixture"])
        app.expect(r"^READY$")
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent])
        field_names = ["items_", "large_", "nested_", "map_", "hash_", "note_", "extension_", "child__snapshot"]
        method_names = ["rehash_only", "mutate", "make_valueless", "replace_child"]
        if args.registered_any:
            field_names.extend(("extension__snapshot", "alias__snapshot"))
            method_names.append("update_registered")
        fields = " ".join(f"--field {field}" for field in field_names)
        methods = " ".join(f'--on "snapshot_test::State::{method}()"' for method in method_names)
        synchronization = "stable-at-boundaries" if args.registered_any else "thread-confined"
        send(repl, f"%object prepare snapshot_test::State {methods} {fields} --key map_=key --key hash_=key "
                   "--include state.hpp --policy policy.hpp --compdb compile_commands.json "
                   f"--translation-unit state.cpp --synchronization {synchronization} "
                   "--module fixture --output watch")
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        start_watch = "%object start --plan watch --max-instances 1 --max-snapshot-bytes 65536 --max-depth 64 --max-elements 5000"
        send(repl, start_watch)
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        probes = {out.expect(r"^method_probe=(\d+) active=yes restored=no$")[1] for _ in method_names}
        redirects = {}

        def live_bytes(pc, width):
            with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
                return os.pread(memory.fileno(), width, pc)

        if platform.machine() == "aarch64":
            count = int(watch.string.split("redirects=", 1)[1].split()[0])
            assert count >= 2 * len(probes)
            for _ in range(count):
                row = out.expect(r"^redirect=(0x[0-9a-f]+) translated=0x[0-9a-f]+ "
                                 r"original=([0-9a-f]+) installed=([0-9a-f]+)$")
                pc = int(row[1], 16)
                before, armed = bytes.fromhex(row[2]), bytes.fromhex(row[3])
                assert pc not in redirects and before != armed and live_bytes(pc, len(armed)) == armed
                redirects[pc] = before
        send(repl, start_watch)
        out.expect(r"^error: RPC 6: a snapshot watch is already active$")
        send(repl, f"%record complex.db --sqlite --probe {watch[1]} --name object.trace --count 32 --timeout 5")
        (root / "mutate.now").touch()
        try:
            app.expect(r"^DONE$")
        except AssertionError as error:
            raise AssertionError((target.poll(), "".join(out.seen[-30:]))) from error
        count = int(out.expect(r"^received=(\d+) .*timed_out=true")[1])
        send(repl, f"%object stop {watch[1]}")
        assert {out.expect(r"^method_probe=(\d+) active=no restored=yes$")[1] for _ in method_names} == probes
        assert all(live_bytes(pc, len(before)) == before for pc, before in redirects.items())
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open complex.db --as saved")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(repl, "%events export saved --format json --output complex.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 1
        records = [event["objectObservation"] for event in json.loads((root / "complex.json").read_text())]
        assert not any("receiver admission limit" in item.get("diagnostic", "") for item in records)
        assert all("::rehash_only()" not in item["function"] or item["kind"] == "COVERAGE_GAP"
                   for item in records), records
        mutate = {(item["field"], item["diagnostic"]): item for item in records
                  if item["function"].endswith("::mutate()")}
        assert ("items_[350].value", "350 -> 700") in mutate, mutate
        assert ("large_[4200]", "21 -> 45") in mutate, mutate
        assert ("nested_[1][0]", "7 -> 15") in mutate, mutate
        assert mutate["map_[7]", "removed"]["snapshotBefore"]["signedValue"] == "10", mutate
        assert mutate["map_[12]", "added"]["snapshotAfter"]["signedValue"] == "30", mutate
        assert ("map_[9]", "20 -> 21") in mutate, mutate
        assert ("note_", "index 0 -> index 1") in mutate, mutate
        if args.registered_any:
            assert all(item["synchronization"] == "STABLE_AT_BOUNDARIES" for item in records)
            assert any(item["function"].endswith("::update_registered()") and
                       item["field"] == "extension__snapshot.value" and item["diagnostic"] == "12 -> 18"
                       for item in records), records
            assert mutate["extension__snapshot", "unregistered std::any payload"]["kind"] == "COVERAGE_GAP"
            assert any(item["field"] == "alias__snapshot" and item["kind"] == "COVERAGE_GAP" and
                       "alias" in item["diagnostic"] for item in records), records
        else:
            assert ("extension_", "string -> opaque") in mutate, mutate
        assert mutate["extension_", "opaque std::any payload"]["kind"] == "COVERAGE_GAP", mutate
        assert ("child__snapshot.visible", "11 -> 17") in mutate, mutate
        assert any(item["function"].endswith("::make_valueless()") and item["field"] == "note_" and
                   item["diagnostic"] == "index 1 -> valueless" for item in records), records
        assert any(item["function"].endswith("::replace_child()") and item["field"] == "child__snapshot" and
                   item["kind"] == "COVERAGE_GAP" for item in records), records
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        for name, digest in original.items():
            assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
            assert hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() == digest
        print(f"verified {count} logical deltas and gaps; rehash had no logical delta; restored {len(probes)} method probes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--registered-any", action="store_true")
    run(parser.parse_args())


if __name__ == "__main__":
    main()

"""Check that overlapping scalar stores retain operands without inventing old values."""

import argparse
from contextlib import ExitStack
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_redirects, send, stop, verify_restored_redirects


def run(args, no_values, vector_length=None):
    with tempfile.TemporaryDirectory(prefix="ct-object-overlap-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv, env=None):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, env=env)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        env = None if vector_length is None else dict(os.environ, TRACE_TEST_SVE_VL=str(vector_length))
        target, app = start([args.fixture_dir / "fixture"], env)
        if vector_length is not None:
            extension = "SME" if args.sme_state else "SVE"
            actual_vl = int(app.expect(rf"^{extension} vector length=(\d+)$")[1])
            assert 16 <= actual_vl <= vector_length and actual_vl % 16 == 0
            print(f"{extension} vector length={actual_vl}", flush=True)
        app.expect(r"^READY$")
        preexisting = tuple(int(task.name) for task in Path(f"/proc/{target.pid}/task").iterdir()
                            if int(task.name) != target.pid)
        if args.worker_attach:
            assert len(preexisting) == 1, preexisting
        live_read = None
        if platform.machine() == "aarch64":
            memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

            def live_read(address, count):
                return os.pread(memory.fileno(), count, address)

        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast"])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        spec = "--backend code-cache --type overlap_test::Record --field value --code overlap_write"
        if args.ordered:
            spec += " --serialize-wait-us 100000"
        if args.rendezvous:
            spec += f" --rendezvous {args.rendezvous}"
        send(repl, "%object start " + spec + (" --no-values" if no_values else ""))
        stores = r"[1-9]\d*" if args.sme_state else "1"
        watch = out.expect(rf"^watch=(\d+) active=yes stores={stores} .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, preexisting, live_read=live_read)
        count = 3 if args.external else 1025
        send(repl, f"%record overlap.db --sqlite --probe {watch[1]} --count {count} --timeout 20")
        (root / "write.now").touch()
        final_pattern = "1001" if args.external else "1511|2511"
        final_value = int(app.expect(rf"^DONE value=({final_pattern})$")[1])
        out.expect(rf"^received={count} .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        status = out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        if not no_values and (args.external or not args.ordered):
            assert " coverage_gaps=" in status.string and " coverage=partial" in status.string, status.string
        token = maps(repl, out) if live_read is None else None
        verify_restored_redirects(repl, out, token, target.pid, redirects, live_read=live_read)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open overlap.db --as overlap")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(repl, "%events export overlap --format json --output overlap.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        rows = json.loads((root / "overlap.json").read_text())
        writes = [row for row in rows if row["objectObservation"]["locationKind"] == "STORE"]
        assert len(writes) == count - 1
        assert len({row["tid"] for row in writes}) == (1 if args.external else 2)
        items = [row["objectObservation"] for row in writes]
        assert {item["lifetimeId"] for item in items} == {"1"}
        assert len({item["pc"] for item in items}) == 1
        uncertain = [item for item in items if item.get("diagnostic") ==
                     "overlapping observed writes; old value unavailable"]
        if args.ordered and not args.external:
            assert all("diagnostic" not in item for item in items), items
        if no_values:
            assert not uncertain
            assert all(item["kind"] == "FIELD_WRITTEN" and
                       "oldInteger" not in item and "newInteger" not in item for item in items)
        elif args.external:
            first, second = items
            assert (first["oldInteger"], first["newInteger"]) == (17, 1000)
            assert second["kind"] == "FIELD_WRITTEN" and second["newInteger"] == 1001
            assert "oldInteger" not in second
            assert second["diagnostic"] == "field changed between observed stores; old value unavailable"
        elif args.ordered:
            assert [item["oldInteger"] for item in items] == [17] + [
                item["newInteger"] for item in items[:-1]]
            assert items[-1]["newInteger"] == final_value
        else:
            assert uncertain and all(item["kind"] == "FIELD_WRITTEN" and "oldInteger" not in item
                                     for item in uncertain)
            assert {item["newInteger"] for item in items} == set(range(1000, 1512)) | set(range(2000, 2512))
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"values={not no_values} observed {count - 1} writes, "
          f"{sum('oldInteger' not in item for item in items) if not no_values else 0} "
          f"old values withheld, {len(redirects)} redirects restored")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--ordered", action="store_true")
    parser.add_argument("--external", action="store_true")
    parser.add_argument("--worker-attach", action="store_true")
    parser.add_argument("--sve-state", action="store_true")
    parser.add_argument("--sme-state", action="store_true")
    parser.add_argument("--rendezvous", metavar="SYMBOL")
    args = parser.parse_args()
    if args.external:
        args.ordered = True
    vector_lengths = (None,)
    if args.sve_state or args.sme_state:
        libc = ctypes.CDLL(None)
        libc.getauxval.restype = ctypes.c_ulong
        if platform.machine() != "aarch64" or not (
                libc.getauxval(26) & (1 << 23) if args.sme_state else libc.getauxval(16) & (1 << 22)):
            sys.exit(77)
        args.rendezvous = "overlap_write"
        vector_lengths = (16, 64)
    originals = {file: hashlib.sha256(file.read_bytes()).digest()
                 for file in (args.fixture_dir / "fixture", args.fixture_dir / "fixture.cpp")}
    for vector_length in vector_lengths:
        for no_values in (False, True):
            run(args, no_values, vector_length)
    assert all(hashlib.sha256(file.read_bytes()).digest() == digest for file, digest in originals.items())

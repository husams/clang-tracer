"""Verify logical mutation boundaries on one unchanged executable."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, read_redirects, send, stop, verify_restored_redirects


def run(args):
    digest = hashlib.sha256(args.fixture.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix="ct-structural-") as temporary, ExitStack() as cleanup:
        root = Path(temporary)

        def start(argv):
            child = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, child)
            return child, Output(child.stdout, 35)

        target, app = start([args.fixture])
        app.expect(r"^READY$")
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        mutations = " ".join(f'--mutation "structural_test::{name}"'
                             for name in ("grow", "move", "swap", "insert", "erase"))
        send(repl, f'%object prepare structural_test::Board --module "{args.fixture}" '
                   '--mode logical-writes --field state.quantity --vector entries --element-key id=12 '
                   '--code "structural_test::exercise" --synchronization thread-confined '
                   f'--backend {args.backend} {mutations} '
                   '--mutation "destroy=structural_test::destroy" --output watch')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, "%object preview --plan watch")
        preview = out.expect(r"^watch=0 active=no stores=(\d+) .*|^error:")
        assert preview[1], preview.string
        out.expect(r"^binding=same-build$")
        sites = {}
        for _ in range(int(preview[1])):
            row = out.expect(r'^site role="store" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
            sites[int(row[1], 16)] = bytes.fromhex(row[2])
        send(repl, "%object start --plan watch")
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, args.backend, target.pid, ())
        send(repl, f"%record delta.db --sqlite --probe {watch[1]} --count 64 --timeout 5")
        (root / "go").touch()
        app.expect(r"^DONE$")
        count = int(out.expect(r"^received=(\d+) .*timed_out=true")[1])
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        token = maps(repl, out)
        verify_restored_redirects(repl, out, token, target.pid, redirects)
        assert all(read_code(repl, out, pc, token, len(code)) == code for pc, code in sites.items())
        send(repl, "%detach")
        out.expect(r"^detached session=")
        (root / "stop").touch()
        assert target.wait(timeout=10) == 0
        send(repl, "%events open delta.db --as saved")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(repl, "%events export saved --format json --output delta.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0, "".join(out.seen[-12:])
        records = [event["objectObservation"] for event in json.loads((root / "delta.json").read_text())]
        updates = [item for item in records if item["kind"] == "LOGICAL_UPDATE"]
        expected = [
            ("RESIZED", 100, 100), ("MOVED", 100, 100), ("ERASED", 100, None),
            ("INSERTED", None, 70), ("ERASED", 71, None), ("INSERTED", None, 70),
            ("DESTROYED", 70, None),
        ]
        assert [(item["logicalUpdate"]["change"], item.get("oldInteger"), item.get("newInteger"))
                for item in updates] == expected, records
        assert updates[0]["logicalUpdate"]["beforeSize"] == "2"
        assert updates[0]["logicalUpdate"]["afterSize"] == "4"
        assert updates[1]["logicalUpdate"]["beforeIndex"] == "0"
        assert updates[1]["logicalUpdate"]["afterIndex"] == "3"
        assert [item["logicalUpdate"]["mutation"] for item in updates] == [
            f"structural_test::{name}" for name in ("grow", "move", "swap", "insert", "erase", "insert", "destroy")
        ], updates
        assert all(item["locationKind"] == "CALL_BOUNDARY" and item["line"] > 0 for item in updates), updates
        assert not any(item["kind"] == "COVERAGE_GAP" for item in records), records
        stores = [item for item in records if item["kind"] == "FIELD_CHANGED"]
        assert [(item["oldInteger"], item["newInteger"], item["locationKind"])
                for item in stores] == [(70, 71, "STORE")], stores
        assert hashlib.sha256(args.fixture.read_bytes()).hexdigest() == digest
        print(f"{args.backend}: 7 logical updates, 1 store, restored bytes, unchanged ELF {digest}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), required=True)
    run(parser.parse_args())

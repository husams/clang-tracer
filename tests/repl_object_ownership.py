"""Verify ownership transfer preserves one lifetime across owners."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, read_redirects, send, stop, verify_restored_redirects


def run(args, no_values):
    preserved = ("fixture", "fixture.cpp")
    original = {name: hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() for name in preserved}
    with tempfile.TemporaryDirectory(prefix="ct-object-ownership-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        for name in preserved:
            shutil.copy2(args.fixture_dir / name, root / name)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([root / "fixture"])
        app.expect(r"^READY$")
        if args.arm:
            memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def live(pc, size):
            if args.arm:
                return os.pread(memory.fileno(), size, pc)
            return read_code(repl, out, pc, maps(repl, out), size)

        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast"])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        definition = ('--type ownership_test::Circle --base ownership_test::Shape '
                      '--owner ownership_test::Image --collection shapes_ '
                      '--field radius_ --code "ownership_test::Circle::set_radius(int)"')
        definition += (' --backend code-cache --synchronization thread-confined'
                       if args.backend == "code-cache" else ' --snapshot-field description_')
        rendezvous = " --rendezvous arm_rendezvous" if args.arm and args.backend == "code-cache" else ""
        if no_values:
            definition += " --no-values"
        send(repl, f"%object preview {definition}")
        site = out.expect(r'^site role="store" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
        pc, before = int(site[1], 16), bytes.fromhex(site[2])
        assert live(pc, len(before)) == before
        send(repl, f"%object start {definition}{rendezvous}")
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, args.backend, target.pid, (),
                                   live_read=live if args.arm else None)
        send(repl, f"%record transfer.db --sqlite --name object.trace --probe {watch[1]} --count 11 --timeout 20")
        (root / "transfer.now").touch()
        if args.arm:
            (root / "write.now").touch()
        owners = app.expect(r"^OWNERS first=(0x\w+) second=(0x\w+) circle=(0x\w+)$")
        first, second, address = [str(int(item, 16)) for item in owners.groups()]
        out.expect(r"^received=11 .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        token = None if args.arm else maps(repl, out)
        assert live(pc, len(before)) == before
        verify_restored_redirects(repl, out, token, target.pid, redirects,
                                  live_read=live if args.arm else None)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open transfer.db --as transfer")
        out.expect(r"^opened events=11 retained=11 evicted=0$")
        send(repl, "%events export transfer --format json --output transfer.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        events = json.loads((root / "transfer.json").read_text())
        items = [event["objectObservation"] for event in events]
        write = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        assert [item["kind"] for item in items] == [
            "CREATED", "OWNER_ADDED", "BORROW_RETURN", write, "OWNER_REMOVED",
            "OWNER_ADDED", "BORROW_RETURN", write, "DESTROY_BEGIN",
            "TEARDOWN_END", "OWNER_REMOVED"], items
        assert {item["lifetimeId"] for item in items} == {"1"}
        assert {item["generation"] for item in items} == {"1"}
        assert {event["objectAddress"] for event in events} == {address}
        assert [item["ownerAddress"] for item in items[1:8]] == [first] * 4 + [second] * 3
        if no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in items)
        else:
            assert [(items[index]["oldInteger"], items[index]["newInteger"]) for index in (3, 7)] == [
                (40, 41), (41, 42)]
        assert items[4]["diagnostic"] == "previous membership absent at transfer boundary"
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        for name, digest in original.items():
            assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
            assert hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() == digest
    print(f"{args.backend} values={not no_values} ownership transferred between two receivers with one "
          f"lifetime, two writes and exact restoration of {len(redirects)} redirects")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="ptrace")
    parser.add_argument("--no-values", action="store_true")
    parser.add_argument("--arm", action="store_true")
    args = parser.parse_args()
    run(args, args.no_values)

"""Verify a verified discriminator selects typed samples from a preserved union."""

import argparse
from base64 import b64decode
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import (Output, assert_anonymous_rx, assert_untraced_tasks, maps, read_code,
                 read_redirects, send, stop, verify_restored_redirects)


def run(args, backend, no_values):
    fixture = args.fixture_dir / args.fixture_name
    with tempfile.TemporaryDirectory(prefix="ct-tagged-union-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([fixture])
        app.expect(r"^READY$")
        if args.arm:
            memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def live(pc, size):
            if args.arm:
                memory.seek(pc)
                return memory.read(size)
            return read_code(repl, out, pc, maps(repl, out), size)

        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        send(repl, f'%object prepare tagged_test::State --module "{fixture}" --field payload.integer '
                   '--code _ZN11tagged_test10write_bitsERNS_5StateEj '
                   '--union-tag kind --union-case 1=integer --union-case 2=real '
                   f'--synchronization thread-confined --backend {backend} --output watch')
        package = out.expect(r"^object package=|^error:")
        assert package.string.startswith("object package="), package.string
        manifest = (root / "watch" / "object-plan.pb").read_bytes()
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, "%object preview --plan watch")
        out.expect(r'^field=payload.integer encoding="tagged union" offset=4 ')
        site = out.expect(r'^site role="store" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
        pc, before = int(site[1], 16), bytes.fromhex(site[2])
        assert live(pc, len(before)) == before
        rendezvous = " --rendezvous arm_rendezvous" if args.arm and backend == "code-cache" else ""
        send(repl, "%object start --plan watch" + rendezvous + (" --no-values" if no_values else ""))
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        if args.arm:
            count = int(re.search(r"redirects=(\d+)", watch.string)[1])
            redirects = {}
            assert bool(count) == (backend == "code-cache")
            if not count:
                assert live(pc, 4) == bytes.fromhex("000020d4")
            for _ in range(count):
                record = out.expect(r"^redirect=(0x[0-9a-f]+) translated=(0x[0-9a-f]+) "
                                    r"original=([0-9a-f]+) installed=([0-9a-f]+)$")
                start, translated = (int(value, 16) for value in record.groups()[:2])
                original, armed = (bytes.fromhex(value) for value in record.groups()[2:])
                instruction = int.from_bytes(armed, "little")
                assert len(original) == len(armed) == 4 and instruction >> 26 == 5
                delta = (instruction & 0x03ffffff) << 2
                assert start + (delta if delta < 1 << 27 else delta - (1 << 28)) == translated
                assert_anonymous_rx(target.pid, translated)
                assert live(start, 4) == armed
                redirects[start] = original
            if redirects:
                assert_untraced_tasks(target.pid, ())
        else:
            redirects = read_redirects(repl, out, watch, backend, target.pid, ())
        send(repl, f"%record tagged.db --sqlite --probe {watch[1]} --count 6 --timeout 20")
        (root / "write.now").touch()
        app.expect(r"^DONE integer=23 real=2.5 raw=01020304$")
        out.expect(r"^received=6 .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        assert live(pc, len(before)) == before
        if args.arm:
            assert all(live(start, len(original)) == original for start, original in redirects.items())
        else:
            verify_restored_redirects(repl, out, maps(repl, out), target.pid, redirects)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open tagged.db --as tagged")
        out.expect(r"^opened events=6 retained=6 evicted=0$")
        if not no_values:
            send(repl, "%events show tagged --index 3")
            out.expect(r" tag=1 integer=-2 -> tag=1 integer=23 ")
        send(repl, "%events export tagged --format json --output tagged.json")
        send(repl, "%quit")
        assert repl.wait(timeout=15) == 0, "".join(out.seen[-15:])
        events = json.loads((root / "tagged.json").read_text())
        items = [event["objectObservation"] for event in events]
        assert [item["kind"] for item in items] == ["CREATED"] * 3 + [
            "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"] * 3, items
        assert [item["lifetimeId"] for item in items] == ["1", "2", "3"] * 2
        assert all(item["field"] == "tagged_test::State::payload" and
                   item["synchronization"] == "THREAD_CONFINED" for item in items)
        assert all(item["pc"] == str(pc) and item["locationKind"] == "STORE" for item in items[3:])
        assert all(int(event["objectAddress"]) + 4 == int(item["destination"])
                   for event, item in zip(events[3:], items[3:]))
        if no_values:
            forbidden = {"oldUnionTag", "newUnionTag", "oldUnionMember", "newUnionMember",
                         "snapshotBefore", "snapshotAfter", "oldBytes", "newBytes"}
            assert all(not forbidden.intersection(item) for item in items), items
        else:
            for item, tag, name, encoding, old, new in (
                    (items[3], 1, "integer", "signedValue", "-2", "23"),
                    (items[4], 2, "real", "realValue", 1.5, 2.5)):
                assert (item["oldUnionTag"], item["newUnionTag"]) == (tag, tag), item
                assert item["oldUnionMember"] == item["newUnionMember"] == name, item
                assert item["snapshotBefore"] == {encoding: old}, item
                assert item["snapshotAfter"] == {encoding: new}, item
                assert "oldBytes" not in item and "newBytes" not in item
            unknown = items[5]
            assert (unknown["oldUnionTag"], unknown["newUnionTag"]) == (3, 3), unknown
            assert b64decode(unknown["oldBytes"]) == bytes.fromhex("78563412")
            assert b64decode(unknown["newBytes"]) == bytes.fromhex("04030201")
            assert "snapshotBefore" not in unknown and "snapshotAfter" not in unknown
            assert "unregistered" in unknown["diagnostic"]
        assert (root / "watch" / "object-plan.pb").read_bytes() == manifest
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"backend={backend} values={not no_values} integer, float and unknown tag; restored {len(redirects)} redirects")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--arm", action="store_true")
    parser.add_argument("--fixture-name", default="fixture")
    args = parser.parse_args()
    originals = {name: hashlib.sha256((args.fixture_dir / name).read_bytes()).digest()
                 for name in (args.fixture_name, "state.hpp", "state.cpp", "driver.cpp")}
    for backend in ("ptrace", "code-cache"):
        for no_values in (False, True):
            run(args, backend, no_values)
    assert all(hashlib.sha256((args.fixture_dir / name).read_bytes()).digest() == digest
               for name, digest in originals.items())

"""Verify original stores to nested union storage and its discriminator on Linux."""

import argparse
from base64 import b64decode
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, read_redirects, send, stop, verify_restored_redirects


def integer(value):
    return struct.pack("<Q", value)


def run(args):
    digest = hashlib.sha256(args.fixture.read_bytes()).hexdigest()
    scenario = "wide" if args.scenario in ("wide", "wide_member", "wide_leaf") else args.scenario
    writer = {"wide": "wide", "pair": "pair", "switch": "switch_tag"}.get(scenario, "tagged")
    with tempfile.TemporaryDirectory(prefix="ct-nested-union-") as temporary, ExitStack() as cleanup:
        root = Path(temporary)

        def start(argv):
            child = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, child)
            return child, Output(child.stdout, 40)

        target, app = start([args.fixture, scenario])
        app.expect(r"^READY$")
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        field = "envelope.state.payload"
        options = f"--backend {args.backend} --synchronization thread-confined"
        if not args.lifecycle:
            options += " --first-seen"
        if args.no_values:
            options += " --no-values"
        if args.scenario == "wide":
            field += ".wide.last"
        elif args.scenario != "pair":
            options += (" --union-tag envelope.state.kind --union-case 4294967297=number"
                        " --union-case 4294967298=real --union-case 3=" +
                        ("wide.last" if args.scenario == "wide_leaf" else "wide"))
        send(repl, f'%object prepare union_test::Cell --module "{args.fixture}" --field {field} '
                   f'--code "union_test::Cell::{writer}()" {options} --output watch')
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
        send(repl, f"%record union.db --sqlite --probe {watch[1]} --count 24 --timeout 3")
        (root / "go").touch()
        app.expect(r"^DONE$")
        received = int(out.expect(r"^received=(\d+) .*timed_out=true")[1])
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        token = maps(repl, out)
        verify_restored_redirects(repl, out, token, target.pid, redirects)
        assert all(read_code(repl, out, pc, token, len(code)) == code for pc, code in sites.items())
        send(repl, "%detach")
        out.expect(r"^detached session=")
        (root / "stop").touch()
        assert target.wait(timeout=10) == 0
        send(repl, "%events open union.db --as saved")
        out.expect(rf"^opened events={received} retained={received} evicted=0$")
        send(repl, "%events export saved --format json --output union.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0, "".join(out.seen[-12:])
        events = json.loads((root / "union.json").read_text())
        records = [event["objectObservation"] for event in events]
        assert not any(item["kind"] == "COVERAGE_GAP" for item in records), records
        if args.lifecycle:
            assert any(item["kind"] == "CREATED" for item in records), records
        else:
            assert any(item["kind"] in ("CREATED", "FIRST_SEEN") for item in records), records
        stores = [item for item in records if item["kind"] in ("FIELD_WRITTEN", "FIELD_CHANGED")]
        assert len(stores) == (3 if scenario == "switch" else 2), records
        assert len({item["pc"] for item in stores}) == len(stores), stores
        assert len({item["lifetimeId"] for item in stores}) == 1, stores
        for item in stores:
            assert item["locationKind"] == "STORE" and item["line"] > 0, item
            assert item["field"] == "union_test::Cell::envelope.state.payload", item
            assert item["storageWrite"]["size"] == (16 if scenario == "pair" else 8), item
        if args.no_values:
            assert all(item["kind"] == "FIELD_WRITTEN" and "oldBytes" not in item and "newBytes" not in item
                       and not ({"beforeDiscriminator", "afterDiscriminator"} & item["storageWrite"].keys())
                       for item in stores), stores
        elif scenario == "pair":
            before, after = struct.pack("<QQ", 1, 2), struct.pack("<QQ", 81, 93)
            assert [item["storageWrite"]["region"] for item in stores] == ["REPRESENTATION"] * 2
            assert [(b64decode(item["oldBytes"]), b64decode(item["newBytes"])) for item in stores] == [
                (before, after), (after, before)], stores
        elif scenario == "switch":
            tag, first, second = stores
            assert tag["storageWrite"]["region"] == "UNION_TAG", tag
            assert tag["storageWrite"]["beforeDiscriminator"] == str(0x100000001)
            assert tag["storageWrite"]["afterDiscriminator"] == str(0x100000002)
            assert b64decode(tag["newBytes"]) == integer(0x100000002)
            assert tag["oldUnionMember"] == "number" and tag["newUnionMember"] == "real"
            assert "snapshotBefore" not in tag and "snapshotAfter" not in tag, tag
            assert [item["snapshotAfter"] for item in (first, second)] == [
                {"realValue": 2.5}, {"realValue": 3.5}]
            assert "snapshotBefore" not in first, first
            assert second["snapshotBefore"] == {"realValue": 2.5}
        else:
            before, after = (44, 88) if scenario == "wide" else (7, 0xfedcba9876543210)
            assert [item["storageWrite"]["region"] for item in stores] == ["REPRESENTATION"] * 2
            assert [int(item["storageWrite"].get("offset", 0)) for item in stores] == [
                24 if scenario == "wide" else 0] * 2
            assert [(b64decode(item["oldBytes"]), b64decode(item["newBytes"])) for item in stores] == [
                (integer(before), integer(after)), (integer(after), integer(before))], stores
            if args.scenario in ("tagged", "wide_leaf"):
                label = "number" if args.scenario == "tagged" else "wide.last"
                assert all(item["oldUnionMember"] == item["newUnionMember"] == label for item in stores)
                assert stores[0]["snapshotBefore"] == {"unsignedValue": str(before)}
                assert stores[0]["snapshotAfter"] == {"unsignedValue": str(after)}
            else:
                assert all("snapshotBefore" not in item and "snapshotAfter" not in item for item in stores)
                if scenario == "unknown":
                    assert all("unregistered" in item["diagnostic"] for item in stores)
                if args.scenario == "wide_member":
                    assert all(item["newUnionMember"] == "wide" for item in stores)
        assert hashlib.sha256(args.fixture.read_bytes()).hexdigest() == digest
        print(f"{args.backend} {args.scenario}: {len(stores)} original stores, restored bytes, unchanged ELF {digest}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("scenario", choices=("wide", "wide_member", "wide_leaf", "tagged", "unknown", "switch", "pair"))
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), required=True)
    parser.add_argument("--no-values", action="store_true")
    parser.add_argument("--lifecycle", action="store_true")
    run(parser.parse_args())

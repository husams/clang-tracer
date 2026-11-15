"""Check per-instruction writes through private complex fields on an unchanged ELF."""

import argparse
import base64
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


def number(value):
    return value.to_bytes(4, byteorder="little")


# Every pair returns to its original logical value, so a method snapshot
# would miss all of these stores.
CASES = {
    "short_text": ("text_", "STRING_DATA", 1, b"h", b"X"),
    "long_text": ("text_", "STRING_DATA", 33, b"L", b"Y"),
    "delegated_text": ("text_", "STRING_DATA", 1, b"h", b"D"),
    "variant_value": ("variant_", "REPRESENTATION", 0, number(1), number(41)),
    "variant_text": ("variant_", "STRING_DATA", 33, b"L", b"V"),
    "variant_change": ("variant_", "REPRESENTATION", None, None, None),
    "variant_valueless": ("variant_", "REPRESENTATION", None, None, None),
    "any_text": ("extension_", "STRING_DATA", 33, b"L", b"A"),
    "any_inline": ("extension_", "REPRESENTATION", 8, number(4), number(42)),
    "any_heap": ("extension_", "ANY_PAYLOAD", 8, number(7), number(72)),
}


def run(args):
    digest = hashlib.sha256(args.fixture.read_bytes()).hexdigest()
    field, region, offset, original, replacement = CASES[args.scenario]
    with tempfile.TemporaryDirectory(prefix="ct-value-writes-") as temporary, ExitStack() as cleanup:
        root = Path(temporary)

        def start(argv):
            child = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, child)
            return child, Output(child.stdout, 40)

        target, app = start([args.fixture, args.scenario])
        app.expect(r"^READY$")
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        options = f"--backend {args.backend}" + (" --no-values" if args.no_values else "")
        if args.scenario == "delegated_text":
            options += " --follow-calls"
        transition = args.scenario in ("variant_change", "variant_valueless")
        send(repl, f'%object prepare complex_test::Cell --module "{args.fixture}" '
                   f'--first-seen --synchronization thread-confined --field {field} '
                   f'--code "complex_test::Cell::{args.scenario}()" {options} --output watch')
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
        send(repl, f"%record writes.db --sqlite --probe {watch[1]} --count 32 --timeout 3")
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
        send(repl, "%events open writes.db --as saved")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(repl, "%events export saved --format json --output writes.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0, "".join(out.seen[-12:])
        records = [event["objectObservation"] for event in json.loads((root / "writes.json").read_text())]
        assert not any(item["kind"] == "COVERAGE_GAP" for item in records), records
        stores = [item for item in records if item["kind"] in ("FIELD_WRITTEN", "FIELD_CHANGED")]
        assert len(stores) == 2, records
        assert stores[0]["pc"] != stores[1]["pc"], stores
        if not transition:
            assert stores[0]["line"] != stores[1]["line"], stores
        for item in stores:
            assert item["locationKind"] == "STORE" and item["line"] > 0, item
            assert item["field"] == "complex_test::Cell::" + field, item
            assert item["storageWrite"]["region"] == region, item
        if transition:
            assert {int(item["storageWrite"].get("offset", 0)) for item in stores} == {0, 32}, stores
            if not args.no_values:
                assert {base64.b64decode(item["newBytes"]) for item in stores} == {b"\x01", struct.pack("<d", 3.5)}, stores
                if args.scenario == "variant_valueless":
                    tag = next(item for item in stores if item["storageWrite"]["size"] == 1)
                    assert base64.b64decode(tag["oldBytes"]) == b"\xff", tag
        else:
            assert all(int(item["storageWrite"].get("offset", 0)) == offset for item in stores), stores
            assert all(item["storageWrite"]["size"] == len(original) for item in stores), stores
            if not args.no_values:
                assert [(base64.b64decode(item["oldBytes"]), base64.b64decode(item["newBytes"]))
                        for item in stores] == [(original, replacement), (replacement, original)], stores
        if args.no_values:
            assert all(item["kind"] == "FIELD_WRITTEN" and "oldBytes" not in item and "newBytes" not in item
                       for item in stores), stores
        else:
            assert all(item["kind"] == "FIELD_CHANGED" for item in stores), stores
        assert hashlib.sha256(args.fixture.read_bytes()).hexdigest() == digest
        print(f"{args.backend} {args.scenario}: 2 original stores, restored bytes, unchanged ELF {digest}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("scenario", choices=CASES)
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), required=True)
    parser.add_argument("--no-values", action="store_true")
    run(parser.parse_args())

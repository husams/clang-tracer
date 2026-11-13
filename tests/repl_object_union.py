"""Check that an untagged union reports overlapping storage bytes."""

import argparse
from base64 import b64decode
from contextlib import ExitStack
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, VALUE_FIELDS, maps, read_code, read_redirects, send, stop, verify_restored_redirects


def run(args, backend, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-object-union-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 20)

        endpoint = f"unix://{root / 'control.sock'}"
        _, server = start([args.server, "--listen", endpoint, "--agent", args.agent])
        server.expect("gRPC server listening")
        target, app = start([args.fixture])
        app.expect(r"^READY offset=0$")
        repl, out = start([args.repl, "--endpoint", endpoint, "--attach", str(target.pid)])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        option = " --no-values" if no_values else ""
        package = root / "watch"
        send(repl, f'%object prepare demo::PlanRecord --module "{args.fixture}" --field value '
                   '--code "demo::write_value(demo::PlanRecord &)" '
                   f'--backend {backend} --output "{package}"')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        out.expect(r"^binding=offline$")
        definition = f'--plan "{package}"{option}'
        send(repl, f"%object preview {definition}")
        out.expect(r"^binding=same-build$")
        out.expect(r'^field=value encoding="opaque union bytes" offset=0 build_id=[a-f0-9]+$')
        site = out.expect(r'^site role="store" elf=0x[a-f0-9]+ pc=(0x[a-f0-9]+) .*original=([a-f0-9]+)$')
        pc, original = int(site[1], 16), bytes.fromhex(site[2])
        assert read_code(repl, out, pc, maps(repl, out), len(original)) == original
        send(repl, f"%object start {definition}")
        watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, backend, target.pid, ())
        db, exported = root / "union.db", root / "union.json"
        send(repl, f"%record {db} --sqlite --name object.trace --probe {watch[1]} --count 2 --timeout 15")
        send(target, "g")
        app.expect(r"^WROTE .*value=23 ")
        out.expect(r"^received=2 .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        token = maps(repl, out)
        verify_restored_redirects(repl, out, token, target.pid, redirects)
        assert read_code(repl, out, pc, token, len(original)) == original
        send(repl, f"%events open {db} --as saved")
        out.expect(r"^opened events=2 retained=2 evicted=0$")
        send(repl, "%events show saved --index 1")
        kind = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        row = out.expect(rf" {kind} .*demo::PlanRecord::<storage>.*active member unavailable").string
        assert ("bytes=0000803f -> bytes=17000000" in row) == (not no_values)
        if no_values:
            assert "bytes=" not in row
        send(repl, f"%events export saved --format json --output {exported}")
        send(repl, "%detach")
        out.expect(r"^detached session=")
        created, write = (event["objectObservation"] for event in json.loads(exported.read_text()))
        assert created["kind"] == "CREATED" and write["kind"] == kind
        for event in (created, write):
            assert event["diagnostic"] == "untagged union storage; active member unavailable"
            assert not (VALUE_FIELDS - {"oldBytes", "newBytes"}).intersection(event)
            if no_values:
                assert not VALUE_FIELDS.intersection(event)
        if not no_values:
            assert b64decode(created["newBytes"]) == bytes.fromhex("0000803f")
            assert b64decode(write["oldBytes"]) == bytes.fromhex("0000803f")
            assert b64decode(write["newBytes"]) == bytes.fromhex("17000000")
        send(target, "q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=15) == 0
        send(repl, "%quit")
        assert repl.wait(timeout=15) == 0
        print(f"backend={backend} values={not no_values} storage confirmed; active member unavailable")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    args = parser.parse_args()
    for backend in ("ptrace", "code-cache"):
        for no_values in (False, True):
            run(args, backend, no_values)


if __name__ == "__main__":
    main()

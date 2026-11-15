"""Verify aggregate first-seen scopes, DWARF inline stacks and missing writer source."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, send, stop


def run(args):
    digest = hashlib.sha256(args.fixture.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix="ct-object-origin-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([args.fixture])
        app.expect(r"^READY$")
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast"])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        cases = (
            ("inline", '--type origin_test::Aggregate --field value --first-seen '
                       '--code "origin_test::write_inline(origin_test::Aggregate &)"', "i", 6, 91),
            ("opaque", "--type origin_test::Tracked --field value --code opaque_write", "o", 4, 81),
        )
        for name, definition, command, count, value in cases:
            send(repl, f"%object start {definition} --backend {args.backend}")
            watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
            assert watch[1], watch.string
            send(repl, f"%record {name}.db --sqlite --name object.trace --probe {watch[1]} --count {count} --timeout 20")
            for _ in range(2 if name == "inline" else 1):
                send(target, command)
                app.expect(rf"^{name}={value}$")
            out.expect(rf"^received={count} .*complete=true")
            send(repl, f"%object stop {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
            send(repl, f"%events open {name}.db --as {name}")
            out.expect(rf"^opened events={count} retained={count} evicted=0$")
            send(repl, f"%events export {name} --format json --output {name}.json")
        if args.limits:
            for backend, definition, command, value, active in (
                ("ptrace", cases[0][1], "i", 91, "no"),
                ("code-cache", cases[1][1], "o", 81, "yes"),
            ):
                send(repl, f"%object start {definition} --backend {backend} --tid {target.pid} --max-events 2")
                watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
                assert watch[1], watch.string
                send(repl, f"%record cap-{backend}.db --sqlite --probe {watch[1]} --count 2 --timeout 15")
                send(target, command)
                app.expect(rf"^(?:inline|opaque)={value}$")
                out.expect(r"^received=2 .*complete=true")
                send(repl, f"%object status {watch[1]}")
                out.expect(rf"^watch={watch[1]} active={active} .*observations=2 .*capture_limit=max-events ")
                send(repl, f"%object stop {watch[1]}")
                out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
            send(repl, f"%object start {cases[0][1]} --timeout 1")
            watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
            assert watch[1], watch.string
            time.sleep(1.2)
            send(repl, f"%object status {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*observations=0 .*capture_limit=timeout ")
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        records = json.loads((root / "inline.json").read_text())
        assert len({event["objectAddress"] for event in records}) == 1
        items = [event["objectObservation"] for event in records]
        assert [item["kind"] for item in items] == [
            "FIRST_SEEN", "FIELD_CHANGED", "OBSERVATION_ENDED"] * 2, items
        assert [item["lifetimeId"] for item in items] == ["1"] * 3 + ["2"] * 3, items
        assert not any(item.get("generation") for item in items), items
        assert items[2]["closureReason"] == items[5]["closureReason"] == "METHOD_RETURN"
        for item in (items[1], items[4]):
            assert (item["oldInteger"], item["newInteger"]) == (72, 91)
            assert item["locationKind"] == "STORE" and not item.get("sourceUnavailable")
            names = [entry["function"] for entry in item["inlineChain"]]
            assert "inner(" in names[0] and "write_inline(" in names[-1], names
        opaque = json.loads((root / "opaque.json").read_text())[1]["objectObservation"]
        assert opaque["kind"] == "FIELD_CHANGED"
        assert (opaque["oldInteger"], opaque["newInteger"]) == (73, 81)
        assert opaque["sourceUnavailable"] and "file" not in opaque and "line" not in opaque, opaque
        assert opaque["function"] == "opaque_write" and opaque["moduleBuildId"], opaque
        send(target, "q")
        assert target.wait(timeout=10) == 0
        assert hashlib.sha256(args.fixture.read_bytes()).hexdigest() == digest
        print(f"backend={args.backend} aggregate reused its address across two observation scopes, "
              "inline and missing origins replayed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--limits", action="store_true")
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="ptrace")
    run(parser.parse_args())


if __name__ == "__main__":
    main()

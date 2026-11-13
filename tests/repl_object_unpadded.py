"""Trace unpadded lifetimes and an optimized indirect writer through the REPL."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from contextlib import ExitStack

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, read_redirects, send, stop, verify_restored_redirects


def run(args):
    fixture = Path(args.fixture).resolve()
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    source_root = Path(__file__).resolve().parent / "fixtures" / "object_lifetime"
    sources = {path: hashlib.sha256(path.read_bytes()).digest()
               for path in source_root.glob("unpadded*.*")}
    cases = {
        "v": ("Virtual", "virtual", [(7, 13), (7, 13), (17, 23)]),
        "i": ("Inline", "inline", [(7, 13)]),
        "d": ("Delegating", "delegating", [(7, 13)]),
        "c": ("Callback", "callback", [(7, 13)]),
    }
    with tempfile.TemporaryDirectory(prefix="ct-object-unpadded-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def launch(*argv):
            child = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, cwd=root)
            cleanup.callback(stop, child)
            return child, Output(child.stdout, 20)

        target, app = launch(fixture)
        app.expect(r"^READY$")
        repl, out = launch(args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def read(pc, size):
            return os.pread(memory.fileno(), size, pc)

        for command in args.cases:
            record, writer, values = cases[command]
            options = (f"--type life::{record} --field value --code write_{writer} "
                       f"--backend {args.backend}")
            if args.no_values:
                options += " --no-values"
            send(repl, f"%object preview {options}")
            out.expect(r"^watch=0 active=no stores=[1-9][0-9]* ")
            send(repl, f"%object start {options}")
            watch = out.expect(r"^watch=(\d+) active=yes stores=[1-9][0-9]* .*|^error:")
            assert watch[1], watch.string
            redirects = read_redirects(repl, out, watch, args.backend, target.pid, (), live_read=read)
            count = 4 * len(values)
            send(repl, f"%record {writer}.db --sqlite --probe {watch[1]} --count {count} --timeout 15")
            send(target, command)
            app.expect(r"^DONE$")
            out.expect(rf"^received={count} .*complete=true")
            send(repl, f"%object stop {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*observations={count} .*restoration_pending=no ")
            verify_restored_redirects(repl, out, None, target.pid, redirects, live_read=read)
            send(repl, f"%events open {writer}.db --as events_{writer}")
            out.expect(rf"^opened events={count} retained={count} evicted=0$")
            send(repl, f"%events export events_{writer} --format json --output {writer}.json")
            send(repl, f"%events list events_{writer}")
            out.expect(rf"^{count - 1} ")
            events = json.loads((root / f"{writer}.json").read_text())
            for index, (old, new) in enumerate(values):
                series = [event["objectObservation"] for event in events[index * 4:index * 4 + 4]]
                write_kind = "FIELD_WRITTEN" if args.no_values else "FIELD_CHANGED"
                assert [event["kind"] for event in series] == [
                    "CREATED", write_kind, "DESTROY_BEGIN", "TEARDOWN_END"], series
                assert len({event["lifetimeId"] for event in series}) == 1, series
                assert series[3]["closureReason"] == "DESTRUCTOR_RETURN", series
                write = series[1]
                assert write["function"] == f"write_{writer}" and int(write["elfPc"]) > 0, write
                assert Path(write["file"]).name == "unpadded.cpp" and write["line"] > 0, write
                assert write["locationKind"] == "STORE", write
                if args.no_values:
                    assert all("oldInteger" not in event and "newInteger" not in event for event in series)
                else:
                    assert (series[0]["newInteger"], write["oldInteger"], write["newInteger"]) == (old, old, new)
            if command == "v":
                assert events[4]["objectAddress"] == events[8]["objectAddress"]
                assert int(events[8]["objectObservation"]["generation"]) == 2

        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        send(target, "q")
        assert target.wait(timeout=10) == 0
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest
    assert all(hashlib.sha256(path.read_bytes()).digest() == value for path, value in sources.items())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "fixture", "cases"):
        parser.add_argument(name)
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="ptrace")
    parser.add_argument("--no-values", action="store_true")
    run(parser.parse_args())

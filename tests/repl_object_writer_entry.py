"""Verify a selected writer through direct calls, pointers, and virtual dispatch."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, read_redirects, send, stop, verify_restored_redirects


def run(args, record, writer):
    with tempfile.TemporaryDirectory(prefix="ct-object-writer-entry-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def launch(*command):
            process = subprocess.Popen(command, cwd=root, text=True, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 25)

        target, app = launch(args.fixture)
        app.expect(r"^READY$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def read(pc, count):
            return os.pread(memory.fileno(), count, pc)

        repl, out = launch(args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        options = f'--type writer_entry::{record} --field value --backend code-cache --code "{writer}"'
        if args.writer_entry:
            options += " --writer-entry"
        if args.no_values:
            options += " --no-values"
        send(repl, f"%object preview {options}")
        preview = out.expect(r"^watch=0 active=no stores=[1-9]\d* .*|^error:")
        assert preview.string.startswith("watch="), preview.string
        windows = {}
        for _ in range(int(preview.string.split("redirects=")[1].split()[0])):
            line = out.expect(r"^redirect=(0x[0-9a-f]+).* original=([0-9a-f]+)")
            pc, original = int(line[1], 16), bytes.fromhex(line[2])
            windows[pc] = original
            assert read(pc, len(original)) == original
        send(repl, f"%object start {options}")
        watch = out.expect(r"^watch=(\d+) active=yes stores=[1-9]\d* .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, (), live_read=read)
        assert windows == {pc: original for pc, (original, _, _) in redirects.items()}
        send(repl, f"%record writes.db --sqlite --probe {watch[1]} --count 6 --timeout 15")
        send(target, "g")
        app.expect(r"^DONE$")
        out.expect(r"^received=6 .*complete=true")
        if args.detach:
            send(repl, "%detach")
            out.expect(r"^detached session=")
        else:
            send(repl, f"%object stop {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*observations=6 .*restoration_pending=no ")
        verify_restored_redirects(repl, out, None, target.pid, redirects, live_read=read)
        send(repl, "%events open writes.db --as saved")
        out.expect(r"^opened events=6 retained=6 evicted=0$")
        send(repl, "%events export saved --format json --output writes.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        send(target, "q")
        assert target.wait(timeout=10) == 0

        events = [event["objectObservation"] for event in json.loads((root / "writes.json").read_text())]
        write_kind = "FIELD_WRITTEN" if args.no_values else "FIELD_CHANGED"
        assert [event["kind"] for event in events] == [
            "CREATED", *([write_kind] * 3), "DESTROY_BEGIN", "TEARDOWN_END"], events
        assert len({event["lifetimeId"] for event in events}) == 1, events
        writes = events[1:4]
        assert len({event["pc"] for event in writes}) == 1, writes
        assert all(event["function"] == writer.replace(" &", "&") and
                   event["locationKind"] == "STORE" and Path(event["file"]).name == "writer.cpp"
                   for event in writes), writes
        if args.no_values:
            assert all("oldInteger" not in event and "newInteger" not in event for event in writes), writes
        else:
            assert [(event["oldInteger"], event["newInteger"]) for event in writes] == [
                (1, 11), (11, 12), (12, 13)], writes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--writer-entry", action="store_true")
    parser.add_argument("--no-values", action="store_true")
    parser.add_argument("--detach", action="store_true")
    args = parser.parse_args()
    source = Path(__file__).resolve().parent / "fixtures" / "object_writer_entry"
    originals = {path: hashlib.sha256(path.read_bytes()).digest()
                 for path in [Path(args.fixture), *source.iterdir()] if path.is_file()}
    run(args, "Cell", "writer_entry::poke(writer_entry::Cell &, int)")
    run(args, "Knob", "writer_entry::Knob::set(int)")
    assert all(hashlib.sha256(path.read_bytes()).digest() == digest for path, digest in originals.items())

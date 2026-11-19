"""Restore both method probes repeatedly while the target sleeps at an RX page boundary."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import queue
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks/object-write"))
from run import Output, send, stop


def run(args):
    digest = hashlib.sha256(args.fixture.read_bytes()).digest()
    with tempfile.TemporaryDirectory(prefix="ct-arm-method-stop-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            child = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, child)
            return child, Output(child.stdout, 40)

        target, app = start([args.fixture])
        app.expect(r"^READY$")
        memory = os.open(f"/proc/{target.pid}/mem", os.O_RDONLY)
        cleanup.callback(os.close, memory)
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        translation = args.header.parent / "target.cpp"
        (root / "compile_commands.json").write_text(json.dumps([{
            "directory": str(args.header.parent), "file": str(translation),
            "arguments": ["c++", "-std=c++17", "-c", str(translation)],
        }]))
        send(repl, f'%object prepare boundary_test::Record --on "boundary_test::Record::first()" '
                   f'--on "boundary_test::Record::second()" --field values --include "{args.header}" '
                   f'--module "{args.fixture}" --compdb compile_commands.json '
                   f'--translation-unit "{translation}" --synchronization thread-confined --output watch')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(repl, f"%attach {target.pid}")
        session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        previous = 0
        for cycle in range(8):
            options = " --max-snapshot-bytes 64 --max-elements 8 --max-depth 2" if cycle % 2 else ""
            send(repl, "%object start --plan watch" + options)
            watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
            if not watch[1]:
                time.sleep(0.05)
                diagnostics = []
                while True:
                    try:
                        line = app.lines.get_nowait()
                    except queue.Empty:
                        break
                    if line:
                        diagnostics.append(line)
                raise AssertionError("".join(out.seen[-30:] + diagnostics))
            probes = {out.expect(r"^method_probe=(\d+) active=yes restored=no$")[1] for _ in range(2)}
            assert len(probes) == 2
            count = int(re.search(r"redirects=(\d+)", watch.string)[1])
            assert count >= 4, watch.string
            originals = {}
            for _ in range(count):
                row = out.expect(r"^redirect=(0x[0-9a-f]+) translated=0x[0-9a-f]+ "
                                 r"original=([0-9a-f]+) installed=([0-9a-f]+)$")
                pc = int(row[1], 16)
                original, installed = bytes.fromhex(row[2]), bytes.fromhex(row[3])
                assert pc not in originals and original != installed
                assert os.pread(memory, len(installed), pc) == installed
                originals[pc] = original
            send(repl, f"%record cycle-{cycle}.db --sqlite --probe {watch[1]} --name object.trace --count 12 --timeout 15")
            out.expect(r"^received=12 .*complete=true")
            send(repl, f"%object stop {watch[1]}")
            status = out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no .*|^error:")
            assert status.string.startswith(f"watch={watch[1]} active=no"), status.string
            restored = {out.expect(r"^method_probe=(\d+) active=no restored=yes$")[1] for _ in range(2)}
            assert restored == probes
            assert all(os.pread(memory, len(original), pc) == original for pc, original in originals.items())
            send(repl, "%sessions")
            out.expect(rf"^\* {re.escape(session)} pid={target.pid} state=ATTACHED ")
            send(target, "g")
            progress = int(app.expect(r"^PROGRESS (\d+)$")[1])
            assert progress > previous
            previous = progress
            print(f"cycle={cycle + 1} probes=2 restored={len(originals)} session=attached progress={progress}", flush=True)
        send(repl, "%detach")
        out.expect(rf"^detached session={re.escape(session)}$")
        send(repl, "%quit")
        assert repl.wait(timeout=20) == 0
        send(target, "q")
        assert target.wait(timeout=10) == 0
        assert hashlib.sha256(args.fixture.read_bytes()).digest() == digest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture", "header"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    run(parser.parse_args())

"""Verify a throwing method yields a retirement gap with no sampled delta."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, send, stop


def run(args):
    names = ("fixture", "state.hpp", "state.cpp")
    original = {name: hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() for name in names}
    with tempfile.TemporaryDirectory(prefix="ct-object-unwind-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        for name in names:
            shutil.copy2(args.fixture_dir / name, root / name)
        compdb = (args.fixture_dir / "compile_commands.json").read_text()
        (root / "compile_commands.json").write_text(compdb.replace(str(args.fixture_dir), str(root)))

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([root / "fixture"])
        app.expect(r"^READY$")
        command = [args.repl, "--server", args.server, "--agent", args.agent]
        repl, out = start(command if args.check_failed_start else [*command, "--fail-fast"])
        send(repl, '%object prepare snapshot_unwind::State --on "snapshot_unwind::State::fail()" '
                   '--include state.hpp --compdb compile_commands.json --translation-unit state.cpp '
                   '--field value_ --synchronization thread-confined --module fixture --output watch')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, "%object preview --plan watch")
        site = out.expect(r'^site role="method" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
        pc, before = int(site[1], 16), bytes.fromhex(site[2])
        def read_entry():
            if platform.machine() == "aarch64":
                with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
                    memory.seek(pc)
                    return memory.read(len(before))
            return read_code(repl, out, pc, maps(repl, out), len(before))

        assert read_entry() == before
        if args.check_failed_start:
            send(repl, '%object start --type snapshot_unwind::State --field value_ '
                       '--code "snapshot_unwind::State::write(int)" --first-seen')
            write = out.expect(r"^watch=(\d+) active=yes .*|^error:")
            assert write[1], write.string
            send(repl, "%object start --plan watch")
            out.expect(r"^error: .*stop the (active )?object watch")
            send(repl, "%object status")
            out.expect(rf"^watch={write[1]} active=yes ")
            send(repl, "%object stop")
            out.expect(rf"^watch={write[1]} active=no .*restoration_pending=no ")
        send(repl, "%object start --plan watch")
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        probe = out.expect(r"^method_probe=(\d+) active=yes restored=no$")
        assert read_entry() != before
        (root / "throw.now").touch()
        app.expect(r"^CAUGHT exception=42 field=11$")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*observations=1 .*coverage_gaps=1 ")
        out.expect(rf"^method_probe={probe[1]} active=no restored=yes$")
        assert read_entry() == before
        send(repl, f"%record unwind.db --sqlite --name object.trace --probe {watch[1]} --count 1 --timeout 10")
        out.expect(r"^received=1 .*complete=true")
        if args.reexec:
            send(repl, "%object start --plan watch")
            again = out.expect(r"^watch=(\d+) active=yes .*|^error:")
            assert again[1] and again[1] != watch[1], again.string
            out.expect(r"^method_probe=\d+ active=yes restored=no$")
            assert read_entry() != before
            (root / "exec.now").touch()
            app.expect(rf"^REEXEC pid={target.pid}$")
            send(repl, f"%object status {again[1]}")
            out.expect(rf"^watch={again[1]} active=no .*restoration_pending=no .*coverage_gaps=1 "
                       r".*target image changed and the original method redirects are gone$")
            out.expect(r"^method_probe=\d+ active=no restored=yes$")
            send(repl, "%object preview --plan watch")
            rebound = out.expect(r'^site role="method" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
            pc = int(rebound[1], 16)
            assert bytes.fromhex(rebound[2]) == before and read_entry() == before
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open unwind.db --as saved")
        out.expect(r"^opened events=1 retained=1 evicted=0$")
        send(repl, "%events export saved --format json --output unwind.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == int(args.check_failed_start)
        [event] = json.loads((root / "unwind.json").read_text())
        observation = event["objectObservation"]
        assert observation["kind"] == "COVERAGE_GAP"
        assert observation["locationKind"] == "CALL_BOUNDARY"
        assert observation["diagnostic"] == "method return unavailable at retirement"
        assert "snapshotBefore" not in observation and "snapshotAfter" not in observation
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        for name, digest in original.items():
            assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
            assert hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() == digest
    print("throw reached the application handler; one retirement gap, no delta and exact byte restoration")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--reexec", action="store_true")
    parser.add_argument("--check-failed-start", action="store_true")
    run(parser.parse_args())

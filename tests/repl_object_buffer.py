"""Verify overflow of a configured native event buffer without losing target writes."""

import argparse
from contextlib import ExitStack
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_redirects, send, stop, verify_restored_redirects


def run(args):
    count = 2000
    original = hashlib.sha256(args.fixture.read_bytes()).digest()
    with tempfile.TemporaryDirectory(prefix="ct-object-buffer-") as directory, ExitStack() as cleanup:
        def start(argv):
            process = subprocess.Popen(argv, cwd=directory, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([args.fixture, str(count)])
        tids = tuple(map(int, app.expect(r"^READY tids=(\d+),(\d+)$").groups()))
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, "%object start --type demo::Person --field age --code person_write_many "
                   "--backend code-cache --event-buffer 2 --no-values" +
                   (" --overflow stop" if args.stop_on_overflow else ""))
        watch = out.expect(r"^watch=(\d+) active=yes .*event_buffer=2 ")
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, tids)

        send(target, "s")
        app.expect(r"^SETUP ")
        send(target, "g")
        done = app.expect(r"^DONE ns=\d+ completed=(\d+),(\d+) correct=(\d+),(\d+)$")
        assert tuple(map(int, done.groups())) == (count // 2,) * 4, done.string

        send(repl, f"%object stop {watch[1]}")
        status = out.expect(rf"^watch={watch[1]} active=no .*observations=(\d+) .*"
                            rf"executed_stores={count} dropped=(\d+) .*event_buffer=2 ")
        observations, dropped = map(int, status.groups())
        assert 0 < observations and 0 < dropped < count * 2, status.string
        assert ("capture_limit=buffer-overflow" in status.string) == args.stop_on_overflow
        verify_restored_redirects(repl, out, maps(repl, out), target.pid, redirects)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%quit")
        assert repl.wait(timeout=15) == 0
        send(target, "q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=10) == 0
    assert hashlib.sha256(args.fixture.read_bytes()).digest() == original
    print(f"buffer=2 overflow={'stop' if args.stop_on_overflow else 'drop'} "
          f"recorded={observations} dropped={dropped} executed={count}; restored {len(redirects)} redirects")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--stop-on-overflow", action="store_true")
    run(parser.parse_args())

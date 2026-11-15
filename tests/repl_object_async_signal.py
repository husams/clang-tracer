"""Check native SIGUSR1 contexts and resumption on an unchanged prebuilt writer."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_redirects, send, stop, verify_restored_redirects


def run(args, redirect, detach, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-async-signal-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([args.fixture])
        app.expect(r"^READY$")
        live_read = None
        if platform.machine() == "aarch64":
            memory = os.open(f"/proc/{target.pid}/mem", os.O_RDONLY)
            cleanup.callback(os.close, memory)
            def live_read(address, count):
                return os.pread(memory, count, address)
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, "%object start --type demo::PlanRecord --field value --code async_write "
                   f"--backend code-cache --async-signal {int(signal.SIGUSR1)}" +
                   (f" --rendezvous {args.rendezvous}" if args.rendezvous else "") +
                   (" --serialize-wait-us 100000" if args.ordered else "") +
                   (" --no-values" if no_values else ""))
        watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, (),
                                   live_read=live_read)
        if not detach:
            send(repl, f"%record async.db --sqlite --probe {watch[1]} --count 2 --timeout 20")
        if args.rendezvous:
            (root / "write.now").touch()
        send(target, "r" if redirect else "g")
        bounds = app.expect(r"^SPINNING begin=(0x\w+) end=(0x\w+)$")
        begin, end = map(lambda value: int(value, 16), bounds.groups())
        time.sleep(0.025)
        if detach:
            send(repl, "%detach")
            out.expect(r"^detached session=")
        target.send_signal(signal.SIGUSR1)
        observed = app.expect(r"^DONE pc=(0x\w+) value=23$")
        assert begin <= int(observed[1], 16) < end, (bounds.string, observed.string)
        if detach:
            send(repl, f"%attach {target.pid}")
            out.expect(rf"^attached session=\S+ pid={target.pid}$")
        else:
            out.expect(r"^received=2 .*complete=true")
            send(repl, f"%object stop {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        token = maps(repl, out) if live_read is None else None
        verify_restored_redirects(repl, out, token, target.pid, redirects, live_read=live_read)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        if not detach:
            send(repl, "%events open async.db --as async_trace")
            out.expect(r"^opened events=2 retained=2 evicted=0$")
            send(repl, "%events export async_trace --format json --output async.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        if not detach:
            _, event = json.loads((root / "async.json").read_text())
            obs = event["objectObservation"]
            assert obs["kind"] == ("FIELD_WRITTEN" if no_values else "FIELD_CHANGED"), obs
            assert obs["function"] == "async_write" and obs["line"]
            if no_values:
                assert "oldInteger" not in obs and "newInteger" not in obs
            else:
                assert (obs["oldInteger"], obs["newInteger"]) == (17, 23)
        send(target, "q")
        assert target.wait(timeout=10) == 0
    print(f"redirect={redirect} detach={detach} values={not no_values} "
          f"application PC={observed[1]} restored={len(redirects)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--rendezvous", metavar="SYMBOL")
    parser.add_argument("--ordered", action="store_true")
    args = parser.parse_args()
    digest = hashlib.sha256(args.fixture.read_bytes()).digest()
    for no_values in (False, True):
        run(args, False, False, no_values)
        run(args, True, False, no_values)
        run(args, True, True, no_values)
    assert hashlib.sha256(args.fixture.read_bytes()).digest() == digest

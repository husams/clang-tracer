"""Observe an unchanged ELF before main on the diagnostic and native backends."""

import argparse
from base64 import b64decode
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, stop


def run(args, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-object-startup-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        request = root / "definition.json"
        definition = {
            "concreteType": "startup_test::Record", "field": "value",
            "codeSymbol": "startup_write", "noValues": no_values,
            "backend": "CODE_CACHE" if args.agent else "PTRACE",
        }
        if args.module:
            definition["codeModule"] = str(args.module)
        request.write_text(json.dumps(definition))
        launch = [args.tracer, "launch"]
        if args.agent:
            launch += ["--agent", args.agent]
        if args.loader_ready:
            launch += ["--loader-ready"]
        launch += [request, args.fixture]
        trace = subprocess.Popen([*launch, "hold"], cwd=root,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        cleanup.callback(lambda: (root / "stop.now").touch())
        cleanup.callback(stop, trace)
        stdout, stderr = Output(trace.stdout, 20), Output(trace.stderr, 20)
        checkpoint = ("loader-ready" if args.loader_ready else
                      "executable-entry" if args.agent else "exec-stop")
        started = json.loads(stdout.expect(rf'^\{{"coverageStart":"{checkpoint}","pid":[1-9][0-9]*\}}')[0])
        pid = started["pid"]
        if args.agent:
            armed = json.loads(stdout.expect(r'^\{"watchId":"1".*"active":true').string)
            redirects = armed["redirects"]
            assert redirects and armed["storeSites"] == 1
            memory = cleanup.enter_context(Path(f"/proc/{pid}/mem").open("rb", buffering=0))
        stderr.expect(r"^MAIN value=23$")
        if args.agent:
            for redirect in redirects:
                installed = b64decode(redirect["installed"])
                assert os.pread(memory.fileno(), len(installed), int(redirect["pc"])) == installed
            status = Path(f"/proc/{pid}/status").read_text()
            assert next(line for line in status.splitlines() if line.startswith("TracerPid:")).split()[1] == "0"
        trace.send_signal(signal.SIGINT)
        diagnostic = ("all code-cache redirects restored" if args.agent else
                      "original breakpoint bytes restored or target exited")
        stdout.expect(rf'^\{{"watchId":"1".*"diagnostic":"{diagnostic}')
        assert trace.wait(timeout=15) == 0
        status = Path(f"/proc/{pid}/status").read_text()
        assert next(line for line in status.splitlines() if line.startswith("TracerPid:")).split()[1] == "0"
        plan = json.loads(subprocess.check_output(
            [args.tracer, "preview", str(pid), request], cwd=root, text=True))
        if args.agent:
            for redirect in redirects:
                original = b64decode(redirect["original"])
                assert os.pread(memory.fileno(), len(original), int(redirect["pc"])) == original
            plan = plan["plan"]
        assert {item["role"] for item in plan["bindings"]} >= {"writer", "store", "construct", "destroy"}
        events = [row["objectObservation"] for line in stdout.seen if (row := json.loads(line))
                  and "objectObservation" in row]
        assert [item["kind"] for item in events] == [
            "CREATED", "FIELD_WRITTEN" if no_values else "FIELD_CHANGED", "DESTROY_BEGIN", "TEARDOWN_END"], events
        assert {item["lifetimeId"] for item in events} == {"1"}
        assert events[1]["locationKind"] == "STORE" and events[1]["function"] == "startup_write"
        if no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in events)
        else:
            assert (events[1]["oldInteger"], events[1]["newInteger"]) == (17, 23), events
        (root / "stop.now").touch()
        deadline = time.monotonic() + 5
        while Path(f"/proc/{pid}/exe").exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        assert not Path(f"/proc/{pid}/exe").exists()
        if args.module:
            (root / "preinit.now").unlink()
        run = subprocess.run(launch, cwd=root,
                             capture_output=True, text=True, timeout=20)
        assert run.returncode == 0 and "MAIN value=23" in run.stderr, run
        assert len([line for line in run.stdout.splitlines() if "objectObservation" in line]) == 4, run
    print(f"{checkpoint} values={not no_values} captured four events before main and verified live restoration")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("tracer", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--agent", type=lambda value: Path(value).resolve())
    parser.add_argument("--loader-ready", action="store_true")
    parser.add_argument("--module", type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    paths = (args.fixture, args.module) if args.module else (args.fixture,)
    digests = {path: hashlib.sha256(path.read_bytes()).digest() for path in paths}
    for no_values in (False, True):
        run(args, no_values)
    assert all(hashlib.sha256(path.read_bytes()).digest() == digest for path, digest in digests.items())

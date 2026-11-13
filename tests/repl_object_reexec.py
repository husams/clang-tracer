"""Check same-image exec while a native watch owns live redirects."""

import argparse
from base64 import b64decode
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, read_redirects, send, stop


def run_cli(args, root, target, app, live, no_values):
    request = root / "definition.json"
    request.write_text(json.dumps({"concreteType": "demo::PlanRecord", "field": "value",
                                   "codeSymbol": "demo::write_value(demo::PlanRecord&)",
                                   "backend": "CODE_CACHE", "noValues": no_values}))
    watch = subprocess.Popen([args.object_watch, "watch", str(target.pid), request, args.agent, "arm_rendezvous"],
                             cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
    try:
        output = Output(watch.stdout, 30)
        armed = json.loads(output.expect(r'^\{"watchId":.*"active":true.*\}$')[0])
        redirects = armed["redirects"]
        assert redirects
        for patch in redirects:
            installed = b64decode(patch["installed"])
            assert live(int(patch["pc"]), len(installed)) == installed
        (root / "write.now").touch()
        app.expect(r"^WROTE value=23$")
        (root / "exec.now").touch()
        app.expect(rf"^REEXEC pid={target.pid}$")
        # Keep the control input open. Image replacement itself must end the watch.
        assert watch.wait(timeout=5) == 0
        retired = json.loads(output.expect(r'^\{"watchId":.*old redirects are absent.*\}$')[0])
        assert not retired.get("active") and not retired.get("restorationPending")
        plan = json.loads(subprocess.check_output(
            [args.object_watch, "preview", str(target.pid), request], text=True))["plan"]
        [store] = [binding for binding in plan["bindings"] if binding["role"] == "store"]
        assert live(int(store["runtimePc"]), 4) == b64decode(store["original"])
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    finally:
        stop(watch)


def run(args, no_values, standalone=False):
    with tempfile.TemporaryDirectory(prefix="ct-object-exec-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([args.fixture])
        app.expect(r"^READY$")

        def live(pc, count):
            with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
                return os.pread(memory.fileno(), count, pc)

        if standalone:
            return run_cli(args, root, target, app, live, no_values)

        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        send(repl, f"%attach {target.pid}")
        session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        definition = ('--type demo::PlanRecord --field value '
                      '--code "demo::write_value(demo::PlanRecord&)" --backend code-cache')
        send(repl, f"%object preview {definition}")
        store = out.expect(r'^site role="store" elf=0x\w+ pc=(0x\w+) .*original=([0-9a-f]+)$')
        pc, original = int(store[1], 16), bytes.fromhex(store[2])
        assert live(pc, len(original)) == original
        rendezvous = " --rendezvous arm_rendezvous" if platform.machine() == "aarch64" else ""
        send(repl, f"%object start {definition}{rendezvous}" + (" --no-values" if no_values else ""))
        watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, (), live_read=live)
        send(repl, f"%record before.db --sqlite --probe {watch[1]} --count 2 --timeout 20")
        (root / "write.now").touch()
        app.expect(r"^WROTE value=23$")
        out.expect(r"^received=2 .*complete=true")
        (root / "exec.now").touch()
        app.expect(rf"^REEXEC pid={target.pid}$")
        send(repl, f"%object status {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no .*coverage=partial .*"
                   "execution mapping changed and old redirects are absent")
        send(repl, f"%record exec.db --sqlite --session {session} --probe {watch[1]} --count 4 --timeout 20")
        out.expect(r"^received=4 .*complete=true")
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, f"%object preview {definition}")
        rebound = out.expect(r'^site role="store" elf=0x\w+ pc=(0x\w+) .*original=([0-9a-f]+)$')
        assert bytes.fromhex(rebound[2]) == original
        delta = int(rebound[1], 16) - pc
        for old_pc, (before, _, _) in redirects.items():
            assert live(old_pc + delta, len(before)) == before
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open exec.db --as replaced")
        out.expect(r"^opened events=4 retained=4 evicted=0$")
        send(repl, "%events export replaced --format json --output exec.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        observations = [row["objectObservation"]
                        for row in json.loads((root / "exec.json").read_text())]
        assert [item["kind"] for item in observations] == [
            "CREATED", "FIELD_WRITTEN" if no_values else "FIELD_CHANGED",
            "COVERAGE_GAP", "OBSERVATION_ENDED"], observations
        assert observations[2]["closureReason"] == observations[3]["closureReason"] == "MAPPING_CHANGED"
        assert observations[0]["lifetimeId"] == observations[3]["lifetimeId"]
        if no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in observations)
        else:
            assert (observations[1]["oldInteger"], observations[1]["newInteger"]) == (17, 23)
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"values={not no_values} same-PID exec closed the lifetime and left {len(redirects)} original windows")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--object-watch", type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    digest = hashlib.sha256(args.fixture.read_bytes()).digest()
    for no_values in (False, True):
        run(args, no_values)
        if args.object_watch:
            run(args, no_values, standalone=True)
    assert hashlib.sha256(args.fixture.read_bytes()).digest() == digest

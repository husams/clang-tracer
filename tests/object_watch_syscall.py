"""Verify kernel output through read and pread on a preserved scalar field."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, stop


def run(args, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-object-kernel-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = subprocess.Popen([args.fixture], cwd=root, stdout=subprocess.PIPE, text=True)
        cleanup.callback(stop, target)
        app = Output(target.stdout, 20)
        app.expect(r"^READY$")
        definition = root / "definition.json"
        definition.write_text(json.dumps({
            "concreteType": "kernel_test::Record", "field": "value", "codeSymbol": "userspace_write",
            "syscallOutput": True, "synchronization": "THREAD_CONFINED", "noValues": no_values,
        }))
        trace = subprocess.Popen([args.tracer, "watch", str(target.pid), definition], cwd=root,
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        cleanup.callback(stop, trace)
        output = Output(trace.stdout, 20)
        assert json.loads(output.expect(r'^\{"watchId".*$')[0])["active"]
        (root / "write.now").touch()
        app.expect(r"^DONE$")
        trace.send_signal(signal.SIGINT)
        output.expect(r'^\{"watchId".*"diagnostic":"original breakpoint bytes restored or target exited"')
        assert trace.wait(timeout=15) == 0
        subprocess.check_output([args.tracer, "preview", str(target.pid), definition], cwd=root)
        rows = [json.loads(line) for line in output.seen]
        events = [row["objectObservation"] for row in rows if "objectObservation" in row]
        assert len(events) == 7, events
        kernel = [item for item in events if item["locationKind"] == "SYSCALL_OUTPUT"]
        assert len(kernel) == 3 and all(item["module"] and item["pc"] for item in kernel), events
        assert len({item["destination"] for item in kernel}) == 1
        written = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        assert [item["kind"] for item in kernel] == [written, written, "COVERAGE_GAP"]
        assert "partial" in kernel[-1]["diagnostic"] and "oldInteger" not in kernel[-1]
        assert "newInteger" not in kernel[-1]
        store = next(item for item in events if item["locationKind"] == "STORE")
        assert store["kind"] == written and store["function"] == "userspace_write"
        if no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in events)
        else:
            assert [(item["oldInteger"], item["newInteger"]) for item in kernel[:2]] == [(17, 41), (41, 53)]
            assert (store["oldInteger"], store["newInteger"]) == (7, 23)
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"values={not no_values} read, pread and short read distinguished from an application store")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("tracer", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    digest = hashlib.sha256(args.fixture.read_bytes()).digest()
    for no_values in (False, True):
        run(args, no_values)
    assert hashlib.sha256(args.fixture.read_bytes()).digest() == digest

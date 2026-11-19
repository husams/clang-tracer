"""Resolve template records with scoped arguments and capture their scalar writes."""

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
from run import Output, send, stop


CASES = (
    ("app::Plain", 0),
    ("app::Item<int, 3>", 3),
    ("app::Item<app::Foo, 3>", 3),
    ("app::Item<app::Item<app::Foo, 3>, 4>", 4),
    ("app::Item<app::Types::Foo, 3>", 3),
    ("Item<app::Foo, 3>", 3),
)


def run(args, concrete, increment):
    with tempfile.TemporaryDirectory(prefix="ct-object-template-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = subprocess.Popen([args.fixture], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True, cwd=root)
        cleanup.callback(stop, target)
        app = Output(target.stdout, 20)
        app.expect(r"^READY$")
        definition = root / "definition.json"
        # First-seen isolates record lookup from constructor/destructor admission:
        # the issue's aggregate templates need not have out-of-line lifetimes.
        request = {"concreteType": concrete, "field": "state_",
                   "codeSymbol": concrete + "::set(int)", "firstSeen": True,
                   "synchronization": "THREAD_CONFINED"}
        definition.write_text(json.dumps(request))
        preview = subprocess.run([args.tracer, "preview", str(target.pid), definition],
                                 text=True, capture_output=True, timeout=20)
        assert preview.returncode == 0, (concrete, preview.stderr)
        plan = json.loads(preview.stdout)
        assert plan["fieldEncoding"] == "signed int32", plan
        assert int(plan.get("fieldOffset", 0)) == 0, plan
        assert any(site["role"] == "store" for site in plan["bindings"]), plan
        # A similar spelling from a different namespace must still be refused.
        request["concreteType"] = "missing::" + concrete
        definition.write_text(json.dumps(request))
        rejected = subprocess.run([args.tracer, "preview", str(target.pid), definition],
                                  text=True, capture_output=True, timeout=20)
        assert rejected.returncode != 0 and "no complete DWARF record" in rejected.stderr, rejected
        request["concreteType"] = concrete
        definition.write_text(json.dumps(request))
        watch = subprocess.Popen([args.tracer, "watch", str(target.pid), definition], cwd=root,
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, watch)
        output = Output(watch.stdout, 20)
        assert json.loads(output.expect(r'^\{"watchId".*$')[0])["active"]
        for _ in range(2):
            send(target, "g")
            app.expect(r"^WROTE$")
        watch.send_signal(signal.SIGINT)
        stopped = json.loads(output.expect(r'^\{"watchId".*"diagnostic":.*$')[0])
        assert not stopped.get("active") and not stopped.get("restorationPending"), stopped
        assert int(stopped.get("dropped", 0)) == 0, stopped
        assert watch.wait(timeout=15) == 0
        events = [json.loads(line)["objectObservation"] for line in output.seen
                  if "objectObservation" in json.loads(line)]
        assert not any(event["kind"] == "COVERAGE_GAP" for event in events), events
        changes = [event for event in events if event["kind"] == "FIELD_CHANGED"]
        assert [(event["oldInteger"], event["newInteger"]) for event in changes] == [
            (0, 20 + increment), (20 + increment, 39 + increment)], changes
        # Restoring the watch must leave the application usable.
        send(target, "g")
        app.expect(r"^WROTE$")
        target.stdin.close()
        assert target.wait(timeout=10) == 0
    print(f"{concrete}: preview, namespace rejection, writes and restoration passed", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("tracer", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    digest = hashlib.sha256(args.fixture.read_bytes()).digest()
    for concrete, increment in CASES:
        run(args, concrete, increment)
    assert hashlib.sha256(args.fixture.read_bytes()).digest() == digest

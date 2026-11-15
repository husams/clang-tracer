"""Check a two-hop pointee watch through rebinding, reuse and null storage."""

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
from run import Output, VALUE_FIELDS, read_redirects, send, stop, verify_restored_redirects


def run(repl, server, agent, fixture, backend, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-pointee-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 35)

        target, app = start([fixture])
        app.expect(r"^READY$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def read(address, count):
            return os.pread(memory.fileno(), count, address)

        control, out = start([repl, "--server", server, "--agent", agent, "--fail-fast"])
        definition = ('pointee_test::Owner --field state.quantity --pointee "routes.head->node" '
                      "--code _ZN12pointee_test5Owner5writeERNS_4LinkE "
                      "--mutation _ZN12pointee_test5Owner6selectEPNS_4LinkE "
                      f"--backend {backend} --synchronization thread-confined")
        if no_values:
            definition += " --no-values"
        send(control, f'%object prepare {definition} --module "{fixture}" --output watch')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(control, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        start_options = " --rendezvous arm_rendezvous" if (
            backend == "code-cache" and platform.machine() == "aarch64") else ""
        send(control, "%object start --plan watch" + start_options)
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(control, out, watch, backend, target.pid, (), live_read=read)
        send(control, f"%record trace.db --sqlite --probe {watch[1]} --count 12 --timeout 20")
        (root / "write.now").touch()
        done = app.expect(r"^DONE first=(0x[0-9a-f]+):99 second=(0x[0-9a-f]+):23$")
        first, second = (int(address, 16) for address in done.groups())
        assert first != second
        out.expect(r"^received=12 .*complete=true")
        send(control, f"%object stop {watch[1]}")
        stopped = out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        assert "coverage=partial" in stopped.string
        verify_restored_redirects(control, out, None, target.pid, redirects, live_read=read)
        send(control, "%detach")
        out.expect(r"^detached session=")
        send(control, "%events open trace.db --as saved")
        out.expect(r"^opened events=12 retained=12 evicted=0$")
        send(control, "%events export saved --format json --output trace.json")
        send(control, "%quit")
        assert control.wait(timeout=10) == 0, "".join(out.seen[-12:])
        events = [event["objectObservation"] for event in json.loads((root / "trace.json").read_text())]
        written = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        assert [item["kind"] for item in events] == [
            "FIRST_SEEN", "RANGE_BOUND", written, "RANGE_SUSPENDED", "RANGE_BOUND", written,
            "RANGE_SUSPENDED", "RANGE_BOUND", written, "RANGE_SUSPENDED", "COVERAGE_GAP",
            "OBSERVATION_ENDED"], events
        for index, generation, destination in ((1, 1, first), (4, 2, second), (7, 3, second)):
            for item in events[index:index + 3]:
                assert int(item["destination"]) == destination, item
                assert int(item["storageGeneration"]) == generation, item
            field = events[index + 1]
            assert field["field"] == "pointee_test::Owner::routes.head->node->state.quantity", field
            assert field["line"] > 0 and field["locationKind"] == "STORE", field
        assert "pointee path is null" in events[10]["diagnostic"]
        if no_values:
            assert all(not VALUE_FIELDS.intersection(item) for item in events)
        else:
            assert events[1]["newInteger"] == 10 and "oldInteger" not in events[1]
            assert [(events[index]["oldInteger"], events[index]["newInteger"]) for index in (4, 7)] == [
                (11, 20), (21, 21)]
            assert [(item["oldInteger"], item["newInteger"]) for item in events if item["kind"] == written] == [
                (10, 11), (20, 21), (21, 22)]
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        print(f"backend={backend} values={not no_values} verified pointer generations and exact restoration")


if __name__ == "__main__":
    repl, server, agent, fixture = (Path(arg).resolve() for arg in sys.argv[1:])
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    for backend in ("ptrace", "code-cache"):
        for no_values in (False, True):
            run(repl, server, agent, fixture, backend, no_values)
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest

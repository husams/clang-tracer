"""Verify generation changes at a collection mutation inlined into its writer."""

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
from run import Output, read_redirects, send, stop, verify_restored_redirects


def run(repl_path, server, agent, fixture, backend, no_values, alternate, continuations):
    with tempfile.TemporaryDirectory(prefix="ct-inline-collection-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([fixture, "alternate"] if alternate else [fixture])
        app.expect(r"^READY$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def read(address, count):
            return os.pread(memory.fileno(), count, address)

        repl, out = start([repl_path, "--server", server, "--agent", agent, "--fail-fast"])
        definition = ("arm_test::Portfolio --field quantity --vector legs --element-key id=12 "
                      "--code _ZN8arm_test9Portfolio9rebalanceEv --mutation inline_reserve "
                      f"--synchronization thread-confined --backend {backend}")
        if no_values:
            definition += " --no-values"
        send(repl, f'%object prepare {definition} --module "{fixture}" --output watch')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(repl, "%object preview --plan watch")
        preview = out.expect(r"^watch=0 active=no stores=1 .*|^error:")
        assert preview.string.startswith("watch="), preview.string
        anchors = []
        roles = []
        for _ in range(continuations + 1):
            row = out.expect(r'^site role="inline mutation (entry|continuation)" elf=0x[0-9a-f]+ '
                             r'pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
            roles.append(row[1])
            anchors.append((int(row[2], 16), bytes.fromhex(row[3])))
        assert roles.count("entry") == 1 and roles.count("continuation") == continuations
        options = " --rendezvous arm_rendezvous" if backend == "code-cache" and platform.machine() == "aarch64" else ""
        send(repl, "%object start --plan watch" + options)
        watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, backend, target.pid, (), live_read=read)
        send(repl, f"%record inline.db --sqlite --probe {watch[1]} --count 6 --timeout 20")
        (root / "write.now").touch()
        done = app.expect(r"^DONE previous=(0x[0-9a-f]+) current=(0x[0-9a-f]+) value=75$")
        assert done[1] != done[2]
        out.expect(r"^received=6 .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        verify_restored_redirects(repl, out, None, target.pid, redirects, live_read=read)
        assert all(read(pc, len(code)) == code for pc, code in anchors)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open inline.db --as saved")
        out.expect(r"^opened events=6 retained=6 evicted=0$")
        send(repl, "%events export saved --format json --output inline.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        events = [row["objectObservation"] for row in json.loads((root / "inline.json").read_text())]
        assert [item["kind"] for item in events] == [
            "FIRST_SEEN", "RANGE_BOUND", "RANGE_SUSPENDED", "RANGE_BOUND",
            "FIELD_WRITTEN" if no_values else "FIELD_CHANGED", "OBSERVATION_ENDED"], events
        assert [item["storageGeneration"] for item in events[1:5]] == ["1", "1", "2", "2"], events
        assert [int(item["destination"]) for item in events[1:5]] == [int(done[1], 16)] * 2 + [int(done[2], 16)] * 2
        assert len({item["lifetimeId"] for item in events}) == 1, events
        assert events[4]["locationKind"] == "STORE" and events[4]["line"] > 0, events[4]
        if no_values:
            assert "oldInteger" not in events[4] and "newInteger" not in events[4], events[4]
        else:
            assert events[1]["newInteger"] == 100 and "oldInteger" not in events[1], events[1]
            assert (events[3]["oldInteger"], events[3]["newInteger"]) == (100, 100), events[3]
            assert (events[4]["oldInteger"], events[4]["newInteger"]) == (100, 75), events[4]
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        print(f"backend={backend} values={not no_values} alternate={alternate} "
              "verified inline rebind and exact restoration")


if __name__ == "__main__":
    repl, server, agent, fixture = (Path(arg).resolve() for arg in sys.argv[1:5])
    assert sys.argv[5:] in ([], ["--disjoint"], ["--multiple-exits"])
    continuations = 2 if "--multiple-exits" in sys.argv else 1
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    for backend in ("ptrace", "code-cache"):
        for no_values in (False, True):
            for alternate in ((False, True) if len(sys.argv) == 6 else (False,)):
                run(repl, server, agent, fixture, backend, no_values, alternate, continuations)
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest

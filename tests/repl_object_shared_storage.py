"""Join independently recorded writes by their shared file offset."""

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


def run(repl, server, agent, fixture, no_values, caller_lock):
    with tempfile.TemporaryDirectory(prefix="ct-shared-storage-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        backing = root / "shared.bin"
        with backing.open("wb") as file:
            file.truncate(2 * os.sysconf("SC_PAGESIZE"))
        identity = backing.stat()

        def start(argv, cwd):
            process = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 40)

        watches = []
        for index in range(2):
            cwd = root / str(index)
            cwd.mkdir()
            target, app = start([fixture, backing], cwd)
            app.expect(r"^READY$")
            memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

            def read(address, count, fd=memory.fileno()):
                return os.pread(fd, count, address)

            control, out = start([repl, "--server", server, "--agent", agent,
                                  "--attach", str(target.pid), "--fail-fast"], cwd)
            out.expect(rf"^attached session=\S+ pid={target.pid}$")
            definition = ("--type shared_test::Counter --field value --code increment_shared "
                          f'--backend code-cache --shared-file "{backing}"')
            if platform.machine() == "aarch64":
                definition += " --rendezvous arm_rendezvous"
            if caller_lock:
                definition += " --synchronization caller-lock"
            if no_values:
                definition += " --no-values"
            send(control, "%object start " + definition)
            watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
            assert watch[1] and "coverage=partial" in watch.string, watch.string
            redirects = read_redirects(control, out, watch, "code-cache", target.pid, (), live_read=read)
            (cwd / "write.now").touch()
            setup = app.expect(r"^SETUP address=(0x[0-9a-f]+) offset=(\d+)$")
            send(control, f"%record trace.db --sqlite --probe {watch[1]} --count 65 --timeout 25")
            watches.append((target, app, control, out, watch[1], redirects, read, cwd,
                            int(setup[1], 16), int(setup[2])))

        (root / "run.now").touch()
        old_values, addresses, app_total = [], set(), 0
        for target, app, control, out, watch, redirects, read, cwd, address, offset in watches:
            app_total += int(app.expect(r"^DONE sum=(\d+)$")[1])
            addresses.add(address)
            out.expect(r"^received=65 .*complete=true")
            send(control, f"%object stop {watch}")
            status = out.expect(rf"^watch={watch} active=no .*restoration_pending=no .*")
            # Stop adds one observation ending the live lifetime.
            assert "observations=66 " in status.string and "executed_stores=66 " in status.string, status.string
            assert "dropped=0 " in status.string, status.string
            assert "coverage=partial" in status.string, status.string
            verify_restored_redirects(control, out, None, target.pid, redirects, live_read=read)
            send(control, "%detach")
            out.expect(r"^detached session=")
            send(control, "%events open trace.db --as saved")
            out.expect(r"^opened events=65 retained=65 evicted=0$")
            send(control, "%events export saved --format json --output trace.json")
            send(control, "%quit")
            assert control.wait(timeout=10) == 0, "".join(out.seen[-12:])
            events = json.loads((cwd / "trace.json").read_text())
            for sequence, event in enumerate(events, 1):
                assert event["pid"] == target.pid and int(event["probeSequence"]) == sequence, event
                assert int(event["objectAddress"]) == address, event
                assert not int(event.get("agentDroppedEvents", 0)), event
                item = event["objectObservation"]
                assert int(item["lifetimeId"]) == int(item["generation"]) == 1, item
                storage = item["sharedStorage"]
                assert tuple(int(storage[field]) for field in ("device", "inode", "fileOffset")) == (
                    identity.st_dev, identity.st_ino, offset), item
                assert item["externalWriteOrderUnknown"] is True, item
                if caller_lock:
                    assert item["synchronization"] == "CALLER_LOCK", item
                if sequence == 1:
                    assert item["kind"] == "CREATED" and not VALUE_FIELDS.intersection(item), item
                    continue
                assert int(item["destination"]) == address and item["line"] > 0, item
                assert item["kind"] == ("FIELD_WRITTEN" if no_values else "FIELD_CHANGED"), item
                if no_values:
                    assert not VALUE_FIELDS.intersection(item), item
                else:
                    assert item["newInteger"] == item["oldInteger"] + 1, item
                    old_values.append(item["oldInteger"])

        assert len(addresses) == 2
        if not no_values:
            assert sorted(old_values) == list(range(128)), old_values
        assert app_total == sum(range(128))
        with backing.open("rb") as file:
            file.seek(offset)
            assert int.from_bytes(file.read(4), sys.byteorder) == 128
        (root / "stop.now").touch()
        for target, *_ in watches:
            assert target.wait(timeout=10) == 0
        print(f"values={not no_values} caller_lock={caller_lock} shared writes=128 file_offset={offset} "
              "verified two recording identities and exact restoration", flush=True)


if __name__ == "__main__":
    repl, server, agent, fixture = (Path(arg).resolve() for arg in sys.argv[1:5])
    caller_lock = sys.argv[5:] == ["--caller-lock"]
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    for no_values in (False, True):
        run(repl, server, agent, fixture, no_values, caller_lock)
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest

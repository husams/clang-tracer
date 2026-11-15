"""Drive the optimized, prebuilt Shape/Image ELF through the native REPL."""

from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop

WATCH = (
    "%object start --type demo::Circle --base demo::Shape --owner demo::Image "
    "--field radius --collection _shapes --snapshot-field _description "
    "--code borrowed_write"
)


def executable_bytes(process, fixture):
    with open(f"/proc/{process.pid}/mem", "rb", buffering=0) as memory:
        spans = []
        for row in Path(f"/proc/{process.pid}/maps").read_text().splitlines():
            bounds, perms, _, _, _, *name = row.split(maxsplit=5)
            if "x" in perms and name == [str(fixture)]:
                start, end = (int(item, 16) for item in bounds.split("-"))
                spans.append(os.pread(memory.fileno(), end - start, start))
        assert spans
        return b"".join(spans)


def command(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()


def verify_record(path, fault, stack_depth, source, no_values):
    document = json.loads(path.read_text())
    events = document if isinstance(document, list) else document["events"]
    observations = [event["objectObservation"] for event in events]
    kind = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
    expected = [
        "CREATED", "OWNER_ADDED", "BORROW_RETURN",
        *([] if no_values else ["SNAPSHOT_DELTA"]),
        *(["COVERAGE_GAP"] if fault else []),
        kind, "DESTROY_BEGIN", "TEARDOWN_END", "OWNER_REMOVED",
        "CREATED", "OWNER_ADDED", "BORROW_RETURN", kind,
        "DESTROY_BEGIN", "TEARDOWN_END", "OWNER_REMOVED",
    ]
    assert [item["kind"] for item in observations] == expected, observations
    addresses = set()
    lifetime = 0
    for event, item in zip(events, observations):
        if item["kind"] == "COVERAGE_GAP":
            assert "signal 11" in item["diagnostic"], item
            continue
        addresses.add(int(event["objectAddress"]))
        if item["kind"] == "CREATED":
            lifetime += 1
        assert item["concreteType"] == "demo::Circle"
        assert int(item["lifetimeId"]) == int(item["generation"]) == lifetime
        if item["kind"] in {"OWNER_ADDED", "BORROW_RETURN", "OWNER_REMOVED"}:
            assert int(item["ownerAddress"]) != 0 and int(item["ownerIndex"]) == 0
    assert len(addresses) == 1, addresses
    [address] = addresses
    writes = [item for item in observations if item["kind"] == kind]
    assert [int(item["lifetimeId"]) for item in writes] == [1, 2]
    if no_values:
        value_fields = {"oldInteger", "newInteger", "oldString", "newString"}
        assert all(not value_fields.intersection(item) for item in observations), observations
    else:
        assert [(item["oldInteger"], item["newInteger"]) for item in writes] == [(10, 15), (20, 25)]
    assert all(item["field"] == "demo::Circle::radius" for item in writes)
    assert all(int(item["destination"]) > address for item in writes)
    assert writes[0]["destination"] == writes[1]["destination"]
    assert all(int(item["pc"]) > int(item["elfPc"]) for item in writes), writes
    writer_line = next(i for i, line in enumerate(source.read_text().splitlines(), 1)
                       if "WRITER_LINE" in line)
    assert all(Path(item["file"]).name == source.name and item["line"] == writer_line
               for item in writes), writes
    if not no_values:
        [snapshot] = [item for item in observations if item["kind"] == "SNAPSHOT_DELTA"]
        assert snapshot["field"] == "demo::Shape::_description"
        assert (snapshot["oldString"], snapshot["newString"]) == ("ZHJhZnQ=", "c2VsZWN0ZWQ=")
    if fault:
        [gap] = [item for item in observations if item["kind"] == "COVERAGE_GAP"]
        assert gap["pc"] == writes[0]["pc"]
    for event in events:
        item = event["objectObservation"]
        if not stack_depth:
            assert "stack" not in event
        elif item["kind"] == kind:
            assert event["stack"]["frames"][0]["pc"] == item["pc"]
            assert event["stack"]["frames"][0]["line"] == writer_line


def main(repl, server, agent, fixture, source):
    fixture, source = Path(fixture).resolve(), Path(source).resolve()
    original_file = hashlib.sha256(fixture.read_bytes()).digest()
    with tempfile.TemporaryDirectory(prefix="ct-object-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, target)
        application = Transcript(target.stdout)
        application.expect(r"^READY$")
        original_code = executable_bytes(target, fixture)
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(target.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        cleanup.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={target.pid}$")

        def send(line):
            command(console, line)

        send(WATCH.replace("borrowed_write", "nonexistent_writer"))
        output.expect(r"missing or ambiguous object watch ELF symbol: nonexistent_writer")
        assert executable_bytes(target, fixture) == original_code

        for name, fault, depth, no_values in (
            ("normal", False, 0, False), ("fault", True, 5, False),
            ("fault_no_values", True, 5, True),
        ):
            count = 15 + int(fault) - int(no_values)
            db = root / f"{name}.db"
            exported = root / f"{name}.json"
            option = " --no-values" if no_values else ""
            send(f"{WATCH} --stack-depth {depth}{option}")
            started = output.expect(r"^watch=(\d+) active=yes stores=[1-9]\d* .*partial coverage|^error:")
            assert started[1], "".join(output.seen)
            watch = started[1]
            assert executable_bytes(target, fixture) != original_code
            send(f"%record {db} --sqlite --name object.trace --probe {watch} --count {count} --timeout 12")
            command(target, "f" if fault else "g")
            application.expect(r"^destroy .* radius=15$")
            application.expect(r"^destroy .* radius=25$")
            application.expect(rf"^DONE fault={int(fault)}$")
            send(f"%object status {watch}")
            output.expect(rf"^watch={watch} active=yes .* observations={count} ")
            send(f"%object stop {watch}")
            output.expect(rf"^watch={watch} active=no .* restoration_pending=no ")
            assert executable_bytes(target, fixture) == original_code
            send(f"%events open {db} --as {name}")
            send(f"%events list {name} --limit {count}")
            output.expect(r" FIELD_WRITTEN .*generation=2 .*::radius destination="
                          if no_values else r" FIELD_CHANGED .*generation=2 .*20 -> 25")
            send(f"%events export {name} --format json --output {exported}")
            send(f"%object status {watch}")
            output.expect(rf"^watch={watch} active=no ")
            verify_record(exported, fault, depth, source, no_values)

        send(WATCH)
        output.expect(r"^watch=\d+ active=yes ")
        send("%detach")
        output.expect(r"^detached session=")
        assert executable_bytes(target, fixture) == original_code

        send(f"%attach {target.pid}")
        output.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(WATCH)
        watch = output.expect(r"^watch=(\d+) active=yes ")[1]
        command(target, "t")
        application.expect(r"^THREAD DONE$")
        send(f"%object status {watch}")
        output.expect(rf"^watch={watch} active=yes .* restoration_pending=no ")
        send(f"%object stop {watch}")
        output.expect(rf"^watch={watch} active=no .* restoration_pending=no ")
        assert executable_bytes(target, fixture) == original_code

        command(target, "g")
        application.expect(r"^DONE fault=0$")
        command(target, "q")
        assert target.wait(timeout=5) == 0
        send("%quit")
        # The deliberate missing selector sets the batch shell's error status.
        assert console.wait(timeout=15) == 1, "".join(output.seen)
    assert hashlib.sha256(fixture.read_bytes()).digest() == original_file
    print("Shape/Image verified: two generations, two committed writes, snapshot, fault/retry, stop, detach, clone, SQLite")


if __name__ == "__main__":
    main(*sys.argv[1:])

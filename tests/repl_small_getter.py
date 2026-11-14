"""Verify entry, direct call relay and entry trap on prebuilt native code."""

from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def send(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()


def direct_call(body, caller, callee):
    sites = [
        caller + i
        for i in range(len(body) - 4)
        if body[i] == 0xE8
        and caller + i + 5 + int.from_bytes(body[i+1:i+5], "little", signed=True) == callee
    ]
    [site] = sites
    return site


def assert_no_relay(pid, call):
    page = os.sysconf("SC_PAGE_SIZE")
    minimum = int(Path("/proc/sys/vm/mmap_min_addr").read_text())
    first = max(minimum, call + 5 - (1 << 31) - page)
    last = call + 5 + (1 << 31) - 1
    remaining = (first + page - 1) // page * page
    for row in Path(f"/proc/{pid}/maps").read_text().splitlines():
        begin, end = (int(n, 16) for n in row.split()[0].split("-"))
        if end <= remaining:
            continue
        assert begin <= remaining, (hex(begin), hex(remaining), row)
        remaining = end
        if remaining > last:
            return
    raise AssertionError("the fixture left a reachable page available")


def run_case(repl, server, agent, fixture, root, backend):
    blocked = backend == "trap"
    with ExitStack() as cleanup:
        target = subprocess.Popen([fixture, "blocked" if blocked else "open"],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, target)
        app = Transcript(target.stdout)
        line = app.expect(
            r"^READY entry=(0x\w+) caller=(0x\w+) alternate=(0x\w+) roomy=(0x\w+)$"
        )
        entry, caller, alternate, roomy = (int(value, 16) for value in line.groups())
        memory = cleanup.enter_context(open(f"/proc/{target.pid}/mem", "rb", buffering=0))
        def read(address, count):
            return os.pread(memory.fileno(), count, address)
        getter = read(entry, 4)
        assert getter == bytes.fromhex("8b 47 50 c3"), getter.hex()
        call = direct_call(read(caller, 24), caller, entry)
        other_call = direct_call(read(alternate, 24), alternate, entry)
        call_before, other_before = read(call, 5), read(other_call, 5)
        roomy_before = read(roomy, 20)
        if blocked:
            assert_no_relay(target.pid, call)
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(target.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        cleanup.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={target.pid}$")
        if backend == "entry":
            send(console, '%probe inject "demo::Circle::get_branchy() const" --backend entry')
            entry_error = output.expect(r"^error: .+")[0]
            send(console, '%probe inject "demo::Circle::get_branchy() const" --backend auto')
            assert output.expect(r"^error: .+")[0] == entry_error
            assert "unsupported or PC-relative instruction" in entry_error, entry_error
        name = "get_roomy" if backend == "entry" else "get_radius"
        if blocked:
            assert_no_relay(target.pid, call)
        send(console, f'%probe inject "demo::Circle::{name}() const" --backend auto '
                      '--caller "demo::call_radius(const demo::Circle &)" --stack-depth 6')
        installed = output.expect(
            r"^installed probe=(\d+).* backend=(\S+) reason=(\S+) entry=(0x\w+) "
            r"patch=(0x\w+)(?: relay=(0x\w+))?$|^error:"
        )
        assert installed[1] and installed[2] == backend, "".join(output.seen)
        probe, _, reason, selected, patch, relay = installed.groups()
        selected, patch = int(selected, 16), int(patch, 16)
        during_getter, during_call, during_roomy = read(entry, 4), read(call, 5), read(roomy, 20)
        if backend == "trap":
            assert reason == "no_reachable_relay" and selected == patch == entry and not relay
            assert during_getter == b"\xcc" + getter[1:] and during_call == call_before
        elif backend == "call-site":
            assert reason == "entry_jump_unavailable" and selected == entry and patch == call
            assert during_getter == getter and during_call[0] == 0xE8 and during_call != call_before
            displacement = int.from_bytes(during_call[1:], "little", signed=True)
            assert call + 5 + displacement == int(relay, 16)
            assert read(int(relay, 16), 6) == bytes.fromhex("ff 25 00 00 00 00")
        else:
            assert reason == "verified_entry_window" and selected == patch == roomy and not relay
            assert during_roomy != roomy_before and during_getter == getter and during_call == call_before
        assert read(other_call, 5) == other_before

        db = root / f"{backend}.db"
        capture = root / f"{backend}.json"
        count = 8 if blocked else 4
        send(console, f"%record {db} --sqlite --probe {probe} --phase POST --count {count} --timeout 12")
        send(target, "g")
        app.expect(r"^CALLS direct=17 alternate=8 getter=10 foreign_trap=1$")
        output.expect(rf"^received={count} .*complete=true")
        send(console, "%detach")
        output.expect(r"^detached session=")
        after, call_after, roomy_after = read(entry, 4), read(call, 5), read(roomy, 20)
        assert after == getter and call_after == call_before
        assert read(other_call, 5) == other_before and roomy_after == roomy_before

        send(console, f"%events open {db} --as saved")
        output.expect(rf"^opened events={count} retained={count}")
        send(console, f"%events export saved --format json --output {capture}")
        send(console, "%quit")
        assert console.wait(timeout=15) == int(backend == "entry"), "".join(output.seen)
        events = json.loads(capture.read_text())
        assert len(events) == count
        assert [int(event["result"]["signedValue"]) for event in events] == [10] * count
        if backend != "entry":
            return_pcs = [int(event["stack"]["frames"][1]["pc"]) for event in events]
            expected = [call + 5, other_call + 5] * 4 if blocked else [call + 5] * 4
            assert return_pcs == expected, return_pcs

        send(target, "g")
        app.expect(r"^CALLS direct=17 alternate=8 getter=10 foreign_trap=1$")
        send(target, "q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=5) == 0
        print(f"backend={backend} reason={reason} getter={getter.hex(' ')}"
              f" -> {during_getter.hex(' ')} -> {after.hex(' ')}"
              f" caller={call_before.hex(' ')} -> {during_call.hex(' ')}"
              f" -> {call_after.hex(' ')} post_events={count} target_exit=0")
        if backend == "entry":
            print(f"roomy={roomy_before.hex(' ')} -> {during_roomy.hex(' ')}"
                  f" -> {roomy_after.hex(' ')}")


def main(repl, server, agent, fixture):
    fixture = Path(fixture).resolve()
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    with tempfile.TemporaryDirectory(prefix="ct-small-") as directory:
        for backend in ("trap", "call-site", "entry"):
            run_case(repl, server, agent, fixture, Path(directory), backend)
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest


if __name__ == "__main__":
    main(*sys.argv[1:])

"""Discover, patch, record, and reread one direct store through the native REPL."""

from contextlib import ExitStack
import ctypes
import argparse
import errno
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import read_redirects, verify_restored_redirects

WRITER = '"demo::update(demo::Person &)"'
VALUE_FIELDS = {"oldInteger", "newInteger", "oldString", "newString"}


class IOVec(ctypes.Structure):
    _fields_ = [("base", ctypes.c_void_p), ("size", ctypes.c_size_t)]


def assert_field_unreadable(pid, address):
    # The watch uses this same kernel read. The CPU can still execute the
    # store against the write-only VMA, while process_vm_readv requires VM_READ.
    read = ctypes.CDLL(None, use_errno=True).process_vm_readv
    read.argtypes = (ctypes.c_int, ctypes.POINTER(IOVec), ctypes.c_ulong,
                     ctypes.POINTER(IOVec), ctypes.c_ulong, ctypes.c_ulong)
    read.restype = ctypes.c_ssize_t
    value = ctypes.c_int32()
    local = IOVec(ctypes.addressof(value), ctypes.sizeof(value))
    remote = IOVec(address, ctypes.sizeof(value))
    assert read(pid, ctypes.byref(local), 1, ctypes.byref(remote), 1, 0) == -1
    assert ctypes.get_errno() == errno.EFAULT


def run_case(repl, server, agent, fixture, source, no_values, backend, fault=None):
    source_line = next(i for i, line in enumerate(source.read_text().splitlines(), 1)
                       if "STORE_LINE" in line)
    with tempfile.TemporaryDirectory(prefix="ct-person-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        args = [fixture, fault] if fault else ([fixture, "write-only"] if no_values else [fixture])
        target = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, target)
        app = Transcript(target.stdout)
        address = int(app.expect(r"^READY address=(0x[0-9a-f]+)$")[1], 16)
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(target.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        cleanup.callback(stop, console)
        out = Transcript(console.stdout)

        def send(line):
            console.stdin.write(line + "\n")
            console.stdin.flush()

        def run(command):
            target.stdin.write(command + "\n")
            target.stdin.flush()

        def maps():
            send("%memory maps")
            token = out.expect(r"^memory mapping=(\S+)$")[1]
            out.expect(r"^memory mappings=[1-9][0-9]*$")
            return token

        def read_code(pc, token, count):
            send(f"%memory read {pc:#x} --mapping {token} --count {count}")
            row = out.expect(rf"^{pc:#x}  ((?:[0-9a-f]{{2}} )+) +\|")
            return bytes.fromhex(row[1])

        initial_session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        before_token = maps()
        send(f"%probe entry {WRITER} --mapping {before_token}")
        out.expect(r'^entry=0x[0-9a-f]+ .*boundary=DWARF ')
        site = out.expect(
            r"^(0x[0-9a-f]+)  ((?:[0-9a-f]{2} )+) +movl\b.*\(%rdi\).* next=(0x[0-9a-f]+)$"
        )
        pc, original, successor = int(site[1], 16), bytes.fromhex(site[2]), int(site[3], 16)
        assert successor == pc + len(original)
        assert original == bytes.fromhex("c7 07 01 00 00 00"), original.hex(" ")
        out.expect(r"^patch_size=")
        assert read_code(pc, before_token, len(original)) == original

        mode = "off" if no_values else "on"
        option = " --no-values" if no_values else ""
        send(f"%object start --type demo::Person --field age --code {WRITER} "
             f"--backend {backend} --stack-depth 4{option}")
        started = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
        assert started[1], "".join(out.seen)
        watch = started[1]
        assert f"values={mode}" in started.string
        redirects = read_redirects(console, out, started, backend, target.pid, ())
        patched = read_code(pc, maps(), len(original))
        assert patched == (original if redirects else b"\xcc" + original[1:])

        db, replay = root / "age.db", root / "age.json"
        write_count = 2 if no_values and fault != "fault-skip" else 1
        count = 1 + write_count + bool(fault)
        send(f"%record {db} --sqlite --name object.trace --probe {watch} --count {count} --timeout 12")
        if no_values and not fault:
            assert_field_unreadable(target.pid, address)
        run("g")
        if fault:
            out_age = 42 if fault == "fault-skip" else 1
            app.expect(rf"^FAULT pc={pc:#x} count=1 first_age={out_age}$")
        app.expect(r"^STORES_DONE$")
        if no_values and not fault:
            assert_field_unreadable(target.pid, address)
        out.expect(rf"^received={count} .*complete=true")
        send(f"%object status {watch}")
        status = out.expect(rf"^watch={watch} active=yes .*observations={count} restoration_pending=no values={mode}")
        if redirects:
            stores = 1 if fault == "fault-skip" else 2
            assert f"executed_stores={stores} dropped=0 " in status.string
        send("%detach")
        out.expect(rf"^detached session={re.escape(initial_session)}$")

        send(f"%attach {target.pid}")
        next_session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        assert next_session != initial_session
        restored_token = maps()
        restored = read_code(pc, restored_token, len(original))
        assert restored == original
        verify_restored_redirects(console, out, restored_token, target.pid, redirects)
        run("r")
        app.expect(rf"^age=1 address={address:#x}$")
        restored_token = maps()
        send(f"%memory read {address:#x} --mapping {restored_token} --type i32")
        out.expect(rf"^{address:#x} i32=1$")

        send(f"%events open {db} --as ages")
        out.expect(rf"^opened events={count} retained={count}")
        send(f"%events show ages --index {2 if fault else 1}")
        kind = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        values = "" if no_values else " 42 -> 1"
        event_line = out.expect(
            rf" {kind} demo::Person#1 generation=1 address={address:#x} "
            rf"demo::Person::age{values} destination={address:#x} pc={pc:#x} .*person\.cpp:{source_line}$"
        ).string.strip()
        send(f"%events export ages --format json --output {replay}")
        send("%detach")
        out.expect(rf"^detached session={re.escape(next_session)}$")

        created, *writes = json.loads(replay.read_text())
        first = created["objectObservation"]
        assert first["kind"] == "CREATED" and int(created["objectAddress"]) == address
        if fault:
            failed = writes.pop(0)["objectObservation"]
            assert failed["kind"] == "COVERAGE_GAP" and failed["locationKind"] == "STORE", failed
            assert (failed["lifetimeId"], failed["generation"]) == (first["lifetimeId"], first["generation"])
            assert int(failed["pc"]) == pc and int(failed["destination"]) == address
            assert "signal 11" in failed["diagnostic"] and not VALUE_FIELDS.intersection(failed)
        assert len(writes) == write_count
        if no_values:
            assert not VALUE_FIELDS.intersection(first), first
        else:
            assert first["newInteger"] == 42
        for write in writes:
            change = write["objectObservation"]
            assert change["kind"] == kind
            if no_values:
                assert not VALUE_FIELDS.intersection(change), change
            else:
                assert (change["oldInteger"], change["newInteger"]) == (42, 1)
            assert (first["lifetimeId"], first["generation"]) == (change["lifetimeId"], change["generation"])
            assert int(write["objectAddress"]) == int(change["destination"]) == address
            assert int(change["pc"]) == int(change["elfPc"]) == pc
            assert Path(change["file"]).name == source.name and change["line"] == source_line
            assert int(write["stack"]["frames"][0]["pc"]) == pc

        run("v")
        app.expect(r"^after_detach_age=1$")
        run("q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=5) == 0
        send("%quit")
        assert console.wait(timeout=15) == 0, "".join(out.seen)
        print(f"store={pc:#x} successor={successor:#x} bytes={original.hex(' ')}"
              f" -> {patched.hex(' ')} -> {restored.hex(' ')}")
        print(event_line)
        if no_values and not fault:
            print("values=off, write-only field returned EFAULT to process_vm_readv,"
                  " two FIELD_WRITTEN events including the same-value store")
        print("reattached to same PID, age=1, second update after detach=1, target_exit=0")
        if fault:
            print(f"{fault} backend={backend} values={mode} original_fault_pc={pc:#x} "
                  f"writes={write_count} fault_gaps=1")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture", "source"):
        parser.add_argument(name)
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="ptrace")
    parser.add_argument("--faults", action="store_true")
    args = parser.parse_args()
    fixture, source = Path(args.fixture).resolve(), Path(args.source).resolve()
    original_elf = hashlib.sha256(fixture.read_bytes()).digest()
    for no_values in (False, True):
        run_case(args.repl, args.server, args.agent, fixture, source, no_values, args.backend)
        if args.faults:
            for fault in ("fault-retry", "fault-skip"):
                run_case(args.repl, args.server, args.agent, fixture, source, no_values,
                         args.backend, fault)
    assert hashlib.sha256(fixture.read_bytes()).digest() == original_elf


if __name__ == "__main__":
    main()

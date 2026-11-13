"""Deliver SIGUSR1 at an exact instruction of an anonymous code-cache prologue."""

from contextlib import ExitStack
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, VALUE_FIELDS, read_redirects, send, stop, verify_restored_redirects

ARM = platform.machine() == "aarch64"
SCALABLE_PHASES = ("sve16", "sve64", "sme16")
libc = c.CDLL(None, use_errno=True)
libc.ptrace.argtypes = (c.c_int, c.c_int, c.c_void_p, c.c_void_p)
libc.ptrace.restype = c.c_long
libc.getauxval.argtypes = (c.c_ulong,)
libc.getauxval.restype = c.c_ulong


class Iovec(c.Structure):
    _fields_ = [("base", c.c_void_p), ("size", c.c_size_t)]


def ptrace(request, pid, address=0, data=0):
    c.set_errno(0)
    result = libc.ptrace(request, pid, address, data)
    if result == -1 and c.get_errno():
        raise OSError(c.get_errno(), f"ptrace {request:#x}")
    return result


def wait_stop(pid, expected):
    _, status = os.waitpid(pid, 0)
    assert os.WIFSTOPPED(status) and os.WSTOPSIG(status) == expected, (status, expected)


def signal_at(pid, address, start):
    ptrace(0x4206, pid)  # PTRACE_SEIZE
    ptrace(0x4207, pid)  # PTRACE_INTERRUPT
    wait_stop(pid, signal.SIGTRAP)
    aligned = address & ~7
    shift = 8 * (address - aligned)
    trap = 0xD4200000 if ARM else 0xCC
    mask = ((1 << (32 if ARM else 8)) - 1) << shift
    original = ptrace(1, pid, aligned) & ((1 << 64) - 1)
    ptrace(4, pid, aligned, (original & ~mask) | (trap << shift))
    start.touch()
    ptrace(7, pid)
    wait_stop(pid, signal.SIGTRAP)
    ptrace(4, pid, aligned, original)
    registers = (c.c_uint64 * (34 if ARM else 27))()
    iov = Iovec(c.addressof(registers), c.sizeof(registers))
    ptrace(0x4204, pid, 1, c.byref(iov))
    pc = 32 if ARM else 16
    assert registers[pc] == address + (0 if ARM else 1)
    registers[pc] = address
    ptrace(0x4205, pid, 1, c.byref(iov))
    assert libc.syscall(131 if ARM else 234, pid, pid, signal.SIGUSR1) == 0
    ptrace(7, pid)
    _, status = os.waitpid(pid, 0)
    if os.WIFSTOPPED(status) and os.WSTOPSIG(status) != signal.SIGUSR1:
        ptrace(0x4204, pid, 1, c.byref(iov))
        raise AssertionError(f"signal delivery stopped with {os.WSTOPSIG(status)} at "
                             f"{registers[pc]:#x} after trap {address:#x}")
    assert os.WIFSTOPPED(status), status
    ptrace(17, pid, 0, signal.SIGUSR1)  # PTRACE_DETACH


def hook_pc(redirects, read, phase, dispatch_pc=None):
    if phase == "dispatch":
        block = bytes.fromhex("e8 10 80 d2 01 00 00 d4" if ARM else "b8 0e 00 00 00 0f 05")
        entry = redirects[dispatch_pc][2]
        return entry + read(entry, 4096).index(block) + len(block)
    store = bytes.fromhex("010000b9" if ARM else "8937")
    patch = store if ARM else b"\x90" * 5
    [gate] = [entry for original, _, entry in redirects.values() if original == patch]
    if phase == "entry":
        return gate + (4 if ARM else 5)
    entry = read(gate, 128)
    if phase == "gate":
        acquired = bytes.fromhex("01 fe 02 c8" if ARM else "f0 4d 0f b1 13")
        return gate + entry.index(acquired) + len(acquired)
    if phase == "exit":
        released = bytes.fromhex("a2 ff ff 35" if ARM else "f0 49 ff 0b")
        return gate + entry.index(released) + (0 if ARM else len(released))
    if ARM:
        for offset in range(0, len(entry), 4):
            word = int.from_bytes(entry[offset:offset + 4], "little")
            if word >> 26 == 5:
                body = gate + offset + 4 * ((word & 0x1FFFFFF) - (word & 0x2000000))
                break
        code = read(body, 4096)
        for offset in range(0, len(code) - 8, 4):
            if code[offset:offset + 4] == store:
                instruction = int.from_bytes(code[offset + 4:offset + 8], "little")
                if instruction & 0xFFC003FF == 0xD10003FF:  # SUB SP, SP, #frame
                    if phase == "deferred" or phase in SCALABLE_PHASES:
                        block = bytes.fromhex("e8 10 80 d2 01 00 00 d4")  # rt_sigprocmask
                        return body + code.index(block, offset + 8) + len(block)
                    if phase == "recovered":
                        restore = bytes.fromhex("02 44 1b d5")  # MSR FPCR, X2
                        return body + code.index(restore, offset + 8) + len(restore)
                    return body + offset + 8
    else:
        branch = bytes.fromhex("3e ff 25 00 00 00 00")
        slot = entry.index(branch) + len(branch)
        body = int.from_bytes(entry[slot:slot + 8], "little")
        code = read(body, 4096)
        sub = bytes.fromhex("48 8d 64 24 80")  # LEA -128(%RSP), %RSP
        after = code.index(store + sub) + len(store + sub)
        if phase == "deferred":
            block = bytes.fromhex("b8 0e 00 00 00 0f 05")  # rt_sigprocmask syscall
            return body + code.index(block, after) + len(block)
        return body + after
    raise AssertionError("could not identify the original store and its successor hook")


def run(repl, server, agent, fixture, phase, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-signal-window-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        dispatch = phase in ("dispatch", "tail")
        after = phase not in ("entry", "gate", "dispatch", "tail")

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        position = "after" if after else "before"
        if dispatch or phase in SCALABLE_PHASES:
            position = phase
        elif phase == "exit" and not ARM:
            position = "return"
        target, app = start([fixture, position])
        app.expect(r"^READY$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def read(address, count):
            return os.pread(memory.fileno(), count, address)

        control, out = start([repl, "--server", server, "--agent", agent, "--fail-fast",
                              "--attach", str(target.pid)])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        options = " --rendezvous arm_rendezvous" if ARM else ""
        if dispatch:
            options += " --follow-calls"
        elif not ARM:
            options += " --writer-entry"
        if no_values:
            options += " --no-values"
        writer = {"dispatch": "signal_window_dispatch", "tail": "signal_window_tail", "sve16": "signal_window_sve",
                  "sve64": "signal_window_sve", "sme16": "signal_window_sme"}.get(phase, "signal_window_write")
        definition = (f"--type signal_test::Record --field value --code {writer} "
                      f"--backend code-cache --async-signal {int(signal.SIGUSR1)}" + options)
        dispatch_pc = None
        if dispatch:
            send(control, "%object preview " + definition)
            entry = out.expect(r'^site role="writer" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+)|^error:')
            assert entry[1], entry.string
            dispatch_pc = int(entry[1], 16) + 4  # One stack adjustment before CALL/BL.
        send(control, "%object start " + definition)
        watch = out.expect(rf"^watch=(\d+) active=yes stores={0 if dispatch else 1} .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(control, out, watch, "code-cache", target.pid, (), live_read=read,
                                   runtime_calls=dispatch)
        count = 6 if phase == "tail" else 2
        record = f"%record trace.db --sqlite --probe {watch[1]} --count {count} --timeout 20"
        if phase == "tail":
            # Record before warmup so replay distinguishes its write from
            # the write after the signal handler edits the argument.
            send(control, record)
            (root / "write.now").touch()
            app.expect(r"^WARMED$")
            # The tail gate is the runtime mapping with the original
            # destination and the saved-frame-relative target slot.
            mask_call = bytes.fromhex("e8 10 80 d2 01 00 00 d4" if ARM else "b8 0e 00 00 00 0f 05")
            tail_destination = bytes.fromhex("f0 93 41 f9" if ARM else
                                             "49 89 44 24 d8 49 89 54 24 f8")
            gates = []
            page_size = os.sysconf("SC_PAGE_SIZE")
            for line in Path(f"/proc/{target.pid}/maps").read_text().splitlines():
                address, permissions, _, _, inode, *name = line.split()
                if permissions != "r-xp" or inode != "0" or name:
                    continue
                begin, end = (int(value, 16) for value in address.split("-"))
                for page in range(begin, end, page_size):
                    code = read(page, min(end - page, page_size))
                    if mask_call in code and tail_destination in code:
                        gates.append(page + code.index(mask_call) + len(mask_call))
            assert len(gates) == 1, gates
            [interrupt] = gates
        else:
            interrupt = hook_pc(redirects, read, phase, dispatch_pc)
            send(control, record)
        signal_at(target.pid, interrupt, root / ("tail.now" if phase == "tail" else "write.now"))
        app.expect(rf"^DONE pc_ok=1 value={23 if after else 43} result=44$")
        out.expect(rf"^received={count} .*complete=true")
        send(control, f"%object stop {watch[1]}")
        status = out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        if phase != "after":
            assert "coverage_gaps=" not in status.string, status.string
        verify_restored_redirects(control, out, None, target.pid, redirects, live_read=read)
        send(control, "%detach")
        out.expect(r"^detached session=")
        send(control, "%events open trace.db --as saved")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(control, "%events export saved --format json --output trace.json")
        send(control, "%quit")
        assert control.wait(timeout=10) == 0
        events = json.loads((root / "trace.json").read_text())
        item = events[-1]["objectObservation"]
        if phase == "tail":
            observed = [event["objectObservation"] for event in events]
            write = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
            assert [entry["kind"] for entry in observed] == [
                "CREATED", write, "DESTROY_BEGIN", "TEARDOWN_END", "CREATED", write], observed
        if phase == "after":
            assert item["kind"] == "COVERAGE_GAP" and "after the application store" in item["diagnostic"]
            assert not VALUE_FIELDS.intersection(item)
        elif no_values:
            assert item["kind"] == "FIELD_WRITTEN" and not VALUE_FIELDS.intersection(item)
        else:
            assert item["kind"] == "FIELD_CHANGED" and (item["oldInteger"], item["newInteger"]) == (
                17, 23 if after else 43), item
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        print(f"phase={phase} values={not no_values} verified original PC and edited register")


if __name__ == "__main__":
    repl, server, agent, fixture = (Path(arg).resolve() for arg in sys.argv[1:5])
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    scalable_state = ARM and (libc.getauxval(16) & (1 << 22) or libc.getauxval(26) & (1 << 23))
    phases = ("entry", "gate", "after", "exit", "recovered" if scalable_state else "deferred")
    if not scalable_state or libc.getauxval(16) & (1 << 22):
        phases += ("dispatch", "tail")
    if ARM and libc.getauxval(16) & (1 << 22):
        phases += ("sve16",)
        if libc.getauxval(26) & (1 << 37):
            phases += ("sme16",)
    if len(sys.argv) > 5:
        phases = tuple(sys.argv[5:])
    for phase in phases:
        for no_values in (False, True):
            run(repl, server, agent, fixture, phase, no_values)
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest

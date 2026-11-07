"""Read local process memory through the native REPL against changing Linux VMAs."""

from contextlib import ExitStack
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def main(repl, server, target):
    with tempfile.TemporaryDirectory(prefix="ct-memory-") as directory, ExitStack() as stack:
        backing = Path(directory) / "backing file"
        backing.write_bytes(b"B" * os.sysconf("SC_PAGE_SIZE"))
        child = subprocess.Popen([target, backing], stdout=subprocess.PIPE, text=True)
        stack.callback(stop, child)
        target_out = Transcript(child.stdout)
        start = target_out.expect(r"^memory base=(0x[0-9a-f]+) page=(\d+)$")
        base, page = int(start[1], 16), int(start[2])
        console = subprocess.Popen(
            [repl, "--server", server, "--attach", str(child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        stack.callback(stop, console)
        out = Transcript(console.stdout)
        out.expect(rf"^attached session=\S+ pid={child.pid}$")

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def change(message):
            child.send_signal(signal.SIGUSR1)
            target_out.expect(rf"^{message}$")

        def maps():
            send("%memory maps")
            token = out.expect(r"^memory mapping=(\S+:mem:[0-9a-f]{64})$")[1]
            out.expect(r"^memory mappings=[1-9][0-9]*$")
            return token

        def read(address, token, display, expected, *, error=False):
            start = len(out.seen)
            send(f"%memory read {address:#x} --mapping {token} {display}")
            out.expect(expected)
            if error:
                assert all(not line.startswith("0x") for line in out.seen[start:]), out.seen[start:]

        change("hole ready")
        token = maps()
        assert any(line.startswith(f"{base + page:#x}-{base + 2 * page:#x} r--p ")
                   for line in out.seen), out.seen
        assert any(line.startswith(f"{base + 2 * page:#x}-{base + 3 * page:#x} ---p ")
                   for line in out.seen), out.seen
        assert any(str(backing) in line for line in out.seen), out.seen

        across = base + page - 4
        read(across, token, "--count 8", rf"^{across:#x}  12 34 56 78 9a bc de f0 ")
        bits = bytes.fromhex("12 34 56 78 9a bc de f0")
        for scalar, signed in (("u64", False), ("i64", True)):
            value = int.from_bytes(bits, "little", signed=signed)
            read(across, token, f"--type {scalar}", rf"^{across:#x} {scalar}={value}$")
        read(base + 8, token, "--type f32", rf"^{base + 8:#x} f32=-1\.5$")
        guard = base + 2 * page
        read(guard - 1, token, "--count 2", rf"RPC 7: .*unreadable at {guard:#x}", error=True)
        gap = base + 3 * page
        read(gap, token, "--count 1", rf"RPC 11: .*unmapped at {gap:#x}", error=True)
        fault = base + 5 * page
        read(fault - 1, token, "--count 2", rf"RPC 11: .*target memory.* at {fault:#x}", error=True)
        read(base, token, "--count 65537", r"invalid memory read size", error=True)
        read(2**64 - 2, token, "--count 4", r"RPC 3: invalid or overflowing", error=True)

        change("permissions changed")
        read(guard, token, "--count 1", r"RPC 10: memory mapping snapshot is stale", error=True)
        updated = maps()
        assert updated != token
        read(guard, updated, "--type u8", rf"^{guard:#x} u8=0$")
        change("remapped")
        read(base + page, updated, "--count 1", r"RPC 10: memory mapping snapshot is stale", error=True)
        replacement = maps()
        assert replacement != updated
        read(base + page, replacement, "--type u8", rf"^{base + page:#x} u8=66$")

        send("%detach")
        out.expect(r"^detached session=")
        read(base, replacement, "--count 1", r"error: .*session", error=True)
        send(f"%attach {child.pid}")
        out.expect(rf"^attached session=\S+ pid={child.pid}$")
        read(base, replacement, "--count 1", r"RPC 3: memory mapping token does not identify", error=True)
        fresh = maps()
        read(base + page, fresh, "--type u8", rf"^{base + page:#x} u8=66$")
        assert "TracerPid:\t0" in Path(f"/proc/{child.pid}/status").read_text()
        send("%quit")
        assert console.wait(timeout=15) == 1, "".join(out.seen)
        assert child.poll() is None


if __name__ == "__main__":
    main(*sys.argv[1:])

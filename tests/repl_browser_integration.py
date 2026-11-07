"""Browse local C++ targets without reusing stale process or mapping identities."""

from contextlib import ExitStack
import os
from pathlib import Path
import queue
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time


class Transcript:
    def __init__(self, stream):
        self.lines = queue.Queue()
        self.seen = []

        def read():
            for line in stream:
                self.lines.put(line)
            self.lines.put(None)

        threading.Thread(target=read, daemon=True).start()

    def expect(self, pattern):
        deadline = time.monotonic() + 25
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                line = None
            if line is None:
                raise AssertionError((pattern, "".join(self.seen)))
            self.seen.append(line)
            if match := re.search(pattern, line):
                return match


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def start_time(pid):
    stat = Path(f"/proc/{pid}/stat").read_text()
    return int(stat.rsplit(")", 1)[1].split()[19])


def main(repl, server, target, library):
    with tempfile.TemporaryDirectory(prefix="ct-browser-") as directory, ExitStack() as stack:
        image = Path(directory) / "libbrowse.so"
        shutil.copyfile(library, image)
        targets = []
        for _ in range(2):
            child = subprocess.Popen([target, image], stdout=subprocess.PIPE, text=True)
            stack.callback(stop, child)
            log = Transcript(child.stdout)
            log.expect(r"^loaded$")
            targets.append((child, log))
        selected, target_log = targets[1]
        other, _ = targets[0]
        repl = subprocess.Popen(
            [repl, "--server", server, "--process", target],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        stack.callback(stop, repl)
        output = Transcript(repl.stdout)

        def send(line):
            repl.stdin.write(line + "\n")
            repl.stdin.flush()

        for child in sorted((selected, other), key=lambda process: process.pid):
            output.expect(rf'^{child.pid} start={start_time(child.pid)} exe="[^"]*repl-browser-target"')
        send(str(selected.pid))
        output.expect(rf"^attached session=\S+ pid={selected.pid}$")
        send(f"%attach {other.pid} --start-time {start_time(other.pid) + 1}")
        output.expect(r"target process start time changed")
        send("%threads")
        output.expect(rf'^tid={selected.pid} start={start_time(selected.pid)} name=')
        worker = output.expect(r"^tid=(\d+) start=\d+ name=")
        assert int(worker[1]) not in (selected.pid, other.pid)

        def snapshot():
            send("%modules --details")
            token = output.expect(r"^mapping=(\S+)$")[1]
            mapping = output.expect(r' inode=(\d+) build_id=([a-f0-9]+) "[^"]*/libbrowse\.so"$')
            return token, mapping[1], mapping[2]

        token, inode, build_id = snapshot()
        send("%symbols --module libbrowse.so --filter stack_add --addresses")
        assert output.expect(r"^mapping=(\S+)$")[1] == token
        address = output.expect(r"^stack_add\(int, int\).* address=(0x[0-9a-f]+) elf=0x[0-9a-f]+$")[1]
        send(f"%symbols --address {hex(int(address, 16) + 1)} --mapping {token}")
        output.expect(rf"^stack_add\(int, int\).* address={address} ")

        selected.send_signal(signal.SIGUSR1)
        target_log.expect(r"^unloaded$")
        send(f"%symbols --address {address} --mapping {token}")
        output.expect(r"mapped images changed")

        original_id = bytes.fromhex(build_id)
        changed_id = original_id[:-1] + bytes([original_id[-1] ^ 1])
        data = image.read_bytes()
        assert data.count(original_id) == 1
        replacement = image.with_suffix(".new")
        replacement.write_bytes(data.replace(original_id, changed_id, 1))
        os.replace(replacement, image)
        selected.send_signal(signal.SIGUSR1)
        target_log.expect(r"^loaded$")
        send(f"%symbols --address {address} --mapping {token}")
        output.expect(r"mapped images changed")
        fresh_token, fresh_inode, fresh_id = snapshot()
        assert fresh_token != token and fresh_inode != inode and fresh_id == changed_id.hex()
        send(f"%symbols --module libbrowse.so --filter stack_add --addresses --mapping {fresh_token}")
        output.expect(r"^stack_add\(int, int\).* address=0x[0-9a-f]+")
        send("%quit")
        output.expect(r"^detached session=")
        assert repl.wait(timeout=15) == 1  # Stale selections deliberately fail.
        assert selected.poll() is None and other.poll() is None
        print("Local picker, threads, symbol round trip and stale mapping rejection passed.")


if __name__ == "__main__":
    main(*sys.argv[1:])

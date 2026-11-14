"""Exercise terminal history persistence in the native REPL with a temporary HOME."""
import errno
import os
import pty
import re
import select
import socket
import stat
import subprocess
import sys
import tempfile
import termios
import time
from pathlib import Path

ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
SINGLE = "int history_single = 3;"
FIRST, SECOND = "int history_first = 1;", "int history_second = 2;"


class Terminal:
    def __init__(self, console, endpoint, home, cwd):
        self.master, slave = pty.openpty()
        env = dict(os.environ, TERM="xterm-256color", HOME=str(home))
        env.pop("NO_COLOR", None)
        self.child = subprocess.Popen([console, "--endpoint", endpoint], stdin=slave,
                                      stdout=slave, stderr=slave, env=env, cwd=cwd,
                                      start_new_session=True)
        os.close(slave)
        self.pending = bytearray()

    def until(self, pattern, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            cleaned = ANSI.sub(b"", self.pending).replace(b"\r", b"")
            if re.search(pattern, cleaned, re.DOTALL):
                self.pending.clear()
                return cleaned
            if not select.select([self.master], [], [], 0.1)[0]:
                continue
            try:
                part = os.read(self.master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not part:
                break
            self.pending.extend(part)
            # Replxx asks the terminal for its cursor position.
            if b"\x1b[6n" in self.pending:
                os.write(self.master, b"\x1b[1;1R")
                self.pending[:] = self.pending.replace(b"\x1b[6n", b"")
        raise AssertionError((pattern, bytes(self.pending[-4000:])))

    def send(self, text):
        # Wait for replxx to finish changing terminal modes before fast typeahead.
        deadline = time.monotonic() + 5
        while termios.tcgetattr(self.master)[3] & termios.ICANON:
            assert time.monotonic() < deadline, bytes(self.pending[-4000:])
            time.sleep(0.001)
        os.write(self.master, text.encode())

    def saved(self, path, text, timeout=20):
        """Wait until path holds text, reading output so the editor never blocks."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if path.exists() and text in path.read_bytes():
                return
            if select.select([self.master], [], [], 0.05)[0]:
                try:
                    data = os.read(self.master, 65536)
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
                    break
                if b"\x1b[6n" in data:
                    os.write(self.master, b"\x1b[1;1R")
        raise AssertionError(f"{text!r} never reached {path}")

    def close(self):
        if self.child.poll() is None:
            self.child.kill()
        self.child.wait(timeout=15)
        os.close(self.master)


def main():
    console, fixture = map(os.path.abspath, sys.argv[1:3])
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
    server = subprocess.Popen([fixture, "--listen", endpoint], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    workspace = tempfile.TemporaryDirectory(prefix="tracer-repl-history-")
    work = Path(workspace.name)
    home = work / "home"
    home.mkdir()
    history = home / ".clang-tracer-history"
    terminal = None
    try:
        assert server.stdout.readline().strip() == "ready"

        # Piped input is not interactive, so it must neither read nor write history.
        piped = subprocess.run([console, "--endpoint", endpoint], input="%help\n%quit\n",
                               text=True, capture_output=True, timeout=60, cwd=work,
                               env=dict(os.environ, HOME=str(home)))
        assert piped.returncode == 0, (piped.stdout, piped.stderr)
        assert not history.exists(), "piped session wrote history"

        terminal = Terminal(console, endpoint, home, work)
        terminal.until(rb"tracer> ")
        # Ctrl+J inserts a newline, so this is a single two-line entry.
        terminal.send(f"{FIRST}\n{SECOND}\r")
        terminal.saved(history, SECOND.encode())
        # replxx orders entries by millisecond timestamp, so keep them distinct.
        time.sleep(0.1)
        terminal.send(f"{SINGLE}\r")
        terminal.saved(history, SINGLE.encode())
        # Entries are on disk before exit; a killed session must keep them.
        terminal.close()
        terminal = None

        mode = stat.S_IMODE(history.stat().st_mode)
        assert mode == 0o600, oct(mode)
        saved = history.read_bytes()
        # replxx stores an embedded newline as ETB, so the cell stays one record.
        assert f"{FIRST}\x17{SECOND}".encode() in saved, saved
        assert f"{FIRST}\n".encode() not in saved, saved

        terminal = Terminal(console, endpoint, home, work)
        terminal.until(rb"tracer> ")
        terminal.send("\x1b[A")
        terminal.until(rb"history_single = 3;")
        terminal.send("\x1b[A")
        terminal.until(rb"history_first = 1;.*history_second = 2;")
        terminal.close()
        terminal = None
        assert stat.S_IMODE(history.stat().st_mode) == 0o600
    finally:
        if terminal is not None:
            terminal.close()
        server.terminate()
        server.communicate(timeout=10)
        workspace.cleanup()
    print("native history persistence checks passed")


if __name__ == "__main__":
    main()

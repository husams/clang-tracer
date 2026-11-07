"""Exercise the actual terminal editor, including Tab and bare void calls."""
import errno
import fcntl
import os
import pty
import re
import select
import socket
import struct
import subprocess
import sys
import termios
import time


def main():
    console, fixture = map(os.path.abspath, sys.argv[1:3])
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
    server = subprocess.Popen([fixture, "--listen", endpoint], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    child = None
    master, slave = pty.openpty()
    transcript = bytearray()
    pending = bytearray()
    ansi = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")

    def until(pattern, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            cleaned = ansi.sub(b"", pending).replace(b"\r", b"")
            if re.search(pattern, cleaned, re.DOTALL):
                pending.clear()
                return cleaned
            if not select.select([master], [], [], 0.1)[0]:
                continue
            try:
                part = os.read(master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not part:
                break
            pending.extend(part)
            transcript.extend(part)
            # Replxx asks the terminal for its cursor position.
            if b"\x1b[6n" in pending:
                os.write(master, b"\x1b[1;1R")
                pending[:] = pending.replace(b"\x1b[6n", b"")
        raise AssertionError((pattern, bytes(transcript[-8000:])))

    def send(text):
        # Wait for replxx to finish changing terminal modes before fast typeahead;
        # otherwise the terminal can translate Enter into a literal newline.
        deadline = time.monotonic() + 5
        while termios.tcgetattr(master)[3] & termios.ICANON:
            assert time.monotonic() < deadline, bytes(transcript[-4000:])
            time.sleep(0.001)
        os.write(master, text.encode())

    try:
        assert server.stdout.readline().strip() == "ready"
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 160, 0, 0))
        env = dict(os.environ, TERM="xterm-256color")
        env.pop("NO_COLOR", None)
        child = subprocess.Popen([console, "--endpoint", endpoint],
                                 stdin=slave, stdout=slave, stderr=slave,
                                 env=env, start_new_session=True)
        os.close(slave)
        slave = -1
        until(rb"tracer> ")
        send("%attach 7\r")
        until(rb"attached session=s1.*tracer\[7\]> ")
        send("%collect --count 4 --as batch\r")
        until(rb"received=4.*tracer\[7\]> ")
        send('auto data = tracer.batch("batch"); std::cout << "terminal-ready" << std::endl;\r')
        until(rb"(?m)^terminal-ready\n.*tracer\[7\]> ")
        # This is the exact user-reported spelling, with no trailing semicolon.
        send("tracer.print(data.at(0))\r")
        until(rb"phase: PRE.*tracer\[7\]> ")
        send("tracer.print(data)\r")
        until(rb"Collection retained=4 received=4.*tracer\[7\]> ")
        send("tracer.bat\t")
        until(rb"tracer\.batch")
        send("\x15tracer.events_a\t")
        until(rb"tracer\.events_async")
        send("\x15data.rec\t")
        until(rb"data\.received")
        send("\x15data.at(0).desc\t")
        until(rb"data\.at\(0\)\.describe")
        send("\x15%help ap\t")
        until(rb"%help api")
        send("\r")
        until(rb"Range pipelines.*tracer\[7\]> ")
        send("data.at(0).argument_snapshot(0).cpp_t\t")
        until(rb"argument_snapshot\(0\)\.cpp_type")
        send("\x15%help api EventV\t\r")
        until(rb"auto order = e.argument_snapshot\(0\);.*elements\(\) const;.*tracer\[7\]> ")
        assert b'\x1b[1;36mauto\x1b[0m order' in transcript
        assert b'\x1b[32m"id"\x1b[0m' in transcript
        send("%help probe inj\t\r")
        until(rb"Deployment options.*tracer\[7\]> ")
        assert b'\x1b[33m--callback\x1b[0m' in transcript
        # Enter must keep an unfinished block in the editor; Ctrl+C discards it.
        send("for (const auto& e : data.events) {\r")
        send("tracer.print(e);\r")
        send("\x03")
        until(rb"tracer\[7\]> ")
        send('std::cout << "cancel-kept-" << data.received << std::endl;\r')
        until(rb"(?m)^cancel-kept-4\n.*tracer\[7\]> ")
        send("for (const auto& e : data.events) {\rtracer.print(e);\r}\r")
        until(rb"phase: PRE.*phase: POST.*tracer\[7\]> ")
        send("data.at(0).argument<bool>(0)\r")
        until(rb"error:.*tracer\[7\]> ")
        send("data.rec\t")
        until(rb"data\.received")
        send("\x15tracer.print(data.at(0))\r")
        until(rb"phase: PRE.*tracer\[7\]> ")
        send("%quit\r")
        until(rb"detached session=s1")
        child.wait(timeout=15)
        assert child.returncode == 0, bytes(transcript[-8000:])
        assert server.poll() is None, "editor stopped an external service"
    finally:
        if child is not None and child.poll() is None:
            child.terminate()
            child.wait(timeout=15)
        os.close(master)
        if slave >= 0:
            os.close(slave)
        server.terminate()
        server.communicate(timeout=10)
    print("native terminal print/completion checks passed")


if __name__ == "__main__":
    main()

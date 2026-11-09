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
import tempfile
import termios
import time
from pathlib import Path


def main():
    console, fixture = map(os.path.abspath, sys.argv[1:3])
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
    server = subprocess.Popen([fixture, "--listen", endpoint], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    workspace = tempfile.TemporaryDirectory(prefix="tracer-repl-editor-")
    work = Path(workspace.name)
    headers = work / "editör headers"
    headers.mkdir()
    changed_header = work / "changed.hpp"
    changed_header.write_text("#pragma once\n")
    (headers / "editor-test.hpp").write_text("constexpr int terminal_header_value = 27;\n")
    (work / "file with spaces.cpp").write_text('std::cout << "snippet-loaded" << std::endl;\n')
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
                                 env=env, cwd=work, start_new_session=True)
        os.close(slave)
        slave = -1
        until(rb"tracer> ")
        send("%attach 7\r")
        until(rb"attached session=s1.*tracer\[7\]> ")
        send("%collect")
        until(rb"\[Tab: insert template\]")
        send("\t")
        until(rb"%collect --count count --as name")
        # A typed space can match the separator already after the count.
        send("4 \tbatch\t\r")
        until(rb"received=4.*tracer\[7\]> ")

        start = len(transcript)
        send("#incl\t")
        until(rb"#include <header>")
        assert b"\x1b[0;94m#include" in transcript[start:]
        send("cstddef\t\r")
        until(rb"\ntracer\[7\]> ")
        send('  #include "edi\t')
        until(rb'  #include "edit\xc3\xb6r headers/"')
        send('editor-t\t"\r')
        until(rb"\ntracer\[7\]> ")
        send("concep\t")
        until(rb"concept")
        assert b"\x1b[0;96mconcept" in transcript
        send("\x15if\t")
        until(rb"if \(condition\) \{")
        send('terminal_header_value == 28\t\x1b[Zterminal_header_value == 27\t'
             'std::cout << "editor {ready}" << std::endl;\t\r')
        until(rb"(?m)^editor \{ready\}\n.*tracer\[7\]> ")

        send("for\t")
        until(rb"for \(init; condition; step\) \{")
        # Typing and deleting a duplicate template separator must keep the fields.
        send("int i = 0;\x7f\t")
        until(rb"for \(int i = 0; condition; step\) \{")
        send("i < 2\t++i\t")
        until(rb"for \(int i = 0; i < 2; \+\+i\) \{")
        send('std::cout << "loop-" << i << std::endl;\t\r')
        until(rb"(?m)^loop-0\nloop-1\n.*tracer\[7\]> ")

        send('#include "changed.hpp"\r')
        until(rb"\ntracer\[7\]> ")
        original_header = changed_header.read_bytes()
        stamp = changed_header.stat()
        changed_header.write_bytes(original_header + b"// changed during editing\n")
        send("for (int x = 1; x < 2; ++x) {\rstd::cou\t")
        until(rb"C\+\+ completion: C\+\+ header changed: [^\n]*changed.hpp;[^\n]*\n"
              rb"tracer\[7\]> \n?for \(int x = 1; x < 2; \+\+x\) \{\n +std::cou")
        assert b"\x1b[JC++ completion:" in transcript
        changed_header.write_bytes(original_header)
        os.utime(changed_header, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        send('t << "completion-input-kept" << std::endl;\r}\r')
        until(rb"(?m)^completion-input-kept\n.*tracer\[7\]> ")

        send("%probe inject \t")
        until(rb'%probe inject "selector"')
        send("demo::compute(int)\t")
        until(rb"\[optional: --callback FILE or --prepared DIRECTORY\]")
        send("--prepared prepared\t")
        until(rb'%probe inject "demo::compute\(int\)" --prepared prepared')
        send("\x03")
        until(rb"\ntracer\[7\]> ")

        send("%probe inject --callback\t")
        until(rb'%probe inject "selector" --callback "file"')
        send("demo::compute(int)\thook.cpp\t")
        until(rb'%probe inject "demo::compute\(int\)" --callback "hook.cpp"')
        send("\x03")
        until(rb"\ntracer\[7\]> ")

        send("%probe prepare\t")
        until(rb'%probe prepare "selector" --include "header" --output "directory"')
        send("demo::compute(int)\tdemo.hpp\tprepared\t")
        until(rb'%probe prepare "demo::compute\(int\)" --include "demo.hpp" --output "prepared"')
        send("\x03")
        until(rb"\ntracer\[7\]> ")

        send("%cpp load\t")
        until(rb'%cpp load "file"')
        send("file w\x1b/\t\r")
        until(rb"(?m)^snippet-loaded\n.*tracer\[7\]> ")

        send('%probe prepare "demo()" --cwd edi\t')
        until(rb'--cwd "edit\xc3\xb6r headers/')
        send("\x15%collect --phase PO\t")
        until(rb"--phase POST")
        send("\x15%events  ex\t")
        until(rb"%events export")
        send("\x15%shell foo|bar")
        until(rb"%shell foo\|bar")
        assert b"\x1b[0;22;35m|" in transcript
        send("\x15")
        start = len(transcript)
        send("%symbols --lim\t9")
        until(rb"%symbols --limit 9")
        command = bytes(transcript[start:])
        assert b"\x1b[0;96m%symbols" in command, command
        assert b"\x1b[0;22;33m--limit" in command, command
        assert b"\x1b[0;22;32m9" in command, command
        start = len(transcript)
        send('\x15const auto shade = "café";')
        until(rb'const auto shade = "caf\xc3\xa9";')
        code = bytes(transcript[start:])
        assert b"\x1b[0;96mconst" in code, code
        assert b"\x1b[0;22;35m=" in code, code
        assert b'\x1b[0;22;32m"caf\xc3\xa9"' in code, code
        send("\x15%type session.pid\r")
        until(rb"(?m)^.* pid;\n.*tracer\[7\]> ")
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
        send("%help api drop_w\t\r")
        until(rb"(?m)^drop_while\n.*first false result.*tracer\[7\]> ")
        send("%help probe inj\t\r")
        until(rb"Deployment options.*tracer\[7\]> ")
        assert b'\x1b[33m--callback\x1b[0m' in transcript
        # Enter must keep an unfinished block in the editor; Ctrl+C discards it.
        send("for (const auto& e : data.events) {\r")
        until(rb"\{\n +\n *\}")
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
        workspace.cleanup()
    print("native terminal print/completion checks passed")


if __name__ == "__main__":
    main()

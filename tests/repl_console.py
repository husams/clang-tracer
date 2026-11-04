"""Executable-level native console checks; no Python tracing dependency."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    console, fixture = map(os.path.abspath, sys.argv[1:3])
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
    server = subprocess.Popen([fixture, "--listen", endpoint], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert server.stdout.readline().strip() == "ready"
        with tempfile.TemporaryDirectory(prefix="tracer-repl-console-") as directory:
            root = Path(directory)
            callback = root / "client only callback.cpp"
            callback.write_text("// client-only interceptor\n")
            capture = root / "capture.pb"
            export = root / "events.json"
            commands = f'''%attach 7
%interceptor load "{callback}" --as hook
%interceptor upload hook --target "multiply(int, int)"
%collect --count 4 --as batch
%events show batch --index 1
%events calls batch
auto data = tracer.batch("batch");
std::cout << "event-print-start" << std::endl;
tracer.print(data.at(1));
std::cout << "event-print-end" << std::endl;
std::cout << "exact-print-start" << std::endl;
tracer.print(data.at(0))
std::cout << "exact-print-end" << std::endl;
std::cout << "cpp-size=" << data.size() << " result=" << data.at(1).result() << std::endl;
int typed_sum = data.at(0).argument_as<int>(0) + data.at(1).result_as<int>();
std::cout << "typed-sum=" << typed_sum << std::endl;
try {{ data.at(1).result_as<bool>(); }} catch (const std::invalid_argument&) {{ std::cout << "typed-mismatch-rejected" << std::endl; }}
auto captured = session;
auto another = tracer.collect(captured, 4);
std::cout << "collected-cpp=" << another.size() << std::endl;
%cpp reset
std::cout << "retained-session=" << session.pid << std::endl;
%record "{capture}" --count 4
%events open "{capture}" --as saved
%events export saved --format json --output "{export}"
%events stats saved
tracer.detach(session)
%quit
'''
            run = subprocess.run([console, "--endpoint", endpoint, "--fail-fast"], input=commands, text=True, capture_output=True, timeout=45)
            assert run.returncode == 0, (run.stdout, run.stderr)
            for expected in ["installed probe=5", "cpp-size=4 result=42", "typed-sum=63", "typed-mismatch-rejected", "collected-cpp=4", "retained-session=7", "duration_ns=10", "paired=2", "detached session=s1"]:
                assert expected in run.stdout, (expected, run.stdout, run.stderr)
            assert len(json.loads(export.read_text())) == 4
            printed = run.stdout.split("event-print-start\n", 1)[1].split("event-print-end", 1)[0]
            assert "phase: POST" in printed and "signed_value: 42" in printed, printed
            printed = run.stdout.split("exact-print-start\n", 1)[1].split("exact-print-end", 1)[0]
            assert "phase: PRE" in printed and "signed_value: 21" in printed, printed
            discovery = subprocess.run([console, "--fail-fast"], input='''%help api
%help Event
%complete "tracer.pr"
tracer_repl::Collection empty;
%complete "empty.rec"
tracer.print(empty)
%status
%quit
''', text=True, capture_output=True, timeout=20)
            assert discovery.returncode == 0, (discovery.stdout, discovery.stderr)
            for expected in ("T result_as() const", "std::uint64_t received", "  print", "  received", "Collection retained=0", "ready=unconnected"):
                assert expected in discovery.stdout, (expected, discovery.stdout, discovery.stderr)
            # Disk recording must remain available when named memory collections are full.
            disk_only = root / "full-buffer-recording.pb"
            full = subprocess.run([console, "--endpoint", endpoint, "--buffer-events", "4", "--fail-fast"],
                input=f'%attach 7\n%collect --count 4 --as full\n%record "{disk_only}" --count 4\n%quit\n',
                text=True, capture_output=True, timeout=15)
            assert full.returncode == 0 and disk_only.stat().st_size > 0, (full.stdout, full.stderr)
            # A partial C++ input must leave commands responsive.
            rejected = subprocess.run([console], input='''int kept = 42;
int broken = ;
for (int e : {1,2}) { int unfinished = 0;
std::cout << "recovered=" << kept << std::endl;
%complete "kep"
%status
%quit
''', text=True, capture_output=True, timeout=20)
            assert rejected.returncode == 1 and "ready=unconnected" in rejected.stdout
            assert "recovered=42" in rejected.stdout and "  kept" in rejected.stdout, (rejected.stdout, rejected.stderr)
            assert rejected.stderr.count("error:") == 2 and "%cpp reset" not in rejected.stderr, rejected.stderr
            # Explicit unreachable endpoint never starts a local replacement.
            unavailable = subprocess.run([console, "--endpoint", "unix:///nonexistent/tracer.sock", "--fail-fast"], input="%sessions\n", text=True, capture_output=True, timeout=10)
            assert unavailable.returncode == 1 and "server ready" not in unavailable.stdout
            # SIGINT cancels a truly blocked stream, then permits another command.
            waiting = subprocess.Popen([console, "--endpoint", endpoint], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            waiting.stdin.write("%attach 7\n%collect --after 900 --as stopped\n")
            waiting.stdin.flush()
            while "attached session=" not in waiting.stdout.readline():
                assert waiting.poll() is None
            time.sleep(0.2)
            waiting.send_signal(signal.SIGINT)
            waiting.stdin.write("%status\n%quit\n")
            waiting.stdin.flush()
            stdout, stderr = waiting.communicate(timeout=10)
            assert waiting.returncode == 0, (stdout, stderr)
            assert "cancelled=true" in stdout and "ready=true" in stdout, (stdout, stderr)
            assert server.poll() is None, "console stopped an external service"
    finally:
        server.terminate()
        server.communicate(timeout=10)
    print("native console executable checks passed")


if __name__ == "__main__":
    main()

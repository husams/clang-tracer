"""Real Linux attach/deploy/collect/restore path for the native C++17 console."""
import argparse
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "target", "callback"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--bool-target")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="tracer-native-repl-") as directory:
        root = Path(directory)
        with (root / "target.log").open("w+") as target_log:
            target = subprocess.Popen([args.target], stdout=target_log, stderr=subprocess.STDOUT)
            console = None
            boolean_target = None
            orphan_server_pid = None
            try:
                source = root / "local callback.cpp"
                source.write_text(Path(args.callback).read_text())
                console = subprocess.Popen(
                    [args.repl, "--server", args.server, "--attach", str(target.pid), "--fail-fast"],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                transcript = []

                def until(pattern):
                    while True:
                        line = console.stdout.readline()
                        if not line:
                            stderr = console.stderr.read()
                            raise AssertionError((pattern, "".join(transcript), stderr))
                        transcript.append(line)
                        match = re.search(pattern, line)
                        if match:
                            return match

                until(r"attached session=")
                console.stdin.write(f'''%server status
%symbols --filter Calculator --limit 10
%interceptor load "{source}" --as hook
%interceptor upload hook --target "Calculator::multiply(int, int) const" --stack-depth 8
''')
                console.stdin.flush()
                probe = until(r"installed probe=(\d+)").group(1)
                recording = root / "capture.pb"
                console.stdin.write(f'''%collect --count 6 --timeout 10 --as batch
%events list batch --limit 2
%events show batch --index 1
%events calls batch
auto captured = tracer.batch("batch");
std::cout << "native-bare-print-start" << std::endl;
tracer.print(captured.at(0))
std::cout << "native-bare-print-end" << std::endl;
%help Event
%complete "captured.at(0).res"
auto saved_session = session;
std::cout << "native-cpp-size=" << captured.size() << std::endl;
int numeric_checks = 0;
for (const auto& e : captured) {{ if (e.has_result() && e.argument_as<int>(0) > 0 && e.argument_as<int>(1) > 0 && e.result_as<int>() > 0) ++numeric_checks; }}
std::cout << "native-numeric-checks=" << numeric_checks << std::endl;
%record "{recording}" --count 6 --timeout 10
%events open "{recording}" --as saved
%events stats saved
%probe remove {probe}
%detach
%server stop
try {{ tracer.detach(saved_session); std::cout << "stale-detach-accepted" << std::endl; }} catch (const std::exception&) {{ std::cout << "stale-detach-rejected" << std::endl; }}
%quit
''')
                console.stdin.flush()
                stdout, stderr = console.communicate(timeout=75)
                text = "".join(transcript) + stdout
                assert console.returncode == 0, (text, stderr)
                for expected in ("ownership=this-console", "Calculator::multiply", "received=6", "native-cpp-size=6", "native-numeric-checks=3", "object_fields {", 'name: "bias"', "stack {", "phase: POST", "result {", "restored=true", "detached session=", "owned server stopped after restoration", "stale-detach-rejected"):
                    assert expected in text, (expected, text, stderr)
                assert "stale-detach-accepted" not in text
                bare_print = text.split("native-bare-print-start\n", 1)[1].split("native-bare-print-end", 1)[0]
                assert "phase: PRE" in bare_print and "object_fields {" in bare_print, bare_print
                assert "T result_as() const" in text and "  result_as\n" in text, text
                assert target.poll() is None, "tracing terminated the application"
                assert recording.stat().st_size > 0
                # Name attach exercises the integrated process lookup and quit cleanup.
                selected = subprocess.run(
                    [args.repl, "--server", args.server, "--process", os.path.realpath(args.target), "--fail-fast"],
                    input="%status\n%quit\n", text=True, capture_output=True, timeout=45)
                assert selected.returncode == 0, (selected.stdout, selected.stderr)
                assert f"pid={target.pid}" in selected.stdout and "detached session=" in selected.stdout
                # An abruptly terminated console must not leave a detached-session
                # server alive with hooks installed in the application.
                console = subprocess.Popen(
                    [args.repl, "--server", args.server, "--attach", str(target.pid), "--fail-fast"],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                transcript = []
                orphan_server_pid = int(until(r"ownership=this-console pid=(\d+)").group(1))
                until(r"attached session=")
                console.stdin.write(f'''%interceptor load "{source}" --as crash_hook
%interceptor upload crash_hook --target "Calculator::multiply(int, int) const"
''')
                console.stdin.flush()
                until(r"installed probe=\d+")
                console.terminate()
                console.communicate(timeout=10)

                def server_alive():
                    try:
                        stat = Path(f"/proc/{orphan_server_pid}/stat").read_text()
                        return stat.rsplit(")", 1)[1].split()[0] != "Z"
                    except FileNotFoundError:
                        return False

                deadline = time.monotonic() + 30
                while server_alive() and time.monotonic() < deadline:
                    time.sleep(0.05)
                assert not server_alive(), "owned server survived console termination"
                orphan_server_pid = None
                assert target.poll() is None, "parent-death cleanup terminated the application"
                status = Path(f"/proc/{target.pid}/status").read_text()
                assert re.search(r"^TracerPid:\s+0$", status, re.MULTILINE), status
                # Reattaching and deploying proves the former hooks were restored.
                restored = subprocess.run(
                    [args.repl, "--server", args.server, "--attach", str(target.pid), "--fail-fast"],
                    input=f'%probe inject "Calculator::multiply(int, int) const" --callback "{source}"\n%collect --count 2 --timeout 10\n%quit\n',
                    text=True, capture_output=True, timeout=60)
                assert restored.returncode == 0, (restored.stdout, restored.stderr)
                assert "received=2" in restored.stdout, restored.stdout
                if args.bool_target:
                    boolean_source = root / "boolean.cpp"
                    boolean_source.write_text('class Interceptor { public: void pre(const Call& c) { c.argument<bool>(0); } void post(const Call& c) { c.argument<bool>(0); } };\n')
                    boolean_target = subprocess.Popen([args.bool_target], stdout=target_log, stderr=subprocess.STDOUT)
                    boolean_recording = root / "booleans.pb"
                    commands = f'''%probe inject "signature_fixture::boolean_target(bool)" --callback "{boolean_source}"
%collect --count 4 --timeout 10 --phase POST --as bools
auto booleans = tracer.batch("bools");
bool typed_pass = booleans.size() == 4;
for (const auto& e : booleans) {{ bool a = e.argument_as<bool>(0); bool r = e.result_as<bool>(); typed_pass = typed_pass && a && !r; }}
std::cout << "native-bool-checks=" << std::boolalpha << typed_pass << std::endl;
%record "{boolean_recording}" --count 4 --timeout 10 --phase POST
%events open "{boolean_recording}" --as bool_replay
bool replay_result = tracer.batch("bool_replay").at(0).result_as<bool>();
bool replay_argument = std::get<bool>(tracer.batch("bool_replay").at(0).argument_value(0));
std::cout << "replay-bool-result=" << replay_result << " replay-bool-argument=" << replay_argument << std::endl;
%quit
'''
                    verified = subprocess.run(
                        [args.repl, "--server", args.server, "--attach", str(boolean_target.pid), "--fail-fast"],
                        input=commands, text=True, capture_output=True, timeout=75)
                    assert verified.returncode == 0, (verified.stdout, verified.stderr)
                    assert "native-bool-checks=true" in verified.stdout, verified.stdout
                    assert "replay-bool-result=false replay-bool-argument=true" in verified.stdout, verified.stdout
                    assert "detached session=" in verified.stdout, verified.stdout
                    assert boolean_target.poll() is None, "boolean tracing terminated the application"
                    print(verified.stdout)
                print(text)
                print("console termination restored hooks and stopped its owned server")
                print("native Linux REPL integration passed")
            finally:
                if console is not None and console.poll() is None:
                    console.send_signal(signal.SIGINT)
                    try:
                        console.stdin.write("%quit\n")
                        console.stdin.flush()
                    except (BrokenPipeError, ValueError):
                        pass
                    console.communicate(timeout=45)
                if orphan_server_pid is not None:
                    try:
                        os.kill(orphan_server_pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                if boolean_target is not None and boolean_target.poll() is None:
                    boolean_target.terminate()
                    boolean_target.wait(timeout=10)
                target.terminate()
                target.wait(timeout=10)


if __name__ == "__main__":
    main()
